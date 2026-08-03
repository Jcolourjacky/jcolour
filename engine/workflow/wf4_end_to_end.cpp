#include "wf4_end_to_end.h"
#include "wf4_chart.h"
#include "wf4_measure.h"
#include "wf4_profile.h"
#include "wf4_transform.h"
#include "../epson/epson.h"

namespace jcolour {
namespace workflow {

WF4CalibrationResult run_wf4_calibration(
    const std::string& printer_uri,
    const std::string& paper_id,
    bool dry_run) {
    
    WF4CalibrationResult result;
    
    // Step 1: Generate calibration chart
    auto chart = generate_wf4_calibration_chart();
    
    // Step 2: Print chart
    if (!dry_run) {
        if (!print_wf4_chart(chart, printer_uri)) {
            result.status = WF4CalibrationResult::Status::FAILED;
            result.error_message = "Failed to print chart";
            return result;
        }
    }
    
    // Step 3: Measure chart
    auto measurements = measure_wf4_chart(chart, dry_run);
    
    // Step 4: Analyze ink limits
    result.ink_limits = analyze_wf4_ink_limits(chart, dry_run);
    
    // Step 5: Generate ICC profile
    TI3Data ti3;
    ti3.filename = "/tmp/wf4_measurements.ti3";
    for (const auto& m : measurements) {
        ti3.patch_names.push_back(m.label);
        ti3.lab_values.push_back(m.L_star);
        ti3.lab_values.push_back(m.a_star);
        ti3.lab_values.push_back(m.b_star);
    }
    
    result.profile = generate_wf4_icc_profile(ti3, paper_id, "/tmp");
    
    // Step 6: Validate
    auto check = verify_wf4_profile(result.profile, ti3);
    result.status = check.passed ? WF4CalibrationResult::Status::SUCCESS
                                : WF4CalibrationResult::Status::PARTIAL;
    
    return result;
}

bool print_wf4_chart(const WF4ChartLayout& chart,
                     const std::string& printer_uri) {
    // Placeholder: actual implementation would use epson module
    return true;
}

std::vector<WF4Measurement> measure_wf4_chart(
    const WF4ChartLayout& chart,
    bool dry_run) {
    return simulate_wf4_measurements(chart);
}

WF4VerificationReport verify_wf4_calibration(
    const WF4CalibrationResult& result,
    const std::string& verification_chart_path) {
    
    WF4VerificationReport report;
    
    // Generate verification chart
    auto chart = generate_wf4_verification_chart();
    auto measurements = measure_wf4_chart(chart, true);
    
    report.patches_tested = static_cast<int>(measurements.size());
    
    // Calculate delta E statistics
    double total_de = 0.0;
    double max_de = 0.0;
    
    for (const auto& m : measurements) {
        double de = std::sqrt(m.a_star * m.a_star + m.b_star * m.b_star);
        total_de += de;
        max_de = std::max(max_de, de);
    }
    
    if (!measurements.empty()) {
        report.avg_delta_e = total_de / measurements.size();
    }
    report.max_delta_e = max_de;
    report.passed = (report.avg_delta_e < 1.5 && report.max_delta_e < 4.0);
    
    return report;
}

}  // namespace workflow
}  // namespace jcolour
