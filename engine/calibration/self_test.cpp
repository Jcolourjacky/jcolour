#include "self_test.h"
#include "state_machine.h"
#include "charts.h"
#include "../workflow/wf1_measure.h"
#include "../workflow/wf1_chart.h"
#include <algorithm>
#include <cmath>

namespace jcolour {
namespace calibration {

// Convert calibration::ChartLayout to workflow::ChartLayout
static workflow::ChartLayout to_workflow_chart(const ChartLayout& cal_chart) {
    workflow::ChartLayout wc;
    wc.columns = 6;
    wc.dpi = 1440;
    wc.patch_size_mm = 10.0f;
    for (const auto& p : cal_chart.patches) {
        workflow::Patch wp;
        wp.label = p.label;
        wp.ink_density = p.cmyk[3];  // K channel for ink density
        wp.channel = 0;
        wc.patches.push_back(wp);
    }
    return wc;
}

SelfTestResult::Verdict classify_result(double delta_e_avg,
                                         double delta_e_max,
                                         int workflow) {
    // Thresholds depend on workflow (from §5)
    double avg_threshold;
    double max_threshold;

    switch (workflow) {
        case 1:  // Single black
            avg_threshold = 0.5;
            max_threshold = 1.5;
            break;
        case 2:  // Four-level gray
            avg_threshold = 1.0;
            max_threshold = 2.0;
            break;
        case 3:  // Gray + tone
            avg_threshold = 1.5;
            max_threshold = 3.0;
            break;
        case 4:  // RGB 12-channel
        case 6:  // ProPhoto RGB
            avg_threshold = 1.5;
            max_threshold = 3.0;
            break;
        default:
            avg_threshold = 1.0;
            max_threshold = 2.0;
    }

    if (delta_e_avg < avg_threshold && delta_e_max < max_threshold) {
        return SelfTestResult::Verdict::PASS;
    } else if (delta_e_avg < avg_threshold * 2 && delta_e_max < max_threshold * 2) {
        return SelfTestResult::Verdict::LIGHT_RECALIB;
    } else {
        return SelfTestResult::Verdict::FULL_RECALIB;
    }
}

SelfTestResult run_self_test(const SelfTestOptions& options,
                              const std::string& printer_host,
                              int64_t baseline_calibrated_at) {
    (void)printer_host;  // TODO: use epson::Printer
    (void)baseline_calibrated_at;

    SelfTestResult result;
    result.tested_at = std::time(nullptr);

    try {
        // Step 1: Generate self-test chart for this workflow
        ChartLayout cal_chart = build_self_test_chart(options.workflow);
        workflow::ChartLayout chart = to_workflow_chart(cal_chart);

        // Step 2: Simulate measurement (or real scan when B10 is done)
        workflow::ScanResult scan_result;
        if (options.dry_run) {
            // Use simulation
            scan_result = workflow::simulate_wf1_measurement(chart, 0.3f);
        } else {
            // Real scan via i1Pro3
            scan_result = workflow::scan_wf1_chart(chart);
            if (!scan_result.success) {
                result.error_message = scan_result.error_message;
                return result;
            }
        }

        // Step 3: Compute delta-E statistics
        if (scan_result.patches.empty()) {
            result.error_message = "No patches measured";
            return result;
        }

        result.patch_count = static_cast<int>(scan_result.patches.size());

        // For single-black workflow, delta-E is primarily L* error
        double sum_de = 0.0;
        double max_de = 0.0;

        for (const auto& patch : scan_result.patches) {
            // Compute L* error (assuming ideal: L* = 100 - 50*ink)
            double expected_lstar = 100.0 - 50.0 * patch.ink_density;
            double lstar_error = std::abs(patch.measured_lstar - expected_lstar);
            double de = lstar_error;

            sum_de += de;
            max_de = std::max(max_de, de);
            result.per_patch_delta_e.push_back(de);
        }

        result.delta_e_avg = sum_de / result.patch_count;
        result.delta_e_max = max_de;

        // Step 4: Classify the result
        result.verdict = classify_result(result.delta_e_avg, result.delta_e_max, options.workflow);

    } catch (const std::exception& e) {
        result.error_message = e.what();
    }

    return result;
}

}  // namespace calibration
}  // namespace jcolour
