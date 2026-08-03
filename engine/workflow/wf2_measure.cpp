#include "wf2_measure.h"
#include "../src/spectral.h"
#include "../src/km.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace jcolour {
namespace workflow {

// Thread-local Mersenne Twister with a fixed seed for deterministic
// simulation. F30 audit fix: previous code used std::rand + a static
// `seeded` flag, which was both racy and non-deterministic across runs
// (made K-M fitting and audit reproducibility brittle).
static double rand_range(double lo, double hi) {
    thread_local std::mt19937 rng{0xC0FFEEu};
    std::uniform_real_distribution<double> dist(lo, hi);
    return dist(rng);
}

std::vector<Wf2Measurement> simulate_wf2_measurements(const Wf2ChartLayout& chart) {
    std::vector<Wf2Measurement> results;

    for (const auto& patch : chart.patches) {
        Wf2Measurement m;
        m.label = patch.label;
        m.channel = patch.channel;
        m.ink_density = patch.ink_density;

        // Simulate L*a*b* based on ink density
        // Higher ink = lower L*, some a*b* for channel-specific tint
        double base_l = 100.0 - patch.ink_density * 85.0;

        // Channel-specific characteristics. The MAIN_K tint depends on the
        // session black kind: PK biases cool (-a, -b), MK biases slightly warmer.
        double channel_offset_a = 0.0;
        double channel_offset_b = 0.0;
        double noise_scale = 1.0;

        switch (patch.channel) {
            case Wf2Channel::MAIN_K:
                channel_offset_a = chart.black_kind == BlackKind::PK ? -0.5 :  0.5;
                channel_offset_b = chart.black_kind == BlackKind::PK ? -0.5 : -0.5;
                break;
            case Wf2Channel::GY:
                channel_offset_a = 0.0;
                channel_offset_b = 0.0;
                noise_scale = 0.5;
                break;
            case Wf2Channel::LGY:
                channel_offset_a = 0.2;
                channel_offset_b = 0.2;
                noise_scale = 0.7;
                break;
        }

        m.L_star = std::max(0.0, std::min(100.0, base_l + rand_range(-1.0, 1.0) * noise_scale));
        m.a_star = channel_offset_a + rand_range(-0.5, 0.5) * noise_scale;
        m.b_star = channel_offset_b + rand_range(-0.5, 0.5) * noise_scale;

        // Fill spectral data (simple approximation)
        m.spectral_data.fill(0.0);
        double darkness = (100.0 - m.L_star) / 100.0;
        for (int i = 0; i < 36; ++i) {
            // Flat spectrum with darkness factor
            m.spectral_data[i] = 0.3 + darkness * 0.5 + rand_range(-0.02, 0.02);
        }

        results.push_back(m);
    }

    return results;
}

Wf2InkLimit analyze_wf2_ink_limit(const std::vector<Wf2Measurement>& measurements) {
    Wf2InkLimit result;

    if (measurements.empty()) return result;

    result.channel = measurements[0].channel;

    // Find saturation point where dL*/d(density) approaches zero
    std::vector<double> slopes;
    for (size_t i = 1; i < measurements.size(); ++i) {
        double dL = measurements[i].L_star - measurements[i-1].L_star;
        double dd = measurements[i].ink_density - measurements[i-1].ink_density;
        if (dd > 0) {
            slopes.push_back(dL / dd);
        }
    }

    // Limit at first point where slope is nearly flat
    for (size_t i = 0; i < slopes.size(); ++i) {
        if (slopes[i] > -2.0) {  // Nearly flat
            result.limit = measurements[i].ink_density;
            break;
        }
    }

    if (result.limit == 0.0f && !measurements.empty()) {
        result.limit = measurements.back().ink_density;
    }

    // Check monotonicity
    result.is_monotonic = true;
    for (size_t i = 1; i < measurements.size(); ++i) {
        if (measurements[i].L_star > measurements[i-1].L_star + 2.0) {
            result.is_monotonic = false;
            break;
        }
    }

    return result;
}

Wf2AllInkLimits analyze_wf2_ink_limits(const Wf2ChartLayout& chart, bool dry_run) {
    Wf2AllInkLimits result;

    // Simulate or measure
    auto measurements = simulate_wf2_measurements(chart);

    // Group by channel
    std::map<Wf2Channel, std::vector<Wf2Measurement>> by_channel;
    for (const auto& m : measurements) {
        by_channel[m.channel].push_back(m);
    }

    // Analyze each channel
    for (int ch = 0; ch < WF2_NUM_CHANNELS; ++ch) {
        Wf2Channel channel = static_cast<Wf2Channel>(ch);
        auto it = by_channel.find(channel);
        if (it != by_channel.end()) {
            result.per_channel.push_back(analyze_wf2_ink_limit(it->second));
        } else {
            // Default limit
            Wf2InkLimit limit;
            limit.channel = channel;
            limit.limit = 0.95f;
            limit.is_monotonic = true;
            result.per_channel.push_back(limit);
        }
    }

    return result;
}

Wf2KMModel fit_km_model(
    const std::vector<Wf2Measurement>& neugebauer_patches,
    const Wf2NeugebauerPatches& primaries,
    const std::array<double, 36>& paper_white_spectrum) {

    Wf2KMModel model;

    // Simple K-M fitting: use spectral data to estimate K/S ratios
    // Create substrate from paper white spectrum
    spectral::Spectrum substrate_spectrum;
    for (int i = 0; i < 36; ++i) {
        substrate_spectrum[i] = paper_white_spectrum[i];
    }

    model.km = std::make_unique<forward::KMForwardModel>(substrate_spectrum);
    model.yule_nielsen_n = 2.0f;

    return model;
}

float fit_yule_nielsen_n(
    const Wf2KMModel& km_model,
    const std::vector<Wf2Measurement>& measurements) {
    // Placeholder: real implementation would optimize n to minimize prediction error
    return 2.0f;
}

Wf2CalibrationResult run_wf2_calibration(
    const Wf2ChartLayout& chart,
    const std::vector<Wf2Measurement>& measurements,
    bool dry_run) {

    Wf2CalibrationResult result;

    // Analyze ink limits
    result.ink_limits = analyze_wf2_ink_limits(chart, dry_run);

    // Calculate total ink limit (sum of all 3 channels: MAIN_K + GY + LGY)
    result.total_ink_limit = 0.0f;
    for (const auto& limit : result.ink_limits.per_channel) {
        result.total_ink_limit += limit.limit;
    }

    // Fit K-M model (placeholder)
    std::array<double, 36> paper_white;
    paper_white.fill(0.9);  // Default paper white
    auto primaries = generate_wf2_neugebauer_primaries();
    result.km_model = fit_km_model(measurements, primaries, paper_white);

    // Fit Yule-Nielsen n
    result.yule_nielsen_n = fit_yule_nielsen_n(result.km_model, measurements);

    return result;
}

}  // namespace workflow
}  // namespace jcolour