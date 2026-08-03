#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

namespace jcolour {
namespace calibration {

struct G7Measurement {
    double cyan_density = 0.0;
    double magenta_density = 0.0;
    double yellow_density = 0.0;
    double black_density = 0.0;
    double l_star = 0.0;
    double a_star = 0.0;
    double b_star = 0.0;
    int patch_index = 0;
};

struct G7PatchResult {
    int patch_number = 0;
    std::string patch_name;
    double target_L = 0.0;
    double target_a = 0.0;
    double target_b = 0.0;
    double measured_L = 0.0;
    double measured_a = 0.0;
    double measured_b = 0.0;
    double delta_E = 0.0;
    bool passes = false;
    std::string notes;
};

struct G7CalibrationResult {
    std::string instrument_id;
    std::chrono::system_clock::time_point measurement_date;
    std::string profile_name;
    int total_patches = 0;
    int passing_patches = 0;
    int failing_patches = 0;
    double average_delta_E = 0.0;
    double max_delta_E = 0.0;
    double min_delta_E = 0.0;
    double standard_deviation = 0.0;
    double gray_balance_error = 0.0;
    double tonal_response_error = 0.0;
    bool calibration_achieved = false;
    int iteration_count = 0;
    std::vector<G7PatchResult> patch_results;
};

struct G7ReportMetadata {
    std::string report_id;
    std::chrono::system_clock::time_point generated_at;
    std::string software_version;
    std::string operator_name;
    std::string customer_name;
    std::string notes;
};

enum class ReportFormat { PDF, HTML, JSON, CSV, XML };
enum class ReportTemplate { Standard, Detailed, Summary, Compliance };
enum class ChartType { Histogram, LineChart, BarChart, Scatter, Heatmap };

class G7ValidationReport {
public:
    G7ValidationReport();
    ~G7ValidationReport();

    void set_metadata(const G7ReportMetadata& metadata);
    void set_calibration_result(const G7CalibrationResult& result);

    G7ReportMetadata get_metadata() const { return metadata_; }
    G7CalibrationResult get_result() const { return result_; }

    bool generate(ReportFormat format, const std::string& output_path);
    bool generate_with_template(ReportFormat format, ReportTemplate template_type,
                                const std::string& output_path);

    static std::string format_name(ReportFormat format);
    static std::string file_extension(ReportFormat format);

private:
    G7ReportMetadata metadata_;
    G7CalibrationResult result_;

    bool generate_pdf(const std::string& output_path);
    bool generate_html(const std::string& output_path);
    bool generate_json(const std::string& output_path);
    bool generate_csv(const std::string& output_path);
    bool generate_xml(const std::string& output_path);

    std::string generate_pdf_content();
    std::string generate_html_content();
    std::string generate_json_content();
    std::string generate_csv_content();
    std::string generate_xml_content();

    std::string generate_passfail_bar(int pass, int fail);
    std::string generate_delta_e_chart();
    std::string generate_gray_balance_table();
    std::string generate_patch_summary_table();

    bool write_file(const std::string& path, const std::string& content);
    bool write_binary(const std::string& path, const std::vector<uint8_t>& data);

    double calculate_pass_rate();
    std::string calculate_status();

    std::string time_to_string(std::chrono::system_clock::time_point tp);
    std::string format_double(double value, int precision);
};

class G7ComplianceChecker {
public:
    static bool check_conformance(const G7CalibrationResult& result,
                                  double max_allowed_delta_E = 3.0);

    static std::vector<std::string> get_violations(const G7CalibrationResult& result,
                                                   double tolerance = 3.0);

    static double calculate_gray_balance_score(const G7CalibrationResult& result);

    static double calculate_neutrality_score(const G7CalibrationResult& result,
                                            const std::vector<G7Measurement>& measurements);

    enum class ComplianceLevel { Perfect, Acceptable, Warning, Failed };

    static ComplianceLevel determine_compliance_level(const G7CalibrationResult& result,
                                                     double tolerance = 3.0);

    static std::string compliance_level_text(ComplianceLevel level);
};

class G7ChartGenerator {
public:
    static std::vector<uint8_t> generate_delta_e_histogram(
        const std::vector<double>& delta_e_values,
        int bins = 20);

    static std::vector<uint8_t> generate_gray_balance_chart(
        const std::vector<G7Measurement>& measurements);

    static std::vector<uint8_t> generate_tonal_response_chart(
        const std::vector<double>& target_values,
        const std::vector<double>& measured_values);

    static std::vector<uint8_t> generate_scatter_plot(
        const std::vector<double>& target_values,
        const std::vector<double>& measured_values);

    static std::vector<uint8_t> generate_chart(ChartType type,
                                               const std::vector<double>& data,
                                               int width = 800,
                                               int height = 400);
};

class G7Certification {
public:
    struct Certificate {
        std::string certificate_number;
        std::string issued_to;
        std::chrono::system_clock::time_point issue_date;
        std::chrono::system_clock::time_point expiry_date;
        std::string calibrator_name;
        std::string instrument_serial;
        std::string profile_reference;
        double certification_score = 0.0;
        bool is_valid = false;
    };

    static Certificate generate_certificate(const G7CalibrationResult& result,
                                           const std::string& issued_to,
                                           const std::string& calibrator_name);

    static bool verify_certificate(const Certificate& cert);

    static bool is_certificate_expired(const Certificate& cert);

    static std::string certificate_as_pdf(const Certificate& cert);
};

class G7TrendAnalysis {
public:
    struct TrendPoint {
        std::chrono::system_clock::time_point date;
        double avg_delta_e = 0.0;
        int patches_passed = 0;
        std::string profile;
    };

    G7TrendAnalysis();

    void add_result(const G7CalibrationResult& result, const std::string& profile);

    std::vector<TrendPoint> get_trend_data(const std::string& profile);

    bool has_degradation(const std::string& profile, double threshold_delta = 0.5);

    std::string generate_trend_report();

private:
    std::vector<TrendPoint> historical_data_;
};

class G7TestPattern {
public:
    struct PatternSpec {
        int width_patches = 16;
        int height_patches = 6;
        double patch_size_mm = 10.0;
        double spacing_mm = 2.0;
        bool include_gray_ramp = true;
        bool include_gray_balance = true;
    };

    static PatternSpec standard_it8_pattern();
    static PatternSpec extended_g7_pattern();
    static PatternSpec quick_verification_pattern();

    static std::vector<std::string> get_required_patches(const PatternSpec& spec);
};

class G7Comparator {
public:
    struct ComparisonResult {
        double delta_e_difference = 0.0;
        std::vector<int> improved_patches;
        std::vector<int> degraded_patches;
        double avg_improvement = 0.0;
        double avg_degradation = 0.0;
    };

    static ComparisonResult compare(const G7CalibrationResult& before,
                                    const G7CalibrationResult& after);
};

}
}