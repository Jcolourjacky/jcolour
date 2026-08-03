#pragma once

#include <array>
#include <string>
#include <vector>

namespace jcolour {
namespace workflow {

// Complete workflow 1 calibration session
// This orchestrates the entire workflow 1 process from start to finish
struct WF1CalibrationSession {
    std::string paper_id;
    std::string ink_batch_id;
    bool dry_run = false;  // Default to real hardware (simulation only in tests)

    // Configuration
    int ink_ramp_steps = 21;  // Number of steps in ink ramp
    int verification_steps = 11;
    float max_ink_limit = 0.95f;  // Safety cap
};

// Result of a complete workflow 1 calibration
struct WF1CalibrationResult {
    enum class Status {
        SUCCESS,
        PARTIAL,  // Some steps completed but issues remain
        FAILED
    };

    Status status = Status::FAILED;

    // Calibration data
    float ink_limit = 0.0f;
    std::array<float, 256> lut;  // L* → ink% mapping

    // Validation results
    float delta_e_avg = 0.0f;
    float delta_e_max = 0.0f;
    bool verification_passed = false;

    // Diagnostics
    std::string error_message;
    std::vector<std::string> warnings;
};

// Run complete workflow 1 calibration
WF1CalibrationResult run_wf1_calibration(const WF1CalibrationSession& session);

}  // namespace workflow
}  // namespace jcolour
