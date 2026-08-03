#pragma once

#include <string>
#include <vector>
#include "wf4_transform.h"
#include "wf4_profile.h"

namespace jcolour {
namespace workflow {

// Workflow 4 end-to-end result
struct WF4CalibrationResult {
    enum class Status { SUCCESS, PARTIAL, FAILED };
    Status status = Status::FAILED;
    WF4AllInkLimits ink_limits;
    WF4Profile profile;
    std::string error_message;
};

// Run full workflow 4 calibration
// This orchestrates: chart generation -> printing -> measurement -> ICC generation
WF4CalibrationResult run_wf4_calibration(
    const std::string& printer_uri,
    const std::string& paper_id,
    bool dry_run = true);

// Print workflow 4 chart using PJL
bool print_wf4_chart(
    const WF4ChartLayout& chart,
    const std::string& printer_uri);

// Measure workflow 4 chart using i1Pro3
std::vector<WF4Measurement> measure_wf4_chart(
    const WF4ChartLayout& chart,
    bool dry_run = true);

// Generate verification report
struct WF4VerificationReport {
    double avg_delta_e = 0.0;
    double max_delta_e = 0.0;
    int patches_tested = 0;
    bool passed = false;
};

WF4VerificationReport verify_wf4_calibration(
    const WF4CalibrationResult& result,
    const std::string& verification_chart_path);

}  // namespace workflow
}  // namespace jcolour
