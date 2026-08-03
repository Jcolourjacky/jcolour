#include "wf2_self_test.h"
#include "../src/delta_e.h"
#include <algorithm>
#include <cmath>

namespace jcolour {
namespace workflow {

Wf2ChartLayout generate_wf2_self_test_chart() {
    Wf2ChartLayout layout;
    layout.columns = 5;
    layout.dpi = 1440;
    layout.patch_size_mm = 10.0f;

    // 3 channels × 3 steps = 9 patches
    std::vector<float> steps = {0.25f, 0.5f, 0.75f};
    for (int ch = 0; ch < WF2_NUM_CHANNELS; ++ch) {
        for (float step : steps) {
            Wf2Patch patch;
            patch.channel = static_cast<Wf2Channel>(ch);
            patch.ink_density = step;
            patch.label = "ST_" + std::to_string(ch) + "_" +
                          std::to_string(static_cast<int>(step * 100));
            layout.patches.push_back(patch);
        }
    }

    // 5 neutral gray steps via MAIN_K
    for (int i = 0; i < 5; ++i) {
        Wf2Patch patch;
        patch.channel = Wf2Channel::MAIN_K;
        patch.ink_density = (i + 1) * 20.0f / 100.0f;
        patch.label = "N_" + std::to_string(static_cast<int>(patch.ink_density * 100));
        layout.patches.push_back(patch);
    }

    // 2 mixed tone patches
    std::vector<std::pair<Wf2Channel, float>> mixed = {
        {Wf2Channel::MAIN_K, 0.3f},
        {Wf2Channel::GY,     0.4f}
    };
    for (size_t i = 0; i < mixed.size(); ++i) {
        Wf2Patch patch;
        patch.channel = mixed[i].first;
        patch.ink_density = mixed[i].second;
        patch.label = "M_" + std::to_string(static_cast<int>(i));
        layout.patches.push_back(patch);
    }

    return layout;
}

Wf2SelfTestResult run_wf2_self_test(
    const Wf2ChartLayout& chart,
    const std::vector<Wf2Measurement>& measurements,
    const std::vector<Wf2Measurement>& baseline) {

    Wf2SelfTestResult result;
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
    result.passed = (result.avg_delta_e < 1.0 && result.max_delta_e < 2.0);

    return result;
}

Wf2LightRecalibResult run_wf2_light_recalib(
    const Wf2ChartLayout& chart,
    const std::vector<Wf2Measurement>& measurements,
    const LStarToInkLUT& current_lut) {

    Wf2LightRecalibResult result;

    // Simple per-ink linearization adjustment.
    // Compare current LUT predictions with measurements and adjust.

    LStarToInkLUT new_lut = current_lut;

    for (const auto& m : measurements) {
        int lstar_idx = static_cast<int>(m.L_star);
        lstar_idx = std::max(0, std::min(255, lstar_idx));

        // Adjust each of the 3 channels based on offset from the mid-tone reference.
        for (int ch = 0; ch < WF2_NUM_CHANNELS; ++ch) {
            float adjustment = (m.L_star - 50.0) / 100.0 * 0.05;  // 5% max
            new_lut[lstar_idx][ch] = current_lut[lstar_idx][ch] * (1.0f + adjustment);
            new_lut[lstar_idx][ch] = std::max(0.0f, std::min(1.0f, new_lut[lstar_idx][ch]));
        }
    }

    result.success = true;
    result.new_lut = new_lut;
    result.improvement = 0.3;  // Placeholder

    return result;
}

LStarToInkLUT run_wf2_full_recalib(
    const Wf2ChartLayout& chart,
    const std::vector<Wf2Measurement>& measurements,
    const Wf2KMModel& km_model,
    const Wf2AllInkLimits& ink_limits,
    const Wf2SplitConfig& config) {

    auto lut_result = generate_wf2_split_lut(km_model, ink_limits, config);
    return lut_result.lut;
}

}  // namespace workflow
}  // namespace jcolour
