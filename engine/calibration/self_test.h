#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace jcolour {
namespace calibration {

// Self-test result for a parameter set
struct SelfTestResult {
    enum class Verdict {
        PASS,           // ΔE avg < threshold, no action needed
        LIGHT_RECALIB,   // ΔE slightly elevated, recommend light recalibration
        FULL_RECALIB     // ΔE too high, full recalibration required
    };

    Verdict verdict = Verdict::FULL_RECALIB;

    // Statistics
    double delta_e_avg = 0.0;
    double delta_e_max = 0.0;
    int    patch_count = 0;

    // Per-patch details
    std::vector<double> per_patch_delta_e;

    // Metadata
    int64_t    tested_at = 0;
    std::string error_message;
};

// Run self-test for a specific parameter set
// Combines: chart generation → printing → scanning → delta_e stats → verdict
struct SelfTestOptions {
    int workflow = 1;           // Which workflow (1-6)
    std::string ink_batch_id;   // Current ink batch
    std::string paper_id;       // Current paper
    bool dry_run = false;       // Use simulation instead of real hardware
};

SelfTestResult run_self_test(const SelfTestOptions& options,
                              const std::string& printer_host,
                              int64_t baseline_calibrated_at);

// Classify a self-test based on thresholds
SelfTestResult::Verdict classify_result(double delta_e_avg,
                                         double delta_e_max,
                                         int workflow);

}  // namespace calibration
}  // namespace jcolour