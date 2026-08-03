#pragma once

#include <vector>
#include <string>
#include "wf2_lut.h"
#include "wf2_measure.h"

namespace jcolour {
namespace workflow {

// Self-test result for workflow 2
struct Wf2SelfTestResult {
    double avg_delta_e = 0.0;
    double max_delta_e = 0.0;
    int patches_tested = 0;
    bool passed = false;
    std::vector<double> individual_delta_e;
};

// Light recalibration result
struct Wf2LightRecalibResult {
    bool success = false;
    LStarToInkLUT new_lut;
    double improvement = 0.0;
};

// Generate self-test patches (19 patches)
Wf2ChartLayout generate_wf2_self_test_chart();

// Run self-test against baseline
Wf2SelfTestResult run_wf2_self_test(
    const Wf2ChartLayout& chart,
    const std::vector<Wf2Measurement>& measurements,
    const std::vector<Wf2Measurement>& baseline);

// Perform light recalibration (only per-ink linearization)
Wf2LightRecalibResult run_wf2_light_recalib(
    const Wf2ChartLayout& chart,
    const std::vector<Wf2Measurement>& measurements,
    const LStarToInkLUT& current_lut);

// Full recalibration
LStarToInkLUT run_wf2_full_recalib(
    const Wf2ChartLayout& chart,
    const std::vector<Wf2Measurement>& measurements,
    const Wf2KMModel& km_model,
    const Wf2AllInkLimits& ink_limits,
    const Wf2SplitConfig& config);

}  // namespace workflow
}  // namespace jcolour