#include "wf3_measure.h"
#include "wf3_chart.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace jcolour {
namespace workflow {

namespace {

// K/S → R via the Kubelka–Munk single-constant inversion:
//   R = 1 + K/S − √((K/S)² + 2·K/S)
// Numerically stable for K/S ≥ 0; clamped against extreme values.
inline float ks_to_reflectance(float ks) {
    if (ks < 0.0f) ks = 0.0f;
    float root = std::sqrt(ks * ks + 2.0f * ks);
    return 1.0f + ks - root;
}

// R → K/S via the inverse:  K/S = (1 − R)² / (2·R)
inline float reflectance_to_ks(float R) {
    if (R < 1e-4f) R = 1e-4f;
    if (R > 0.9999f) R = 0.9999f;
    return ((1.0f - R) * (1.0f - R)) / (2.0f * R);
}

// Apply the Yule–Nielsen correction R_apparent = R_predicted^(1/n) only at
// the prediction stage; the n factor accounts for spectral dot-gain when
// halftoned ink reflectance is observed by the spectrophotometer.
inline float apply_yule_nielsen(float R, float n) {
    if (n <= 0.0f) return R;
    return std::pow(std::max(R, 0.0f), 1.0f / n);
}

}  // namespace

std::array<float, 36> predict_wf3_spectrum(
    const std::array<float, 6>& ink_densities,
    const WF3KMModel& model) {

    std::array<float, 36> result{};

    if (!model.params) {
        result.fill(1.0f);
        return result;
    }

    const float n = (model.params->n > 0.0f) ? model.params->n : 1.0f;
    const float R_min = model.params->R_min;
    const float R_max = model.params->R_max;

    for (int wl = 0; wl < 36; ++wl) {
        // Paper baseline K/S at this wavelength.
        float ks_paper = reflectance_to_ks(model.paper_white[wl]);

        // Each ink contributes c · (K/S_full − K/S_paper). For c = 0 the
        // mixture collapses back to paper; for c = 1 on a single channel
        // the mixture equals that channel's stored K/S.
        float ks_mix = ks_paper;
        for (int ch = 0; ch < 6; ++ch) {
            float c = ink_densities[ch];
            if (c <= 0.0f) continue;
            float ks_full = model.params->km_ks_full[ch][wl];
            ks_mix += c * (ks_full - ks_paper);
        }
        if (ks_mix < 0.0f) ks_mix = 0.0f;

        float R = ks_to_reflectance(ks_mix);
        R = apply_yule_nielsen(R, n);

        if (R < R_min) R = R_min;
        if (R > R_max) R = R_max;
        result[wl] = R;
    }

    return result;
}

WF3KMModel fit_wf3_km_model(
    const std::vector<WF3Measurement>& single_channel_measurements,
    const std::vector<int>& channel_indices,
    const std::array<float, 36>* paper_white_override) {

    WF3KMModel model;
    model.params = std::make_unique<WF3CYNSNParams>();
    model.params->n = 2.0f;

    // ---- Paper white ----------------------------------------------------
    // Prefer an override (the caller knows the actual paper). Otherwise
    // scan for a measurement where ink density is zero — that's our
    // best in-band paper sample. Fall back to a sensible default if
    // neither is available.
    if (paper_white_override) {
        model.paper_white = *paper_white_override;
    } else {
        bool found_paper = false;
        for (const auto& m : single_channel_measurements) {
            if (m.ink_density <= 0.001f) {
                model.paper_white = m.spectrum;
                found_paper = true;
                break;
            }
        }
        if (!found_paper) {
            model.paper_white.fill(0.95f);  // Typical photo paper
        }
    }

    // ---- Per-channel K/S at full coverage --------------------------------
    // For each requested channel, find the measurement at the highest
    // density (best approximation of full coverage) and compute K/S(λ)
    // from the measured reflectance. This is the F8 audit fix: previously
    // the model hardcoded avg_reflectance = 0.5 for every channel.
    for (int ch_idx : channel_indices) {
        if (ch_idx < 0 || ch_idx >= 6) continue;

        const WF3Measurement* best = nullptr;
        for (const auto& m : single_channel_measurements) {
            if (m.channel != ch_idx) continue;
            if (m.ink_density <= 0.001f) continue;
            if (!best || m.ink_density > best->ink_density) {
                best = &m;
            }
        }

        if (best) {
            // Convert the measured reflectance at density c_best into
            // K/S, then scale to the c = 1 endpoint linearly:
            //   K/S_full = K/S_paper + (K/S_measured − K/S_paper) / c_best
            float c = std::max(0.05f, best->ink_density);
            for (int wl = 0; wl < 36; ++wl) {
                float ks_measured = reflectance_to_ks(best->spectrum[wl]);
                float ks_paper = reflectance_to_ks(model.paper_white[wl]);
                float ks_full = ks_paper + (ks_measured - ks_paper) / c;
                model.params->km_ks_full[ch_idx][wl] = std::max(0.0f, ks_full);
            }
        } else {
            // No measurement for this channel — leave as paper baseline so
            // the model is still total but this channel has no effect.
            for (int wl = 0; wl < 36; ++wl) {
                model.params->km_ks_full[ch_idx][wl] =
                    reflectance_to_ks(model.paper_white[wl]);
            }
        }
    }

    return model;
}

float fit_wf3_yule_nielsen_n(
    const WF3KMModel& model,
    const std::vector<WF3Measurement>& measurements) {

    if (!model.params || measurements.empty()) return 2.0f;

    // F9 audit fix: real grid search using the spectral RMS residual
    // between model prediction and measurement (not "error += 0.01f").
    float best_n = 2.0f;
    double best_rms = std::numeric_limits<double>::infinity();

    auto eval = [&](float n) -> double {
        WF3KMModel probe;
        probe.paper_white = model.paper_white;
        probe.params = std::make_unique<WF3CYNSNParams>(*model.params);
        probe.params->n = n;

        double sq_sum = 0.0;
        size_t count = 0;
        for (const auto& m : measurements) {
            if (m.ink_density <= 0.001f) continue;  // skip paper white
            std::array<float, 6> mix{};
            if (m.channel >= 0 && m.channel < 6) {
                mix[m.channel] = m.ink_density;
            } else {
                continue;
            }
            auto pred = predict_wf3_spectrum(mix, probe);
            for (int wl = 0; wl < 36; ++wl) {
                double d = static_cast<double>(pred[wl]) - m.spectrum[wl];
                sq_sum += d * d;
                ++count;
            }
        }
        return count > 0 ? std::sqrt(sq_sum / count) : std::numeric_limits<double>::infinity();
    };

    for (float n = 1.0f; n <= 4.0f + 1e-3f; n += 0.1f) {
        double rms = eval(n);
        if (rms < best_rms) {
            best_rms = rms;
            best_n = n;
        }
    }

    return best_n;
}

WF3InkLimits analyze_wf3_ink_limits(
    const std::vector<WF3Measurement>& ramp_measurements) {

    WF3InkLimits limits;

    // Compute per-channel D_min (paper) and D_max (highest ink) from the
    // actual measurements rather than aggregating across channels (which
    // the original code did, making D_max meaningless).
    for (int ch = 0; ch < 6; ++ch) {
        WF3InkLimits::PerChannelLimit limit;
        limit.channel = ch;
        limit.max_density = 1.0f;

        float min_L = 100.0f;
        float max_L = 0.0f;
        bool has_data = false;
        for (const auto& m : ramp_measurements) {
            if (m.channel != ch) continue;
            has_data = true;
            if (m.L_star < min_L) min_L = m.L_star;
            if (m.L_star > max_L) max_L = m.L_star;
        }

        if (has_data) {
            limit.D_min = 0.0f;          // paper is "no density"
            limit.D_max = max_L - min_L;  // ΔL* between paper and ink-full
        }
        limits.limits.push_back(limit);
    }

    return limits;
}

bool run_wf3_calibration(
    const Wf3ChartLayout& /*chart*/,
    const std::vector<WF3Measurement>& measurements,
    WF3KMModel& out_model,
    WF3InkLimits& out_limits) {

    if (measurements.empty()) {
        return false;
    }

    std::vector<int> channels(6);
    for (int i = 0; i < 6; ++i) channels[i] = i;

    out_model = fit_wf3_km_model(measurements, channels);
    out_limits = analyze_wf3_ink_limits(measurements);
    return true;
}

std::vector<WF3Measurement> simulate_wf3_measurements(
    const Wf3ChartLayout& chart) {

    std::vector<WF3Measurement> result;
    result.reserve(chart.patches.size() + chart.multi_patches.size());

    // Deterministic dither for reproducible test output (replaces the old
    // std::rand path that gave run-to-run variation).
    auto noise = [](int seed) -> float {
        uint32_t x = static_cast<uint32_t>(seed) * 2654435761u + 1u;
        x = (x ^ (x >> 16)) * 0x85ebca6bu;
        return (static_cast<float>(x & 0xFFFF) / 65535.0f - 0.5f) * 0.01f;
    };

    // Cheap per-channel spectral absorption profile. Three bands keep this
    // light but channel-distinct enough that K-M fitting recovers a stable,
    // distinguishable per-channel K/S curve during tests.
    static constexpr float channel_tilt[6][3] = {
        {1.00f, 1.00f, 1.00f},  // MAIN_K — neutral
        {0.90f, 0.90f, 0.90f},  // GY     — slightly less than black
        {0.55f, 0.55f, 0.55f},  // LGY    — much less
        {1.20f, 0.40f, 0.30f},  // C      — absorbs red/yellow
        {0.30f, 1.20f, 0.40f},  // VM     — absorbs green/blue
        {0.30f, 0.40f, 1.20f},  // Y      — absorbs blue
    };
    auto band_of = [](int wl) { return (wl < 14) ? 0 : (wl < 26 ? 1 : 2); };

    // ---- Single-ink patches ---------------------------------------------
    for (size_t i = 0; i < chart.patches.size(); ++i) {
        const auto& patch = chart.patches[i];
        WF3Measurement m;
        m.patch_index = static_cast<int>(i);
        m.label = patch.label;
        m.channel = static_cast<int>(patch.channel);
        m.ink_density = patch.ink_density;

        float paper_R = 0.93f;
        float attenuation = patch.ink_density * 0.75f;
        int ch = std::clamp(m.channel, 0, 5);

        for (int wl = 0; wl < 36; ++wl) {
            float band_factor = channel_tilt[ch][band_of(wl)];
            float R = paper_R - attenuation * band_factor * 0.7f;
            R = std::clamp(R + noise(static_cast<int>(i) * 36 + wl),
                           0.02f, 0.98f);
            m.spectrum[wl] = R;
        }
        m.L_star = std::max(0.0f, std::min(100.0f,
                                            100.0f * (1.0f - attenuation * 0.85f)));
        m.a_star = (ch == 3) ? -10.0f : (ch == 4 ? 30.0f : 0.0f);
        m.b_star = (ch == 5) ? 40.0f : 0.0f;

        result.push_back(m);
    }

    // ---- Multi-ink (Neugebauer / cellular) patches ---------------------
    // F22 audit fix: multi_patches were previously degraded to single-ink
    // measurements; now we simulate their combined spectrum by stacking
    // each ink's attenuation. The dominant channel (largest density) is
    // recorded in m.channel for callers that only consult that field.
    size_t base_index = chart.patches.size();
    for (size_t j = 0; j < chart.multi_patches.size(); ++j) {
        const auto& mp = chart.multi_patches[j];
        WF3Measurement m;
        m.patch_index = static_cast<int>(base_index + j);
        m.label = mp.label;

        int dom_channel = 0;
        float dom_density = 0.0f;
        float paper_R = 0.93f;

        for (int wl = 0; wl < 36; ++wl) {
            int band = band_of(wl);
            float total_attenuation = 0.0f;
            for (const auto& [ch_enum, dens] : mp.channels) {
                int ch = std::clamp(static_cast<int>(ch_enum), 0, 5);
                total_attenuation += dens * 0.75f * channel_tilt[ch][band] * 0.7f;
                if (dens > dom_density) {
                    dom_density = dens;
                    dom_channel = ch;
                }
            }
            float R = paper_R - total_attenuation;
            R = std::clamp(R + noise(static_cast<int>(m.patch_index) * 36 + wl),
                           0.02f, 0.98f);
            m.spectrum[wl] = R;
        }

        m.channel = dom_channel;
        m.ink_density = dom_density;

        // Sum attenuation across inks for a rough L* (caps at near-black
        // when total ink loading is high).
        float total_dens = 0.0f;
        for (const auto& [ch_enum, dens] : mp.channels) {
            (void)ch_enum;
            total_dens += dens;
        }
        float att = std::min(0.95f, total_dens * 0.55f);
        m.L_star = std::max(0.0f, std::min(100.0f, 100.0f * (1.0f - att)));
        m.a_star = 0.0f;
        m.b_star = 0.0f;

        result.push_back(m);
    }

    return result;
}

}
}
