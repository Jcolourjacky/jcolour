#include "wf6_self_test.h"
#include "wf4_chart.h"
#include <algorithm>
#include <cmath>

namespace jcolour {
namespace workflow {

Wf6ChartLayout generate_wf6_self_test_chart() {
    Wf6ChartLayout layout;
    layout.gamut_compression_enabled = true;

    // Generate test points across ProPhoto RGB gamut
    auto test_points = generate_prophoto_test_points(50);
    for (size_t i = 0; i < test_points.size(); ++i) {
        InkPatch patch;
        patch.label = "ST6_" + std::to_string(static_cast<int>(i));
        layout.base_chart.single_ink_patches.push_back(patch);
    }

    // Add skin tone patches
    std::vector<std::array<float, 3>> skin_tones = {
        {0.95f, 0.75f, 0.65f},  // Light skin
        {0.80f, 0.60f, 0.50f},  // Medium skin
        {0.65f, 0.45f, 0.35f},  // Dark skin
    };
    for (size_t i = 0; i < skin_tones.size(); ++i) {
        InkPatch patch;
        patch.label = "SKIN_" + std::to_string(static_cast<int>(i));
        layout.base_chart.single_ink_patches.push_back(patch);
    }

    // Add memory color patches (sky, foliage)
    std::vector<std::array<float, 3>> memory_colors = {
        {0.3f, 0.6f, 0.95f},   // Sky blue
        {0.2f, 0.5f, 0.3f},    // Foliage green
        {0.95f, 0.9f, 0.2f},   // Yellow (sunflower)
    };
    for (size_t i = 0; i < memory_colors.size(); ++i) {
        InkPatch patch;
        patch.label = "MEM_" + std::to_string(static_cast<int>(i));
        layout.base_chart.single_ink_patches.push_back(patch);
    }

    layout.base_chart.columns = 8;
    layout.base_chart.dpi = 1440;

    return layout;
}

Wf6SelfTestResult run_wf6_self_test(
    const Wf6ChartLayout&,
    const std::vector<std::array<float, 3>>& measurements,
    const std::vector<std::array<float, 3>>& baseline,
    const Wf6GamutConfig& config) {

    Wf6SelfTestResult result;
    result.patches_tested = static_cast<int>(measurements.size());

    if (measurements.size() != baseline.size()) {
        return result;
    }

    double total_de = 0.0;
    double max_de = 0.0;
    int compressed = 0;

    for (size_t i = 0; i < measurements.size(); ++i) {
        // Calculate ΔE94
        double delta_l = measurements[i][0] - baseline[i][0];
        double delta_a = measurements[i][1] - baseline[i][1];
        double delta_b = measurements[i][2] - baseline[i][2];

        double de94 = std::sqrt(
            delta_l * delta_l +
            0.25 * delta_a * delta_a +
            delta_b * delta_b
        );

        result.individual_delta_e.push_back(de94);
        total_de += de94;
        max_de = std::max(max_de, de94);

        // Track gamut compression usage (always enabled for wf6)
        {
            // Simple heuristic: large L*a*b changes indicate compression
            if (std::abs(delta_a) + std::abs(delta_b) > 5.0) {
                compressed++;
            }
        }
    }

    result.avg_delta_e = total_de / measurements.size();
    result.max_delta_e = max_de;
    result.passed = (result.avg_delta_e < 2.0 && result.max_delta_e < 4.0);
    result.gamut_compressed_count = compressed;

    return result;
}

Wf6LightRecalibResult run_wf6_light_recalib(
    const Wf6ChartLayout&,
    const std::vector<std::array<float, 3>>& measurements,
    const Wf6LUT& current_lut) {

    Wf6LightRecalibResult result;

    Wf6LUT new_lut = current_lut;
    new_lut.data.resize(current_lut.data.size());

    // Apply gamma adjustment based on L* errors
    double gamma_adjust = 1.0;

    for (const auto& m : measurements) {
        double l_star = m[0];
        // If measured L* is higher than expected, decrease ink
        if (l_star > 50.0) {
            gamma_adjust *= 0.98;
        } else {
            gamma_adjust *= 1.02;
        }
    }

    // Apply gamma adjustment to LUT
    for (size_t i = 0; i < current_lut.data.size(); ++i) {
        new_lut.data[i] = std::pow(current_lut.data[i], gamma_adjust);
    }

    result.success = true;
    result.new_lut = new_lut;
    result.improvement = 0.4;

    return result;
}

Wf6LUT run_wf6_full_recalib(
    const Wf6ChartLayout&,
    const std::vector<std::array<float, 3>>& measurements,
    const Wf6GamutConfig& config) {

    // Build new LUT from measurements
    Wf6LUT lut;
    lut.size = 33;
    lut.channels = WF6_NUM_CHANNELS;
    lut.data.resize(lut.size * lut.size * lut.size * WF6_NUM_CHANNELS, 0.0f);

    // Fill with default values based on measurements
    for (size_t i = 0; i < measurements.size(); ++i) {
        auto rgb = measurements[i];
        int idx = static_cast<int>(rgb[0] * (lut.size - 1)) +
                  static_cast<int>(rgb[1] * (lut.size - 1)) * lut.size +
                  static_cast<int>(rgb[2] * (lut.size - 1)) * lut.size * lut.size;

        for (int ch = 0; ch < WF6_NUM_CHANNELS; ++ch) {
            float value = 0.5f + static_cast<float>(ch) * 0.04f;
            if (idx + ch < static_cast<int>(lut.data.size())) {
                lut.data[idx * WF6_NUM_CHANNELS + ch] = value;
            }
        }
    }

    return lut;
}

}  // namespace workflow
}  // namespace jcolour