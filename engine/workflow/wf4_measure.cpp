#include "wf4_measure.h"
#include "../src/spectral.h"
#include "../src/delta_e.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace jcolour {
namespace workflow {

// Thread-local deterministic RNG. F30 audit fix: was std::rand with a
// time(0) seed and a shared `seeded` flag — non-deterministic and not
// thread-safe.
static double rand_range(double lo, double hi) {
    thread_local std::mt19937 rng{0xDEADBEEFu};
    std::uniform_real_distribution<double> dist(lo, hi);
    return dist(rng);
}

std::vector<WF4Measurement> simulate_wf4_measurements(const WF4ChartLayout& chart) {
    std::vector<WF4Measurement> results;

    // Single ink patches: simulate Kubelka-Munk-like response
    for (const auto& patch : chart.single_ink_patches) {
        WF4Measurement m;
        m.label = patch.label;
        m.channel = patch.channel;
        m.ink_density = patch.ink_density;

        // Simple CMF-based L* estimation
        // Higher ink density -> lower L* (darker)
        double base_l = 100.0 - patch.ink_density * 90.0;
        // Add some channel-specific offset
        double channel_offset = static_cast<int>(patch.channel) * 0.5;
        double noise = rand_range(-1.0, 1.0);
        m.L_star = std::max(0.0, std::min(100.0, base_l - channel_offset + noise));
        m.a_star = rand_range(-1.0, 1.0);
        m.b_star = rand_range(-1.0, 1.0);

        // Fill spectral data placeholder
        m.spectral_data.resize(36);
        for (int i = 0; i < 36; ++i) {
            m.spectral_data[i] = (100.0 - m.L_star) / 100.0 * 0.5 + 0.3;
        }

        results.push_back(m);
    }

    // TIL patches: simulate based on total ink loading. `composition`
    // preserves the full channel-density list so analyze_til can later use
    // it to find which combinations cause coalescence (audit F10/F23).
    for (const auto& patch : chart.til_patches) {
        WF4Measurement m;
        m.label = patch.label;
        m.channel = InkChannel::MAIN_K;  // sentinel — see `composition` for real data
        m.ink_density = 0.0f;
        m.composition = patch.channels;

        // Calculate total ink loading
        float total_ink = 0.0f;
        for (const auto& ch : patch.channels) {
            total_ink += ch.second;
        }

        // Total ink affects darkness
        double base_l = 100.0 - total_ink * 70.0;
        m.L_star = std::max(0.0, std::min(100.0, base_l + rand_range(-1.0, 1.0)));
        m.a_star = rand_range(-2.0, 2.0);
        m.b_star = rand_range(-2.0, 2.0);

        m.spectral_data.resize(36);
        for (int i = 0; i < 36; ++i) {
            m.spectral_data[i] = (100.0 - m.L_star) / 100.0 * 0.5 + 0.3;
        }

        results.push_back(m);
    }

    return results;
}

WF4InkLimit analyze_ink_limit(const std::vector<WF4Measurement>& channel_measurements) {
    WF4InkLimit result;
    if (channel_measurements.empty()) return result;

    result.channel = channel_measurements[0].channel;

    // Find saturation point: where dL*/d(density) drops below threshold
    // Use finite differences
    std::vector<double> slopes;
    for (size_t i = 1; i < channel_measurements.size(); ++i) {
        double dL = channel_measurements[i].L_star - channel_measurements[i-1].L_star;
        double dd = channel_measurements[i].ink_density - channel_measurements[i-1].ink_density;
        if (dd > 0) {
            slopes.push_back(dL / dd);
        }
    }

    // Limit is at first point where slope approaches zero
    for (size_t i = 0; i < slopes.size(); ++i) {
        if (slopes[i] > -2.0) {  // Nearly flat
            result.limit = channel_measurements[i].ink_density;
            break;
        }
    }

    if (result.limit == 0.0f && !channel_measurements.empty()) {
        result.limit = channel_measurements.back().ink_density;
    }

    // Check monotonicity with tolerance for simulation noise
    result.is_monotonic = true;
    for (size_t i = 1; i < channel_measurements.size(); ++i) {
        // Allow small positive excursions due to noise
        if (channel_measurements[i].L_star > channel_measurements[i-1].L_star + 2.0) {
            result.is_monotonic = false;
            break;
        }
    }

    return result;
}

WF4TILResult analyze_til(const std::vector<WF4Measurement>& til_measurements) {
    WF4TILResult result;

    // F10 audit fix: TIL analysis now uses the preserved patch composition.
    // For each TIL patch we compute total ink load by summing composition
    // densities. We then look for the highest total-ink loading that still
    // produces a measurable density increase — beyond that point, additional
    // ink causes coalescence (paper saturates, L* stops dropping).
    //
    // til_measurements should be ordered by ascending total ink load. If the
    // caller doesn't sort them, we do it here.
    struct Sample {
        float total_ink;
        double L_star;
        const std::vector<std::pair<InkChannel, float>>* composition;
    };
    std::vector<Sample> samples;
    samples.reserve(til_measurements.size());
    for (const auto& m : til_measurements) {
        float total = 0.0f;
        for (const auto& [ch, dens] : m.composition) {
            (void)ch;
            total += dens;
        }
        if (total > 0.0f) {
            samples.push_back({total, m.L_star, &m.composition});
        }
    }
    std::sort(samples.begin(), samples.end(),
              [](const Sample& a, const Sample& b) { return a.total_ink < b.total_ink; });

    // Walk ascending: TIL = last total_ink at which adding ink still made
    // the patch noticeably darker (ΔL* < -0.5 from previous step).
    float candidate = 0.0f;
    for (size_t i = 1; i < samples.size(); ++i) {
        double dL = samples[i].L_star - samples[i - 1].L_star;
        if (dL < -0.5) {
            candidate = samples[i].total_ink;
        } else {
            // First flat or rising step → previous candidate is the limit.
            break;
        }
    }

    // Record the strongest meaningful sample's composition so callers can
    // see which channel combinations were probed.
    if (!samples.empty()) {
        const auto& strongest = samples.back();
        for (const auto& [ch, dens] : *strongest.composition) {
            result.individual_limits[ch] =
                std::max(result.individual_limits[ch], dens);
        }
    }

    // Reasonable default if analysis didn't converge on a value: 3.3
    // (300%) is appropriate for 11-channel PRO12 on most photo papers.
    // This is conservative — matte and canvas may want lower; some
    // baryta papers may tolerate up to 3.6.
    result.til = (candidate > 0.0f) ? candidate : 3.3f;
    return result;
}

// Shared analysis core — agnostic of how measurements were obtained.
static WF4AllInkLimits analyze_wf4_ink_limits_core(
    const WF4ChartLayout& chart,
    const std::vector<WF4Measurement>& measurements) {
    WF4AllInkLimits result;

    // Group measurements by channel
    std::map<InkChannel, std::vector<WF4Measurement>> by_channel;
    for (const auto& m : measurements) {
        by_channel[m.channel].push_back(m);
    }

    // Analyze each channel
    for (int ch = 0; ch < NUM_INK_CHANNELS; ++ch) {
        InkChannel channel = static_cast<InkChannel>(ch);
        auto it = by_channel.find(channel);
        if (it != by_channel.end()) {
            result.per_channel.push_back(analyze_ink_limit(it->second));
        } else {
            // Generate default limit
            WF4InkLimit limit;
            limit.channel = channel;
            limit.limit = 0.95f;  // Default
            result.per_channel.push_back(limit);
        }
    }

    // Analyze TIL
    std::vector<WF4Measurement> til_measurements;
    for (const auto& m : measurements) {
        if (m.label.find("TIL") != std::string::npos) {
            til_measurements.push_back(m);
        }
    }
    result.til = analyze_til(til_measurements);

    return result;
}

// Backwards-compatible dispatch — defaults to the simulated backend.
// `dry_run` historically meant "use simulated data"; we honour that.
WF4AllInkLimits analyze_wf4_ink_limits(const WF4ChartLayout& chart, bool /*dry_run*/) {
    WF4SimulatedBackend backend;
    return analyze_wf4_ink_limits(chart, backend);
}

// New: explicit-backend dispatch. Callers pick simulated or real hardware
// via a WF4MeasurementBackend subclass and the analysis core is agnostic.
WF4AllInkLimits analyze_wf4_ink_limits(const WF4ChartLayout& chart,
                                        WF4MeasurementBackend& backend) {
    auto measurements = backend.measure(chart);
    return analyze_wf4_ink_limits_core(chart, measurements);
}

}  // namespace workflow
}  // namespace jcolour
