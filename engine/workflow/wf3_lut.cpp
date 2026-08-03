#include "wf3_lut.h"
#include "wf3_measure.h"
#include "wf3_chart.h"
#include "../src/spectral.h"
#include "../src/delta_e.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace jcolour {
namespace workflow {

float estimate_metamerism(
    const std::array<float, 36>& spectrum,
    const std::array<float, 36>& target_spectrum) {

    // Simplified metamerism score
    // Compare under D50 and A illuminants
    spectral::Spectrum s1, s2;
    for (int i = 0; i < 36; ++i) {
        s1[i] = spectrum[i];
        s2[i] = target_spectrum[i];
    }

    auto d50_ill = spectral::Illuminant::D50();
    auto a_ill = spectral::Illuminant::A();

    spectral::XYZ xyz1_d50 = spectral::spectrum_to_xyz(s1, d50_ill);
    spectral::XYZ xyz2_d50 = spectral::spectrum_to_xyz(s2, d50_ill);
    spectral::XYZ xyz1_a = spectral::spectrum_to_xyz(s1, a_ill);
    spectral::XYZ xyz2_a = spectral::spectrum_to_xyz(s2, a_ill);

    spectral::Lab lab1_d50 = spectral::xyz_to_lab(xyz1_d50);
    spectral::Lab lab2_d50 = spectral::xyz_to_lab(xyz2_d50);
    spectral::Lab lab1_a = spectral::xyz_to_lab(xyz1_a);
    spectral::Lab lab2_a = spectral::xyz_to_lab(xyz2_a);

    float de_d50 = std::sqrt(
        std::pow(lab1_d50.L - lab2_d50.L, 2) +
        std::pow(lab1_d50.a - lab2_d50.a, 2) +
        std::pow(lab1_d50.b - lab2_d50.b, 2));
    float de_a = std::sqrt(
        std::pow(lab1_a.L - lab2_a.L, 2) +
        std::pow(lab1_a.a - lab2_a.a, 2) +
        std::pow(lab1_a.b - lab2_a.b, 2));

    return std::max(de_d50, de_a);
}

// Channel indices into the 6-element wf3 result array. See Wf3Channel enum.
constexpr int IDX_MAIN_K = static_cast<int>(Wf3Channel::MAIN_K);  // 0
constexpr int IDX_GY     = static_cast<int>(Wf3Channel::GY);      // 1
constexpr int IDX_LGY    = static_cast<int>(Wf3Channel::LGY);     // 2
constexpr int IDX_C      = static_cast<int>(Wf3Channel::C);       // 3
constexpr int IDX_VM     = static_cast<int>(Wf3Channel::VM);      // 4
constexpr int IDX_Y      = static_cast<int>(Wf3Channel::Y);       // 5

std::array<float, WF3_NUM_CHANNELS> optimize_split_tone(
    float target_l_star,
    const WF3KMModel& km_model,
    const WF3InkLimits& ink_limits,
    const Wf3SplitConfig& config) {

    std::array<float, WF3_NUM_CHANNELS> result;
    result.fill(0.0f);

    // For wf3, distribute ink as:
    //   Primary: grayscale (MAIN_K + GY + LGY) — MAIN_K is whichever black
    //            (PK or MK) is loaded for the session
    //   Secondary: C / VM / Y at low levels for split-tone
    //
    // NOTE: this is a heuristic, not real optimization (the function name is
    // legacy). The K-M model and Y-N factor are not yet used here — see
    // audit findings F7-F9. PK + MK simultaneous output is no longer
    // possible because wf3's enum exposes only MAIN_K (was the F1 bug).

    float gray_fraction = 1.0f - std::abs(config.target_a + config.target_b) / 50.0f;
    gray_fraction = std::max(0.3f, std::min(1.0f, gray_fraction));

    // total_ink ranges 0..1 with target_l_star ∈ [0, 100]. Lower L* (darker)
    // demands more ink. NOTE: original code used `current_l / 100`, which is
    // inverted (highlights would demand max ink). Fixed here as part of the
    // F5 family of gray→L* sign bugs surfaced in the audit.
    float total_ink = std::max(0.0f, (100.0f - target_l_star) / 100.0f);

    // Reassign the old PK + MK budget entirely to MAIN_K. The MK slot no
    // longer exists in the wf3 enum, so this is structurally impossible to
    // regress.
    float main_k = total_ink * 0.60f;  // was pk(0.4) + mk(0.2)
    float gy     = total_ink * 0.25f;
    float lgy    = total_ink * 0.15f;

    auto limit_of = [&](int ch) -> float {
        return (ch < static_cast<int>(ink_limits.limits.size()))
                   ? ink_limits.limits[ch].max_density
                   : 1.0f;
    };

    result[IDX_MAIN_K] = std::min(main_k, limit_of(IDX_MAIN_K));
    result[IDX_GY]     = std::min(gy,     limit_of(IDX_GY));
    result[IDX_LGY]    = std::min(lgy,    limit_of(IDX_LGY));

    // Tint channels (C / VM / Y) — proportional to chromatic preference.
    float tint_amount = (1.0f - gray_fraction) * 0.3f;
    if (config.target_a > 0) {  // Warm → add Vivid Magenta
        result[IDX_VM] = std::min(tint_amount * config.target_a / 30.0f,
                                  limit_of(IDX_VM));
    } else if (config.target_a < 0) {  // Cool → add Cyan
        result[IDX_C]  = std::min(tint_amount * -config.target_a / 30.0f,
                                  limit_of(IDX_C));
    }
    if (config.target_b > 0) {  // Warm yellow tone
        result[IDX_Y]  = std::min(tint_amount * config.target_b / 30.0f,
                                  limit_of(IDX_Y));
    }

    return result;
}

Wf3LUT generate_wf3_lut(
    const WF3KMModel& km_model,
    const WF3InkLimits& ink_limits,
    const Wf3SplitConfig& config,
    TonePreset preset) {

    Wf3LUT lut;
    lut.preset = preset;
    lut.entries.resize(WF3_LUT_SIZE);

    Wf3SplitConfig preset_config = config;

    // Adjust config based on preset
    switch (preset) {
        case TonePreset::NEUTRAL:
            preset_config.target_a = 0.0f;
            preset_config.target_b = 0.0f;
            break;
        case TonePreset::WARM:
            preset_config.target_a = 10.0f;
            preset_config.target_b = 15.0f;
            break;
        case TonePreset::COOL:
            preset_config.target_a = -8.0f;
            preset_config.target_b = -5.0f;
            break;
        case TonePreset::SPLIT:
            preset_config.target_a = 5.0f;
            preset_config.target_b = 10.0f;
            break;
    }

    // Generate LUT for each L* level
    for (int l = 0; l < WF3_LUT_SIZE; ++l) {
        float l_star = static_cast<float>(l) * 100.0f / (WF3_LUT_SIZE - 1);
        auto inks = optimize_split_tone(l_star, km_model, ink_limits, preset_config);

        Wf3LUTEntry entry;
        entry.inks = inks;

        // Estimate metamerism
        auto spectrum = predict_wf3_spectrum(inks, km_model);
        std::array<float, 36> target_spectrum;
        for (int ch = 0; ch < 36; ++ch) {
            target_spectrum[ch] = 1.0f - l_star / 100.0f * 0.7f;
        }
        entry.metamerism_score = estimate_metamerism(spectrum, target_spectrum);

        lut.entries[l] = entry;
    }

    // Post-process
    enforce_wf3_monotonicity(lut);
    smooth_wf3_lut(lut, 0.3f);

    return lut;
}

Wf3PresetLUTMap generate_all_wf3_presets(
    const WF3KMModel& km_model,
    const WF3InkLimits& ink_limits) {

    Wf3PresetLUTMap lut_map;
    Wf3SplitConfig default_config;

    // Set default ink limits
    for (int i = 0; i < WF3_NUM_CHANNELS; ++i) {
        default_config.ink_limits[i] = 1.0f;
    }

    lut_map[TonePreset::NEUTRAL] = generate_wf3_lut(km_model, ink_limits, default_config, TonePreset::NEUTRAL);
    lut_map[TonePreset::WARM] = generate_wf3_lut(km_model, ink_limits, default_config, TonePreset::WARM);
    lut_map[TonePreset::COOL] = generate_wf3_lut(km_model, ink_limits, default_config, TonePreset::COOL);
    lut_map[TonePreset::SPLIT] = generate_wf3_lut(km_model, ink_limits, default_config, TonePreset::SPLIT);

    return lut_map;
}

std::array<float, WF3_NUM_CHANNELS> wf3_lut_lookup(
    const Wf3LUT& lut,
    float l_star,
    float tint_strength) {

    int idx = static_cast<int>(l_star * (WF3_LUT_SIZE - 1) / 100.0f);
    idx = std::max(0, std::min(WF3_LUT_SIZE - 1, idx));

    return lut.entries[idx].inks;
}

void enforce_wf3_monotonicity(Wf3LUT& lut) {
    // F21 audit fix: previously the function only enforced monotonicity on
    // the channel sum, which silently allowed individual channels to
    // reverse (e.g., GY going up while LGY drops to compensate). That
    // disguised real channel-level reversal bugs.
    //
    // New behaviour: enforce per-channel monotonicity from highlight (high
    // L*, low index in the LUT walk we use) toward shadow (low L*). The
    // LUT is indexed so entries[i] corresponds to L* = i * 100 / (SIZE-1)
    // — i.e., ascending index = ascending L* = brighter pixel = less ink.
    // So per channel, lut.entries[i].inks[ch] should be non-increasing as
    // i grows (paper white at the high end uses less ink than shadows).
    //
    // We walk from the *darkest* end backward, clamping any decrease.
    if (lut.entries.size() < 2) return;
    const int n = static_cast<int>(lut.entries.size());

    for (int ch = 0; ch < WF3_NUM_CHANNELS; ++ch) {
        // From darkest (index 0) to lightest (index n-1), each ink value
        // must be ≥ the next one (≥ because going lighter = less ink).
        for (int i = n - 2; i >= 0; --i) {
            float& here = lut.entries[i].inks[ch];
            float next  = lut.entries[i + 1].inks[ch];
            if (here < next) {
                here = next;  // clamp non-decreasing toward shadows
            }
        }
    }
}

void smooth_wf3_lut(Wf3LUT& lut, float strength) {
    if (lut.entries.size() < 3) return;

    // Simple moving average smoothing
    for (size_t i = 1; i < lut.entries.size() - 1; ++i) {
        for (int ch = 0; ch < WF3_NUM_CHANNELS; ++ch) {
            float smoothed = (lut.entries[i-1].inks[ch] +
                           lut.entries[i].inks[ch] +
                           lut.entries[i+1].inks[ch]) / 3.0f;
            lut.entries[i].inks[ch] = lut.entries[i].inks[ch] * (1.0f - strength) +
                                     smoothed * strength;
        }
    }
}

Wf3LUTValidation validate_wf3_lut(
    const Wf3LUT& lut,
    const WF3KMModel& km_model) {

    Wf3LUTValidation result;
    result.is_valid = true;

    float sum_de = 0.0f;
    int count = 0;

    for (int l = 0; l < WF3_LUT_SIZE; l += 10) {  // Sample every 10th entry
        float l_star = static_cast<float>(l) * 100.0f / (WF3_LUT_SIZE - 1);
        auto inks = lut.entries[l].inks;
        auto spectrum = predict_wf3_spectrum(inks, km_model);

        // Estimate delta E from target
        spectral::Spectrum s;
        for (int ch = 0; ch < 36; ++ch) {
            s[ch] = spectrum[ch];
        }
        auto d50_ill = spectral::Illuminant::D50();
        spectral::XYZ xyz = spectral::spectrum_to_xyz(s, d50_ill);
        spectral::Lab lab = spectral::xyz_to_lab(xyz);

        float target_L = l_star;
        float de = std::abs(lab.L - target_L);

        sum_de += de;
        count++;
        result.max_delta_e = std::max(result.max_delta_e, de);

        if (de > 3.0f) {
            result.is_valid = false;
        }
    }

    result.avg_delta_e = (count > 0) ? sum_de / count : 0.0f;

    return result;
}

}  // namespace workflow
}  // namespace jcolour
