#include <catch2/catch_test_macros.hpp>
#include "wf1_ink_limit.h"
#include "wf1_chart.h"
#include "wf1_measure.h"

using namespace jcolour::workflow;

TEST_CASE("find_ink_limit: works with simulated data", "[wf1][ink_limit]") {
    auto chart = generate_wf1_ink_ramp();
    auto measurements = simulate_wf1_measurement(chart, 0.0f);

    auto result = find_ink_limit(measurements);

    REQUIRE(result.success == true);
    REQUIRE(result.ink_limit > 0.0f);
    REQUIRE(result.ink_limit <= 1.0f);
    REQUIRE(result.saturation_point >= result.ink_limit);
}

TEST_CASE("find_ink_limit: applies safety margin", "[wf1][ink_limit]") {
    auto chart = generate_wf1_ink_ramp();
    auto measurements = simulate_wf1_measurement(chart, 0.0f);

    auto result = find_ink_limit(measurements, 0.10f);

    REQUIRE(result.success == true);
    REQUIRE(result.ink_limit < result.saturation_point);
}

TEST_CASE("find_ink_limit: handles ideal paper", "[wf1][ink_limit]") {
    // For ideal paper, ink_limit should be around 0.85 ± 0.1
    auto chart = generate_wf1_ink_ramp();
    auto measurements = simulate_wf1_measurement(chart, 0.0f);

    auto result = find_ink_limit(measurements);

    REQUIRE(result.ink_limit > 0.7f);   // Not too conservative
    REQUIRE(result.ink_limit < 1.0f);   // Not exceeding 100%
}

TEST_CASE("find_ink_limit: fails on invalid measurements", "[wf1][ink_limit]") {
    ScanResult bad_measurements;
    bad_measurements.success = false;
    bad_measurements.error_message = "No data";

    auto result = find_ink_limit(bad_measurements);

    REQUIRE(result.success == false);
    REQUIRE(!result.error_message.empty());
}

TEST_CASE("find_ink_limit: fails with too few measurements", "[wf1][ink_limit]") {
    ScanResult result;
    result.success = true;
    result.patches.push_back({"P0", 100.0f, true});
    result.patches.push_back({"P1", 50.0f, true});

    auto ink_result = find_ink_limit(result);
    REQUIRE(ink_result.success == false);
}

TEST_CASE("validate_measurements_for_ink_limit: passes with good data", "[wf1][ink_limit]") {
    auto chart = generate_wf1_ink_ramp();
    auto measurements = simulate_wf1_measurement(chart, 0.0f);

    REQUIRE(validate_measurements_for_ink_limit(measurements) == true);
}

TEST_CASE("validate_measurements_for_ink_limit: fails with bad success flag", "[wf1][ink_limit]") {
    ScanResult result;
    result.success = false;

    REQUIRE(validate_measurements_for_ink_limit(result) == false);
}

TEST_CASE("validate_measurements_for_ink_limit: fails with too few patches", "[wf1][ink_limit]") {
    ScanResult result;
    result.success = true;
    result.patches.push_back({"P0", 100.0f, true});
    result.patches.push_back({"P1", 50.0f, true});

    REQUIRE(validate_measurements_for_ink_limit(result) == false);
}

TEST_CASE("InkLimitResult: default values", "[wf1][ink_limit]") {
    InkLimitResult result;
    REQUIRE(result.ink_limit == 0.85f);  // Default from spec
    REQUIRE(result.max_density == 0.0f);
    REQUIRE(result.saturation_point == 0.95f);
    REQUIRE(result.success == false);
    REQUIRE(result.error_message.empty());
}

TEST_CASE("export_chart_with_logo: throws on empty chart", "[wf1][ink_limit]") {
    ChartLayout empty;
    REQUIRE_THROWS_AS(export_chart_with_logo(empty, "logo.png", "output.png"),
                     std::invalid_argument);
}

TEST_CASE("export_chart_with_logo: throws on empty logo path", "[wf1][ink_limit]") {
    auto chart = generate_wf1_ink_ramp();
    REQUIRE_THROWS_AS(export_chart_with_logo(chart, "", "output.png"),
                     std::invalid_argument);
}