#include <catch2/catch_test_macros.hpp>
#include "g7_validation.h"
#include "../src/profile_library.h"
#include <vector>
#include <chrono>

using namespace jcolour::calibration;
using namespace jcolour::profiles;

TEST_CASE("[g7] G7ValidationReport initializes correctly", "[g7]") {
    G7ValidationReport report;
    G7ReportMetadata meta;
    meta.report_id = "TEST-001";
    meta.generated_at = std::chrono::system_clock::now();
    meta.software_version = "1.0.0";

    report.set_metadata(meta);

    G7CalibrationResult result;
    result.total_patches = 10;
    result.passing_patches = 9;
    result.failing_patches = 1;
    result.average_delta_E = 1.5;
    result.calibration_achieved = false;

    report.set_calibration_result(result);

    REQUIRE(report.get_result().total_patches == 10);
}

TEST_CASE("[g7] G7ComplianceChecker check_conformance", "[g7]") {
    G7CalibrationResult result;
    result.total_patches = 5;
    result.passing_patches = 5;
    result.failing_patches = 0;
    result.average_delta_E = 1.0;

    result.patch_results.push_back({1, "N50", 50.0, 0.0, 0.0, 50.5, 0.1, 0.1, 0.5, true, ""});

    bool conforms = G7ComplianceChecker::check_conformance(result, 3.0);
    REQUIRE(conforms == true);
}

TEST_CASE("[g7] G7ComplianceChecker get_violations", "[g7]") {
    G7CalibrationResult result;
    result.patch_results.push_back({1, "N50", 50.0, 0.0, 0.0, 55.0, 0.0, 0.0, 5.0, false, ""});

    auto violations = G7ComplianceChecker::get_violations(result, 3.0);
    REQUIRE((int)violations.size() >= 1);
}

TEST_CASE("[g7] G7ComplianceChecker determine_compliance_level", "[g7]") {
    G7CalibrationResult result;
    result.average_delta_E = 1.0;
    result.patch_results.push_back({1, "N50", 50.0, 0.0, 0.0, 50.5, 0.1, 0.1, 0.5, true, ""});

    auto level = G7ComplianceChecker::determine_compliance_level(result, 3.0);
    REQUIRE(level == G7ComplianceChecker::ComplianceLevel::Perfect);
}

TEST_CASE("[g7] G7TrendAnalysis add_result and get_trend_data", "[g7]") {
    G7TrendAnalysis analysis;

    G7CalibrationResult result;
    result.average_delta_E = 1.0;
    result.passing_patches = 10;
    result.measurement_date = std::chrono::system_clock::now();

    analysis.add_result(result, "TestProfile");

    auto data = analysis.get_trend_data("TestProfile");
    REQUIRE((int)data.size() == 1);
}

TEST_CASE("[g7] G7TestPattern standard_it8_pattern", "[g7]") {
    auto spec = G7TestPattern::standard_it8_pattern();
    REQUIRE(spec.width_patches == 16);
    REQUIRE(spec.height_patches == 6);
    REQUIRE(spec.include_gray_ramp == true);
}

TEST_CASE("[g7] G7TestPattern get_required_patches", "[g7]") {
    auto spec = G7TestPattern::standard_it8_pattern();
    auto patches = G7TestPattern::get_required_patches(spec);
    REQUIRE((int)patches.size() >= 10);  // Gray ramp + CMYK + gray balance
}

TEST_CASE("[g7] G7Comparator compare", "[g7]") {
    G7CalibrationResult before, after;

    before.patch_results.push_back({1, "N50", 50.0, 0.0, 0.0, 55.0, 0.0, 0.0, 5.0, false, ""});
    after.patch_results.push_back({1, "N50", 50.0, 0.0, 0.0, 52.0, 0.0, 0.0, 2.0, true, ""});

    auto result = G7Comparator::compare(before, after);
    REQUIRE(result.improved_patches.size() == 1);
}

TEST_CASE("[g7] G7ValidationReport format_name", "[g7]") {
    REQUIRE(G7ValidationReport::format_name(ReportFormat::PDF) == "PDF");
    REQUIRE(G7ValidationReport::format_name(ReportFormat::HTML) == "HTML");
    REQUIRE(G7ValidationReport::format_name(ReportFormat::JSON) == "JSON");
}

TEST_CASE("[g7] G7ValidationReport file_extension", "[g7]") {
    REQUIRE(G7ValidationReport::file_extension(ReportFormat::PDF) == ".pdf");
    REQUIRE(G7ValidationReport::file_extension(ReportFormat::HTML) == ".html");
    REQUIRE(G7ValidationReport::file_extension(ReportFormat::JSON) == ".json");
}

TEST_CASE("[g7] ProfileValidator validate", "[g7]") {
    std::vector<uint8_t> data(256, 0);
    auto result = ProfileValidator::validate(data);
    REQUIRE(result.is_valid == true);
}

TEST_CASE("[g7] ProfileValidator validate empty data", "[g7]") {
    std::vector<uint8_t> data;
    auto result = ProfileValidator::validate(data);
    REQUIRE(result.is_valid == false);
}