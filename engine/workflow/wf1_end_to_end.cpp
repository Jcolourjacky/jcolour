#include "wf1_end_to_end.h"
#include "wf1_chart.h"
#include "wf1_measure.h"
#include "wf1_ink_limit.h"
#include "wf1_lut.h"
#include "wf1_param.h"
#include "wf1_print.h"
#include <stdexcept>
#include <cmath>

namespace jcolour {
namespace workflow {

WF1CalibrationResult run_wf1_calibration(const WF1CalibrationSession& session) {
    WF1CalibrationResult result;

    try {
        // Step 1: Generate ink ramp chart
        ChartLayout chart = generate_wf1_ink_ramp();
        result.warnings.push_back("Generated ink ramp with " + std::to_string(chart.patches.size()) + " patches");

        // Step 2: Measure the chart (or simulate)
        ScanResult measurements;
        if (session.dry_run) {
            measurements = simulate_wf1_measurement(chart, 0.3f);
        } else {
            measurements = scan_wf1_chart(chart);
            if (!measurements.success) {
                result.error_message = measurements.error_message;
                result.status = WF1CalibrationResult::Status::FAILED;
                return result;
            }
        }
        result.warnings.push_back("Measured " + std::to_string(measurements.patches.size()) + " patches");

        // Step 3: Validate measurements are monotonic
        if (!validate_monotonicity(measurements, 0.5f)) {
            result.warnings.push_back("Measurements show non-monotonic behavior");
        }

        // Step 4: Find ink limit
        InkLimitResult ink_limit_result = find_ink_limit(measurements, session.max_ink_limit);
        result.ink_limit = ink_limit_result.ink_limit;
        result.warnings.push_back("Ink limit: " + std::to_string(result.ink_limit));

        // Step 5: Build LUT
        LStarToInkLUT lut = build_lut(measurements, ink_limit_result);
        LUTValidation lut_valid = validate_lut(lut);

        if (!lut_valid.monotonic) {
            result.warnings.push_back("LUT validation: " + lut_valid.error_message);
        }
        result.lut = lut.to_array();

        // Step 6: Verify with test chart
        ChartLayout verify_chart = generate_wf1_verification();
        ScanResult verify_measurements;
        if (session.dry_run) {
            verify_measurements = simulate_wf1_measurement(verify_chart, 0.3f);
        } else {
            verify_measurements = scan_wf1_chart(verify_chart);
        }

        if (!verify_measurements.patches.empty()) {
            float sum_de = 0.0f;
            float max_de = 0.0f;
            int count = 0;

            for (const auto& patch : verify_measurements.patches) {
                float expected_lstar = 100.0f - 50.0f * patch.ink_density;
                float lstar_error = std::abs(patch.measured_lstar - expected_lstar);
                sum_de += lstar_error;
                max_de = std::max(max_de, lstar_error);
                count++;
            }

            result.delta_e_avg = sum_de / count;
            result.delta_e_max = max_de;
            result.verification_passed = (result.delta_e_avg < 0.5f && result.delta_e_max < 1.5f);

            if (result.verification_passed) {
                result.status = WF1CalibrationResult::Status::SUCCESS;
            } else {
                result.status = WF1CalibrationResult::Status::PARTIAL;
                result.warnings.push_back("Verification delta-E: avg=" + std::to_string(result.delta_e_avg) +
                                         ", max=" + std::to_string(result.delta_e_max));
            }
        }

    } catch (const std::exception& e) {
        result.error_message = e.what();
        result.status = WF1CalibrationResult::Status::FAILED;
    }

    return result;
}

}  // namespace workflow
}  // namespace jcolour
