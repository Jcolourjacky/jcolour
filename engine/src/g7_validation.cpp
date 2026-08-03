#include "g7_validation.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <random>

namespace jcolour {
namespace calibration {

// Helper functions
static std::string time_to_iso_string(std::chrono::system_clock::time_point tp) {
    time_t time = std::chrono::system_clock::to_time_t(tp);
    tm tm_buf;
    localtime_r(&time, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

static std::string format_num(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

static std::string format_double(double value, int precision) {
    return format_num(value, precision);
}

static std::string join_strings(const std::vector<std::string>& vec, const std::string& delim) {
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << delim;
        oss << vec[i];
    }
    return oss.str();
}

// G7ValidationReport implementation
G7ValidationReport::G7ValidationReport() {}

G7ValidationReport::~G7ValidationReport() {}

void G7ValidationReport::set_metadata(const G7ReportMetadata& metadata) {
    metadata_ = metadata;
}

void G7ValidationReport::set_calibration_result(const G7CalibrationResult& result) {
    result_ = result;
}

std::string G7ValidationReport::format_name(ReportFormat format) {
    switch (format) {
        case ReportFormat::PDF: return "PDF";
        case ReportFormat::HTML: return "HTML";
        case ReportFormat::JSON: return "JSON";
        case ReportFormat::CSV: return "CSV";
        case ReportFormat::XML: return "XML";
        default: return "Unknown";
    }
}

std::string G7ValidationReport::file_extension(ReportFormat format) {
    switch (format) {
        case ReportFormat::PDF: return ".pdf";
        case ReportFormat::HTML: return ".html";
        case ReportFormat::JSON: return ".json";
        case ReportFormat::CSV: return ".csv";
        case ReportFormat::XML: return ".xml";
        default: return ".txt";
    }
}

bool G7ValidationReport::generate(ReportFormat format, const std::string& output_path) {
    switch (format) {
        case ReportFormat::PDF: return generate_pdf(output_path);
        case ReportFormat::HTML: return generate_html(output_path);
        case ReportFormat::JSON: return generate_json(output_path);
        case ReportFormat::CSV: return generate_csv(output_path);
        case ReportFormat::XML: return generate_xml(output_path);
        default: return false;
    }
}

bool G7ValidationReport::generate_with_template(ReportFormat format,
                                                ReportTemplate template_type,
                                                const std::string& output_path) {
    (void)template_type;  // Reserved for future use
    return generate(format, output_path);
}

std::string G7ValidationReport::time_to_string(std::chrono::system_clock::time_point tp) {
    return time_to_iso_string(tp);
}

std::string G7ValidationReport::format_double(double value, int precision) {
    return format_num(value, precision);
}

std::string G7ValidationReport::generate_html_content() {
    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>G7 Calibration Validation Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        h1 { color: #333; border-bottom: 2px solid #333; }
        h2 { color: #555; margin-top: 30px; }
        .meta { background: #f5f5f5; padding: 15px; border-radius: 5px; }
        .meta th { text-align: left; width: 200px; }
        .summary { display: flex; gap: 20px; margin: 20px 0; }
        .stat-box { background: #e8f4f8; padding: 20px; border-radius: 5px; flex: 1; }
        .stat-box h3 { margin: 0 0 10px 0; }
        .stat-box .value { font-size: 24px; font-weight: bold; }
        .pass { color: green; }
        .fail { color: red; }
        table { border-collapse: collapse; width: 100%; margin: 20px 0; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: center; }
        th { background: #333; color: white; }
        tr:nth-child(even) { background: #f9f9f9; }
        .bar-chart { margin: 20px 0; }
    </style>
</head>
<body>
    <h1>G7 Calibration Validation Report</h1>

    <div class="meta">
        <h2>Report Information</h2>
        <table>
            <tr><th>Report ID:</th><td>)" << metadata_.report_id << R"(</td></tr>
            <tr><th>Generated:</th><td>)" << time_to_string(metadata_.generated_at) << R"(</td></tr>
            <tr><th>Software:</th><td>)" << metadata_.software_version << R"(</td></tr>
            <tr><th>Operator:</th><td>)" << metadata_.operator_name << R"(</td></tr>
            <tr><th>Customer:</th><td>)" << metadata_.customer_name << R"(</td></tr>
        </table>
    </div>

    <h2>Summary</h2>
    <div class="summary">
        <div class="stat-box">
            <h3>Total Patches</h3>
            <div class="value">)" << result_.total_patches << R"(</div>
        </div>
        <div class="stat-box">
            <h3>Passing</h3>
            <div class="value pass">)" << result_.passing_patches << R"(</div>
        </div>
        <div class="stat-box">
            <h3>Failing</h3>
            <div class="value fail">)" << result_.failing_patches << R"(</div>
        </div>
        <div class="stat-box">
            <h3>Pass Rate</h3>
            <div class="value">)" << format_double(calculate_pass_rate(), 1) << R"(%</div>
        </div>
    </div>

    <div class="summary">
        <div class="stat-box">
            <h3>Avg dE</h3>
            <div class="value">)" << format_double(result_.average_delta_E, 2) << R"(</div>
        </div>
        <div class="stat-box">
            <h3>Max dE</h3>
            <div class="value">)" << format_double(result_.max_delta_E, 2) << R"(</div>
        </div>
        <div class="stat-box">
            <h3>Gray Balance Error</h3>
            <div class="value">)" << format_double(result_.gray_balance_error, 2) << R"(</div>
        </div>
        <div class="stat-box">
            <h3>Status</h3>
            <div class="value )" << (result_.calibration_achieved ? "pass" : "fail") << R"(">)" << calculate_status() << R"(</div>
        </div>
    </div>

    <h2>Pass/Fail Summary</h2>
)" << generate_passfail_bar(result_.passing_patches, result_.failing_patches) << R"(

    <h2>Patch Results</h2>
)" << generate_patch_summary_table() << R"(

    <h2>Gray Balance Analysis</h2>
)" << generate_gray_balance_table() << R"(

    <h2>dE Distribution</h2>
)" << generate_delta_e_chart() << R"(

    <footer style="margin-top: 50px; color: #999; font-size: 12px;">
        Generated by jColour - G7 Calibration Validation Report
    </footer>
</body>
</html>)";
    return html.str();
}

std::string G7ValidationReport::generate_passfail_bar(int pass, int fail) {
    int total = pass + fail;
    if (total == 0) total = 1;

    double pass_pct = 100.0 * pass / total;
    double fail_pct = 100.0 * fail / total;

    std::ostringstream bar;
    bar << R"(<div class="bar-chart">
        <div style="width: 100%; height: 30px; background: #f0f0f0; border-radius: 5px; overflow: hidden;">
            <div style="width: )" << format_double(pass_pct, 1) << R"(%; height: 100%; background: #4CAF50; display: inline-block;"></div>
            <div style="width: )" << format_double(fail_pct, 1) << R"(%; height: 100%; background: #f44336; display: inline-block;"></div>
        </div>
        <div style="display: flex; justify-content: space-between; margin-top: 5px;">
            <span><span style="color: #4CAF50;">●</span> Pass: )" << pass << R"(</span>
            <span><span style="color: #f44336;">●</span> Fail: )" << fail << R"(</span>
        </div>
    </div>)";
    return bar.str();
}

std::string G7ValidationReport::generate_delta_e_chart() {
    int bins = 20;
    std::vector<int> histogram(bins, 0);

    for (const auto& patch : result_.patch_results) {
        int bin = std::min(bins - 1, static_cast<int>(patch.delta_E));
        if (bin >= 0) histogram[bin]++;
    }

    std::ostringstream table;
    table << "<div>\n<table style=\"width: auto;\">\n";
    table << "<tr><th>dE Range</th><th>Count</th></tr>\n";

    int sum_5plus = 0;
    for (int i = 5; i < bins; ++i) sum_5plus += histogram[i];

    table << "<tr><td>0-1</td><td>" << histogram[0] << "</td></tr>\n";
    table << "<tr><td>1-2</td><td>" << histogram[1] << "</td></tr>\n";
    table << "<tr><td>2-3</td><td>" << histogram[2] << "</td></tr>\n";
    table << "<tr><td>3-5</td><td>" << (histogram[3] + histogram[4]) << "</td></tr>\n";
    table << "<tr><td>>5</td><td>" << sum_5plus << "</td></tr>\n";
    table << "</table>\n</div>\n";

    return table.str();
}

std::string G7ValidationReport::generate_gray_balance_table() {
    std::ostringstream table;
    table << "<table>\n";
    table << "<tr><th>Patch</th><th>Target L*a*b*</th><th>Measured L*a*b*</th><th>dE</th><th>Status</th></tr>\n";

    for (const auto& patch : result_.patch_results) {
        if (patch.patch_name.find("N") != std::string::npos ||
            patch.patch_name.find("Gray") != std::string::npos) {
            table << "<tr>\n";
            table << "<td>" << patch.patch_name << "</td>\n";
            table << "<td>" << format_double(patch.target_L, 1) << ", " << format_double(patch.target_a, 1)
                  << ", " << format_double(patch.target_b, 1) << "</td>\n";
            table << "<td>" << format_double(patch.measured_L, 1) << ", " << format_double(patch.measured_a, 1)
                  << ", " << format_double(patch.measured_b, 1) << "</td>\n";
            table << "<td>" << format_double(patch.delta_E, 2) << "</td>\n";
            table << "<td class=\"" << (patch.passes ? "pass" : "fail") << "\">"
                  << (patch.passes ? "PASS" : "FAIL") << "</td>\n";
            table << "</tr>\n";
        }
    }
    table << "</table>\n";
    return table.str();
}

std::string G7ValidationReport::generate_patch_summary_table() {
    std::ostringstream table;
    table << "<table>\n";
    table << "<tr>\n";
    table << "<th>#</th><th>Name</th>\n";
    table << "<th>Target L*</th><th>Measured L*</th>\n";
    table << "<th>Target a*</th><th>Measured a*</th>\n";
    table << "<th>Target b*</th><th>Measured b*</th>\n";
    table << "<th>dE</th><th>Status</th>\n";
    table << "</tr>\n";

    for (const auto& patch : result_.patch_results) {
        table << "<tr>\n";
        table << "<td>" << patch.patch_number << "</td>\n";
        table << "<td>" << patch.patch_name << "</td>\n";
        table << "<td>" << format_double(patch.target_L, 2) << "</td>\n";
        table << "<td>" << format_double(patch.measured_L, 2) << "</td>\n";
        table << "<td>" << format_double(patch.target_a, 2) << "</td>\n";
        table << "<td>" << format_double(patch.measured_a, 2) << "</td>\n";
        table << "<td>" << format_double(patch.target_b, 2) << "</td>\n";
        table << "<td>" << format_double(patch.measured_b, 2) << "</td>\n";
        table << "<td>" << format_double(patch.delta_E, 2) << "</td>\n";
        table << "<td class=\"" << (patch.passes ? "pass" : "fail") << "\">"
              << (patch.passes ? "PASS" : "FAIL") << "</td>\n";
        table << "</tr>\n";
    }
    table << "</table>\n";
    return table.str();
}

std::string G7ValidationReport::generate_json_content() {
    std::ostringstream json;
    json << "{\n";
    json << "  \"report_id\": \"" << metadata_.report_id << "\",\n";
    json << "  \"generated_at\": \"" << time_to_string(metadata_.generated_at) << "\",\n";
    json << "  \"software_version\": \"" << metadata_.software_version << "\",\n";
    json << "  \"operator\": \"" << metadata_.operator_name << "\",\n";
    json << "  \"customer\": \"" << metadata_.customer_name << "\",\n";
    json << "  \"notes\": \"" << metadata_.notes << "\",\n";
    json << "  \"calibration_result\": {\n";
    json << "    \"instrument_id\": \"" << result_.instrument_id << "\",\n";
    json << "    \"measurement_date\": \"" << time_to_string(result_.measurement_date) << "\",\n";
    json << "    \"profile_name\": \"" << result_.profile_name << "\",\n";
    json << "    \"total_patches\": " << result_.total_patches << ",\n";
    json << "    \"passing_patches\": " << result_.passing_patches << ",\n";
    json << "    \"failing_patches\": " << result_.failing_patches << ",\n";
    json << "    \"average_delta_E\": " << result_.average_delta_E << ",\n";
    json << "    \"max_delta_E\": " << result_.max_delta_E << ",\n";
    json << "    \"min_delta_E\": " << result_.min_delta_E << ",\n";
    json << "    \"standard_deviation\": " << result_.standard_deviation << ",\n";
    json << "    \"gray_balance_error\": " << result_.gray_balance_error << ",\n";
    json << "    \"calibration_achieved\": " << (result_.calibration_achieved ? "true" : "false") << ",\n";
    json << "    \"iteration_count\": " << result_.iteration_count << ",\n";
    json << "    \"patches\": [\n";

    for (size_t i = 0; i < result_.patch_results.size(); ++i) {
        const auto& patch = result_.patch_results[i];
        json << "      {\n";
        json << "        \"patch_number\": " << patch.patch_number << ",\n";
        json << "        \"patch_name\": \"" << patch.patch_name << "\",\n";
        json << "        \"target_L\": " << patch.target_L << ",\n";
        json << "        \"measured_L\": " << patch.measured_L << ",\n";
        json << "        \"target_a\": " << patch.target_a << ",\n";
        json << "        \"measured_a\": " << patch.measured_a << ",\n";
        json << "        \"target_b\": " << patch.target_b << ",\n";
        json << "        \"measured_b\": " << patch.measured_b << ",\n";
        json << "        \"delta_E\": " << patch.delta_E << ",\n";
        json << "        \"passes\": " << (patch.passes ? "true" : "false") << ",\n";
        json << "        \"notes\": \"" << patch.notes << "\"\n";
        json << "      }";
        if (i < result_.patch_results.size() - 1) json << ",";
        json << "\n";
    }

    json << "    ]\n";
    json << "  },\n";
    json << "  \"pass_rate\": " << format_double(calculate_pass_rate(), 2) << ",\n";
    json << "  \"status\": \"" << calculate_status() << "\"\n";
    json << "}\n";
    return json.str();
}

std::string G7ValidationReport::generate_csv_content() {
    std::ostringstream csv;
    csv << "Patch,Name,Target_L,Measured_L,Target_a,Measured_a,Target_b,Measured_b,Delta_E,Passes,Notes\n";

    for (const auto& patch : result_.patch_results) {
        csv << patch.patch_number << ","
            << patch.patch_name << ","
            << format_double(patch.target_L, 3) << ","
            << format_double(patch.measured_L, 3) << ","
            << format_double(patch.target_a, 3) << ","
            << format_double(patch.measured_a, 3) << ","
            << format_double(patch.target_b, 3) << ","
            << format_double(patch.measured_b, 3) << ","
            << format_double(patch.delta_E, 3) << ","
            << (patch.passes ? "YES" : "NO") << ","
            << patch.notes << "\n";
    }
    return csv.str();
}

std::string G7ValidationReport::generate_xml_content() {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<G7ValidationReport xmlns=\"http://jcolour.app/g7\">\n";
    xml << "  <Metadata>\n";
    xml << "    <ReportID>" << metadata_.report_id << "</ReportID>\n";
    xml << "    <GeneratedAt>" << time_to_string(metadata_.generated_at) << "</GeneratedAt>\n";
    xml << "    <SoftwareVersion>" << metadata_.software_version << "</SoftwareVersion>\n";
    xml << "    <Operator>" << metadata_.operator_name << "</Operator>\n";
    xml << "    <Customer>" << metadata_.customer_name << "</Customer>\n";
    xml << "    <Notes>" << metadata_.notes << "</Notes>\n";
    xml << "  </Metadata>\n";
    xml << "  <CalibrationResult>\n";
    xml << "    <InstrumentID>" << result_.instrument_id << "</InstrumentID>\n";
    xml << "    <MeasurementDate>" << time_to_string(result_.measurement_date) << "</MeasurementDate>\n";
    xml << "    <ProfileName>" << result_.profile_name << "</ProfileName>\n";
    xml << "    <TotalPatches>" << result_.total_patches << "</TotalPatches>\n";
    xml << "    <PassingPatches>" << result_.passing_patches << "</PassingPatches>\n";
    xml << "    <FailingPatches>" << result_.failing_patches << "</FailingPatches>\n";
    xml << "    <AverageDeltaE>" << format_double(result_.average_delta_E, 4) << "</AverageDeltaE>\n";
    xml << "    <MaxDeltaE>" << format_double(result_.max_delta_E, 4) << "</MaxDeltaE>\n";
    xml << "    <MinDeltaE>" << format_double(result_.min_delta_E, 4) << "</MinDeltaE>\n";
    xml << "    <StandardDeviation>" << format_double(result_.standard_deviation, 4) << "</StandardDeviation>\n";
    xml << "    <GrayBalanceError>" << format_double(result_.gray_balance_error, 4) << "</GrayBalanceError>\n";
    xml << "    <CalibrationAchieved>" << (result_.calibration_achieved ? "true" : "false") << "</CalibrationAchieved>\n";
    xml << "    <IterationCount>" << result_.iteration_count << "</IterationCount>\n";
    xml << "    <PassRate>" << format_double(calculate_pass_rate(), 2) << "</PassRate>\n";
    xml << "    <Status>" << calculate_status() << "</Status>\n";
    xml << "  </CalibrationResult>\n";
    xml << "  <PatchResults>\n";

    for (const auto& patch : result_.patch_results) {
        xml << "    <Patch>\n";
        xml << "      <PatchNumber>" << patch.patch_number << "</PatchNumber>\n";
        xml << "      <PatchName>" << patch.patch_name << "</PatchName>\n";
        xml << "      <Target>\n";
        xml << "        <L>" << format_double(patch.target_L, 4) << "</L>\n";
        xml << "        <a>" << format_double(patch.target_a, 4) << "</a>\n";
        xml << "        <b>" << format_double(patch.target_b, 4) << "</b>\n";
        xml << "      </Target>\n";
        xml << "      <Measured>\n";
        xml << "        <L>" << format_double(patch.measured_L, 4) << "</L>\n";
        xml << "        <a>" << format_double(patch.measured_a, 4) << "</a>\n";
        xml << "        <b>" << format_double(patch.measured_b, 4) << "</b>\n";
        xml << "      </Measured>\n";
        xml << "      <DeltaE>" << format_double(patch.delta_E, 4) << "</DeltaE>\n";
        xml << "      <Passes>" << (patch.passes ? "true" : "false") << "</Passes>\n";
        xml << "      <Notes>" << patch.notes << "</Notes>\n";
        xml << "    </Patch>\n";
    }

    xml << "  </PatchResults>\n";
    xml << "</G7ValidationReport>\n";
    return xml.str();
}

std::string G7ValidationReport::generate_pdf_content() {
    return generate_html_content();
}

bool G7ValidationReport::generate_pdf(const std::string& output_path) {
    std::string html = generate_html_content();
    return write_file(output_path, html);
}

bool G7ValidationReport::generate_html(const std::string& output_path) {
    return write_file(output_path, generate_html_content());
}

bool G7ValidationReport::generate_json(const std::string& output_path) {
    return write_file(output_path, generate_json_content());
}

bool G7ValidationReport::generate_csv(const std::string& output_path) {
    return write_file(output_path, generate_csv_content());
}

bool G7ValidationReport::generate_xml(const std::string& output_path) {
    return write_file(output_path, generate_xml_content());
}

bool G7ValidationReport::write_file(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    if (!out) return false;
    out << content;
    out.close();
    return true;
}

bool G7ValidationReport::write_binary(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    out.close();
    return true;
}

double G7ValidationReport::calculate_pass_rate() {
    if (result_.total_patches == 0) return 0.0;
    return 100.0 * result_.passing_patches / result_.total_patches;
}

std::string G7ValidationReport::calculate_status() {
    return result_.calibration_achieved ? "CALIBRATION ACHIEVED" : "CALIBRATION FAILED";
}

// G7ComplianceChecker implementation
bool G7ComplianceChecker::check_conformance(const G7CalibrationResult& result,
                                            double max_allowed_delta_E) {
    if (result.failing_patches > 0) return false;

    for (const auto& patch : result.patch_results) {
        if (patch.delta_E > max_allowed_delta_E) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> G7ComplianceChecker::get_violations(const G7CalibrationResult& result,
                                                             double tolerance) {
    std::vector<std::string> violations;

    for (const auto& patch : result.patch_results) {
        if (patch.delta_E > tolerance) {
            std::ostringstream oss;
            oss << "Patch " << patch.patch_number << " (" << patch.patch_name << "): dE = "
                << format_double(patch.delta_E, 2) << " exceeds tolerance of " << tolerance;
            violations.push_back(oss.str());
        }
    }
    return violations;
}

double G7ComplianceChecker::calculate_gray_balance_score(const G7CalibrationResult& result) {
    return 1.0 - std::min(1.0, result.gray_balance_error / 10.0);
}

double G7ComplianceChecker::calculate_neutrality_score(const G7CalibrationResult& result,
                                                       const std::vector<G7Measurement>& measurements) {
    if (measurements.empty()) return 1.0;

    double total_deviation = 0.0;
    for (const auto& m : measurements) {
        double deviation = std::sqrt(m.a_star * m.a_star + m.b_star * m.b_star);
        total_deviation += deviation;
    }

    double avg_deviation = total_deviation / measurements.size();
    return std::max(0.0, 1.0 - avg_deviation / 20.0);
}

G7ComplianceChecker::ComplianceLevel G7ComplianceChecker::determine_compliance_level(
    const G7CalibrationResult& result, double tolerance) {

    if (check_conformance(result, tolerance * 0.5)) {
        return ComplianceLevel::Perfect;
    }
    if (check_conformance(result, tolerance)) {
        return ComplianceLevel::Acceptable;
    }
    if (result.average_delta_E < tolerance * 2) {
        return ComplianceLevel::Warning;
    }
    return ComplianceLevel::Failed;
}

std::string G7ComplianceChecker::compliance_level_text(ComplianceLevel level) {
    switch (level) {
        case ComplianceLevel::Perfect: return "PERFECT";
        case ComplianceLevel::Acceptable: return "ACCEPTABLE";
        case ComplianceLevel::Warning: return "WARNING";
        case ComplianceLevel::Failed: return "FAILED";
        default: return "UNKNOWN";
    }
}

// G7ChartGenerator implementation
std::vector<uint8_t> G7ChartGenerator::generate_delta_e_histogram(
    const std::vector<double>& delta_e_values, int bins) {

    std::vector<int> histogram(bins, 0);
    for (double v : delta_e_values) {
        int bin = std::min(bins - 1, static_cast<int>(v));
        if (bin >= 0) histogram[bin]++;
    }

    std::ostringstream data;
    for (int i = 0; i < bins; ++i) {
        data << i << ":" << histogram[i] << "\n";
    }
    std::string str = data.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::vector<uint8_t> G7ChartGenerator::generate_gray_balance_chart(
    const std::vector<G7Measurement>& measurements) {

    std::ostringstream data;
    data << "L,a,b\n";
    for (const auto& m : measurements) {
        data << format_double(m.l_star, 2) << ","
             << format_double(m.a_star, 2) << ","
             << format_double(m.b_star, 2) << "\n";
    }
    std::string str = data.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::vector<uint8_t> G7ChartGenerator::generate_tonal_response_chart(
    const std::vector<double>& target_values,
    const std::vector<double>& measured_values) {

    std::ostringstream data;
    data << "Target,Measured\n";
    int n = static_cast<int>(std::min(target_values.size(), measured_values.size()));
    for (int i = 0; i < n; ++i) {
        data << format_double(target_values[i], 3) << ","
             << format_double(measured_values[i], 3) << "\n";
    }
    std::string str = data.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::vector<uint8_t> G7ChartGenerator::generate_scatter_plot(
    const std::vector<double>& target_values,
    const std::vector<double>& measured_values) {

    return generate_tonal_response_chart(target_values, measured_values);
}

std::vector<uint8_t> G7ChartGenerator::generate_chart(ChartType type,
                                                       const std::vector<double>& data,
                                                       int width, int height) {
    (void)type;
    (void)width;
    (void)height;

    std::ostringstream result;
    result << "Chart Data:\n";
    for (double v : data) {
        result << format_double(v, 3) << "\n";
    }
    std::string str = result.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

// G7Certification implementation
static std::string generate_cert_number() {
    auto now = std::chrono::system_clock::now();
    time_t time = std::chrono::system_clock::to_time_t(now);
    tm tm_buf;
    localtime_r(&time, &tm_buf);

    std::ostringstream oss;
    oss << "G7-";
    oss << std::put_time(&tm_buf, "%Y%m%d");
    oss << "-";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 9999);
    oss << std::setw(4) << std::setfill('0') << dist(gen);

    return oss.str();
}

G7Certification::Certificate G7Certification::generate_certificate(
    const G7CalibrationResult& result,
    const std::string& issued_to,
    const std::string& calibrator_name) {

    Certificate cert;
    cert.certificate_number = generate_cert_number();
    cert.issued_to = issued_to;
    cert.issue_date = std::chrono::system_clock::now();
    cert.expiry_date = cert.issue_date + std::chrono::hours(24 * 365);  // 1 year
    cert.calibrator_name = calibrator_name;
    cert.instrument_serial = result.instrument_id;
    cert.profile_reference = result.profile_name;
    cert.certification_score = result.average_delta_E;
    cert.is_valid = result.calibration_achieved;

    return cert;
}

bool G7Certification::verify_certificate(const Certificate& cert) {
    return cert.is_valid && !is_certificate_expired(cert);
}

bool G7Certification::is_certificate_expired(const Certificate& cert) {
    return cert.expiry_date < std::chrono::system_clock::now();
}

std::string G7Certification::certificate_as_pdf(const Certificate& cert) {
    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html>
<head><title>G7 Certificate</title></head>
<body>
<h1>G7 CALIBRATION CERTIFICATE</h1>
<table>
<tr><td>Certificate #:</td><td>)" << cert.certificate_number << R"(</td></tr>
<tr><td>Issued To:</td><td>)" << cert.issued_to << R"(</td></tr>
<tr><td>Issue Date:</td><td>)" << time_to_iso_string(cert.issue_date) << R"(</td></tr>
<tr><td>Expiry Date:</td><td>)" << time_to_iso_string(cert.expiry_date) << R"(</td></tr>
<tr><td>Calibrator:</td><td>)" << cert.calibrator_name << R"(</td></tr>
<tr><td>Instrument:</td><td>)" << cert.instrument_serial << R"(</td></tr>
<tr><td>Profile:</td><td>)" << cert.profile_reference << R"(</td></tr>
<tr><td>Score:</td><td>)" << format_double(cert.certification_score, 2) << R"( dE</td></tr>
</table>
</body></html>)";
    return html.str();
}

// G7TrendAnalysis implementation
G7TrendAnalysis::G7TrendAnalysis() {}

void G7TrendAnalysis::add_result(const G7CalibrationResult& result, const std::string& profile) {
    TrendPoint point;
    point.date = result.measurement_date;
    point.avg_delta_e = result.average_delta_E;
    point.patches_passed = result.passing_patches;
    point.profile = profile;
    historical_data_.push_back(point);
}

std::vector<G7TrendAnalysis::TrendPoint> G7TrendAnalysis::get_trend_data(const std::string& profile) {
    std::vector<TrendPoint> filtered;
    for (const auto& point : historical_data_) {
        if (point.profile == profile) {
            filtered.push_back(point);
        }
    }
    return filtered;
}

bool G7TrendAnalysis::has_degradation(const std::string& profile, double threshold_delta) {
    auto data = get_trend_data(profile);
    if (data.size() < 2) return false;

    double recent = data.back().avg_delta_e;
    double previous = data[data.size() - 2].avg_delta_e;
    return (recent - previous) > threshold_delta;
}

std::string G7TrendAnalysis::generate_trend_report() {
    std::ostringstream report;
    report << "G7 Calibration Trend Report\n";
    report << "========================================\n\n";

    for (const auto& point : historical_data_) {
        report << time_to_iso_string(point.date) << " | "
               << point.profile << " | dE: " << format_double(point.avg_delta_e, 2)
               << " | Pass: " << point.patches_passed << "/" << point.patches_passed << "\n";
    }
    return report.str();
}

// G7TestPattern implementation
G7TestPattern::PatternSpec G7TestPattern::standard_it8_pattern() {
    PatternSpec spec;
    spec.width_patches = 16;
    spec.height_patches = 6;
    spec.patch_size_mm = 10.0;
    spec.spacing_mm = 2.0;
    spec.include_gray_ramp = true;
    spec.include_gray_balance = true;
    return spec;
}

G7TestPattern::PatternSpec G7TestPattern::extended_g7_pattern() {
    PatternSpec spec = standard_it8_pattern();
    spec.width_patches = 24;
    spec.height_patches = 8;
    return spec;
}

G7TestPattern::PatternSpec G7TestPattern::quick_verification_pattern() {
    PatternSpec spec;
    spec.width_patches = 8;
    spec.height_patches = 4;
    spec.patch_size_mm = 15.0;
    spec.spacing_mm = 3.0;
    spec.include_gray_ramp = true;
    spec.include_gray_balance = true;
    return spec;
}

std::vector<std::string> G7TestPattern::get_required_patches(const PatternSpec& spec) {
    (void)spec;
    std::vector<std::string> patches;

    // Gray ramp patches
    for (int i = 0; i <= 10; ++i) {
        patches.push_back("N" + std::to_string(i));
    }

    // CMYK patches
    const char* primaries[] = {"C", "M", "Y", "K"};
    for (const char* p : primaries) {
        for (int i = 25; i <= 100; i += 25) {
            patches.push_back(std::string(p) + std::to_string(i));
        }
    }

    // Gray balance patches
    for (int i = 25; i <= 75; i += 25) {
        patches.push_back("G" + std::to_string(i));
    }

    return patches;
}

// G7Comparator implementation
G7Comparator::ComparisonResult G7Comparator::compare(
    const G7CalibrationResult& before,
    const G7CalibrationResult& after) {

    ComparisonResult result;
    size_t n = std::min(before.patch_results.size(), after.patch_results.size());

    for (size_t i = 0; i < n; ++i) {
        double before_de = before.patch_results[i].delta_E;
        double after_de = after.patch_results[i].delta_E;
        double diff = before_de - after_de;

        result.delta_e_difference += diff;

        if (diff > 0.1) {
            result.improved_patches.push_back(static_cast<int>(i));
            result.avg_improvement += diff;
        } else if (diff < -0.1) {
            result.degraded_patches.push_back(static_cast<int>(i));
            result.avg_degradation += std::abs(diff);
        }
    }

    if (!result.improved_patches.empty()) {
        result.avg_improvement /= result.improved_patches.size();
    }
    if (!result.degraded_patches.empty()) {
        result.avg_degradation /= result.degraded_patches.size();
    }

    return result;
}

}
}