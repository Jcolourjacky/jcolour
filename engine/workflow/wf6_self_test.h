#pragma once

#include <vector>
#include <string>
#include "wf6_chart.h"
#include "wf6_gamut.h"

namespace jcolour {
namespace workflow {

// Self-test result for workflow 6
struct Wf6SelfTestResult {
    double avg_delta_e = 0.0;
    double max_delta_e = 0.0;
    int patches_tested = 0;
    bool passed = false;
    std::vector<double> individual_delta_e;
    int gamut_compressed_count = 0;
};

// Light recalibration result
struct Wf6LightRecalibResult {
    bool success = false;
    Wf6LUT new_lut;
    double improvement = 0.0;
};

// Generate self-test patches for workflow 6
Wf6ChartLayout generate_wf6_self_test_chart();

// Run self-test against baseline
Wf6SelfTestResult run_wf6_self_test(
    const Wf6ChartLayout& chart,
    const std::vector<std::array<float, 3>>& measurements,
    const std::vector<std::array<float, 3>>& baseline,
    const Wf6GamutConfig& config);

// Perform light recalibration (gamma curve adjustment)
Wf6LightRecalibResult run_wf6_light_recalib(
    const Wf6ChartLayout& chart,
    const std::vector<std::array<float, 3>>& measurements,
    const Wf6LUT& current_lut);

// Full recalibration
Wf6LUT run_wf6_full_recalib(
    const Wf6ChartLayout& chart,
    const std::vector<std::array<float, 3>>& measurements,
    const Wf6GamutConfig& config);

}  // namespace workflow
}  // namespace jcolour