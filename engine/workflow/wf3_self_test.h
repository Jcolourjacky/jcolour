#pragma once

#include <vector>
#include <string>
#include "wf3_lut.h"
#include "wf3_measure.h"

namespace jcolour {
namespace workflow {

// Self-test result for workflow 3
struct Wf3SelfTestResult {
    double avg_delta_e = 0.0;
    double max_delta_e = 0.0;
    int patches_tested = 0;
    bool passed = false;
    std::vector<double> individual_delta_e;
};

// Light recalibration result
struct Wf3LightRecalibResult {
    bool success = false;
    Wf3LUT new_lut;
    double improvement = 0.0;
};

// Generate self-test patches
Wf3ChartLayout generate_wf3_self_test_chart(BlackKind black_kind = BlackKind::PK);

// Run self-test against baseline
Wf3SelfTestResult run_wf3_self_test(
    const Wf3ChartLayout& chart,
    const std::vector<WF3Measurement>& measurements,
    const std::vector<WF3Measurement>& baseline);

// Perform light recalibration (per-ink linearization)
Wf3LightRecalibResult run_wf3_light_recalib(
    const Wf3ChartLayout& chart,
    const std::vector<WF3Measurement>& measurements,
    const Wf3LUT& current_lut);

// Full recalibration
Wf3LUT run_wf3_full_recalib(
    const Wf3ChartLayout& chart,
    const std::vector<WF3Measurement>& measurements,
    const WF3KMModel& km_model,
    const WF3InkLimits& ink_limits,
    const Wf3SplitConfig& config);

}  // namespace workflow
}  // namespace jcolour