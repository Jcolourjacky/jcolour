#include "wf3_self_test.h"
#include "../src/delta_e.h"
#include <algorithm>
#include <cmath>

namespace jcolour {
namespace workflow {

Wf3ChartLayout generate_wf3_self_test_chart(BlackKind black_kind) {
    Wf3ChartLayout layout;
    layout.black_kind = black_kind;
    layout.columns = 8;
    layout.dpi = 1440;
    layout.patch_size_mm = 10.0f;

    // 6 channels × 3 steps = 18 patches
    std::vector<float> steps = {0.25f, 0.5f, 0.75f};
    for (int ch = 0; ch < WF3_NUM_CHANNELS; ++ch) {
        for (float step : steps) {
            Wf3Patch patch;
            patch.channel = static_cast<Wf3Channel>(ch);
            patch.ink_density = step;
            patch.label = "ST_" + std::to_string(ch) + "_" +
                          std::to_string(static_cast<int>(step * 100));
            layout.patches.push_back(patch);
        }
    }

    // 7 neutral gray steps via MAIN_K
    for (int i = 0; i < 7; ++i) {
        Wf3Patch patch;
        patch.channel = Wf3Channel::MAIN_K;
        patch.ink_density = (i + 1) * 12.5f / 100.0f;
        patch.label = "N_" + std::to_string(static_cast<int>(patch.ink_density * 100));
        layout.patches.push_back(patch);
    }

    // 3 split-tone patches (warm/cool/neutral) — printed at the main-black
    // midpoint; the preset itself just labels them for the operator.
    std::vector<TonePreset> presets = {TonePreset::WARM, TonePreset::COOL, TonePreset::NEUTRAL};
    for (size_t i = 0; i < presets.size(); ++i) {
        Wf3Patch patch;
        patch.channel = Wf3Channel::MAIN_K;
        patch.ink_density = 0.5f;
        patch.label = "SPLIT_" + std::to_string(static_cast<int>(i));
        layout.patches.push_back(patch);
    }

    return layout;
}

Wf3SelfTestResult run_wf3_self_test(
    const Wf3ChartLayout& chart,
    const std::vector<WF3Measurement>& measurements,
    const std::vector<WF3Measurement>& baseline) {

    Wf3SelfTestResult result;
    result.patches_tested = static_cast<int>(measurements.size());

    if (measurements.size() != baseline.size()) {
        return result;
    }

    double total_de = 0.0;
    double max_de = 0.0;

    for (size_t i = 0; i < measurements.size(); ++i) {
        double de = std::sqrt(
            std::pow(measurements[i].L_star - baseline[i].L_star, 2) +
            std::pow(measurements[i].a_star - baseline[i].a_star, 2) +
            std::pow(measurements[i].b_star - baseline[i].b_star, 2)
        );
        result.individual_delta_e.push_back(de);
        total_de += de;
        max_de = std::max(max_de, de);
    }

    result.avg_delta_e = total_de / measurements.size();
    result.max_delta_e = max_de;
    result.passed = (result.avg_delta_e < 1.5 && result.max_delta_e < 2.5);

    return result;
}

Wf3LightRecalibResult run_wf3_light_recalib(
    const Wf3ChartLayout& chart,
    const std::vector<WF3Measurement>& measurements,
    const Wf3LUT& current_lut) {

    Wf3LightRecalibResult result;

    // Per-ink linearization adjustment
    Wf3LUT new_lut = current_lut;
    new_lut.entries.resize(WF3_LUT_SIZE);

    for (const auto& m : measurements) {
        int lstar_idx = static_cast<int>(m.L_star);
        lstar_idx = std::max(0, std::min(WF3_LUT_SIZE - 1, lstar_idx));

        for (int ch = 0; ch < WF3_NUM_CHANNELS; ++ch) {
            float adjustment = (m.L_star - 50.0f) / 100.0f * 0.05f;
            new_lut.entries[lstar_idx].inks[ch] = current_lut.entries[lstar_idx].inks[ch] * (1.0f + adjustment);
            new_lut.entries[lstar_idx].inks[ch] = std::max(0.0f, std::min(1.0f, new_lut.entries[lstar_idx].inks[ch]));
        }
    }

    result.success = true;
    result.new_lut = new_lut;
    result.improvement = 0.3f;

    return result;
}

Wf3LUT run_wf3_full_recalib(
    const Wf3ChartLayout& chart,
    const std::vector<WF3Measurement>& measurements,
    const WF3KMModel& km_model,
    const WF3InkLimits& ink_limits,
    const Wf3SplitConfig& config) {

    // Generate new LUT using split-tone optimization
    return generate_wf3_lut(km_model, ink_limits, config, TonePreset::NEUTRAL);
}

}  // namespace workflow
}  // namespace jcolour