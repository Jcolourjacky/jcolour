#pragma once

#include <vector>
#include <string>
#include <array>
#include <chrono>

namespace jcolour {

// Forward declare calibration classes
namespace calibration {
    class G7ValidationReport;
    class G7ComplianceChecker;
    struct G7ReportMetadata;
    struct G7CalibrationResult;
    struct G7PatchResult;
}

namespace workflow {

// Forward declaration
struct WF4ChartLayout;

// Self-test measurement data
struct WF4SelfTestMeasurement {
    int patch_index = 0;
    std::string patch_label;
    float L_star = 0.0f;
    float a_star = 0.0f;
    float b_star = 0.0f;
    float delta_e = 0.0f;
    bool passes = false;
};

// Self-test result
struct WF4SelfTestResult {
    bool passed = false;
    int total_patches = 0;
    int passing_patches = 0;
    int failing_patches = 0;
    float avg_delta_e = 0.0f;
    float max_delta_e = 0.0f;
    std::string verdict;  // "PASS", "FAIL", "CALIBRATE"
    std::vector<WF4SelfTestMeasurement> measurements;
};

// Light recalibration result
struct WF4LightRecalibResult {
    bool success = false;
    float improvement_percent = 0.0f;
    int patches_improved = 0;
    int patches_degraded = 0;
    std::string message;
};

// Full recalibration result
struct WF4FullRecalibResult {
    bool success = false;
    float avg_delta_e_before = 0.0f;
    float avg_delta_e_after = 0.0f;
    std::string message;
};

// Generate self-test chart (29 patches)
WF4ChartLayout generate_wf4_self_test_chart();

// Run self-test against baseline
WF4SelfTestResult run_wf4_self_test(
    const WF4ChartLayout& chart,
    const std::vector<std::array<float, 3>>& current_lab,
    const std::vector<std::array<float, 3>>& baseline_lab);

// Run light recalibration (per-ink linearization + G7 correction)
WF4LightRecalibResult run_wf4_light_recalib(
    const WF4ChartLayout& chart,
    const std::vector<std::array<float, 3>>& measurements);

// Run full recalibration (rebuild LUT + ICC)
WF4FullRecalibResult run_wf4_full_recalib(
    const WF4ChartLayout& chart,
    const std::vector<std::array<float, 3>>& measurements);

// Calculate delta E between two Lab values
float calculate_delta_e(
    float L1, float a1, float b1,
    float L2, float a2, float b2);

// Determine calibration state from self-test result
std::string determine_calib_state(const WF4SelfTestResult& result);

}
}