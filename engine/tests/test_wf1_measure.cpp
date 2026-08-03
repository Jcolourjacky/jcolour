#include <catch2/catch_test_macros.hpp>
#include "wf1_measure.h"
#include "wf1_chart.h"

using namespace jcolour::workflow;

TEST_CASE("scan_wf1_chart: returns error when not connected", "[wf1][measure]") {
    ChartLayout chart = generate_wf1_ink_ramp();
    auto result = scan_wf1_chart(chart);
    REQUIRE(result.success == false);
    REQUIRE(!result.error_message.empty());
}

TEST_CASE("simulate_wf1_measurement: returns correct patch count", "[wf1][measure]") {
    auto chart = generate_wf1_ink_ramp();
    auto result = simulate_wf1_measurement(chart, 0.0f);
    REQUIRE(result.success == true);
    REQUIRE(result.patches.size() == chart.patches.size());
}

TEST_CASE("simulate_wf1_measurement: L* decreases with ink density", "[wf1][measure]") {
    auto chart = generate_wf1_ink_ramp();
    auto result = simulate_wf1_measurement(chart, 0.0f);  // No noise for deterministic test

    for (size_t i = 1; i < result.patches.size(); ++i) {
        // Each step is 5% more ink, so L* should decrease
        float expected_delta = -2.5f;  // 50 * 0.05
        float actual_delta = result.patches[i].measured_lstar - result.patches[i-1].measured_lstar;
        REQUIRE(std::abs(actual_delta - expected_delta) < 0.01f);
    }
}

TEST_CASE("simulate_wf1_measurement: L* range is correct", "[wf1][measure]") {
    auto chart = generate_wf1_ink_ramp();
    auto result = simulate_wf1_measurement(chart, 0.0f);

    // First patch (0% ink) should be near 100
    REQUIRE(result.patches.front().measured_lstar > 99.0f);
    // Last patch (100% ink) should be near 50
    REQUIRE(result.patches.back().measured_lstar < 51.0f);
}

TEST_CASE("simulate_wf1_measurement: all patches valid", "[wf1][measure]") {
    auto chart = generate_wf1_verification();
    auto result = simulate_wf1_measurement(chart, 0.1f);

    for (const auto& m : result.patches) {
        REQUIRE(m.valid == true);
        REQUIRE(m.measured_lstar >= 0.0f);
        REQUIRE(m.measured_lstar <= 100.0f);
    }
}

TEST_CASE("validate_monotonicity: passes with monotonic data", "[wf1][measure]") {
    ScanResult result;
    result.success = true;
    result.patches.push_back({"P0", 0.0f, 100.0f, true});  // ink=0%, L*=100 (white)
    result.patches.push_back({"P1", 0.25f, 97.5f, true});  // ink=25%, L*=97.5
    result.patches.push_back({"P2", 0.50f, 95.0f, true});  // ink=50%, L*=95

    REQUIRE(validate_monotonicity(result) == true);
}

TEST_CASE("validate_monotonicity: fails with non-monotonic data", "[wf1][measure]") {
    ScanResult result;
    result.success = true;
    result.patches.push_back({"P0", 0.0f, 100.0f, true});  // ink=0%, L*=100 (white)
    result.patches.push_back({"P1", 0.05f, 98.0f, true});  // ink=5%, L*=98 (decreases - OK)
    result.patches.push_back({"P2", 0.10f, 99.0f, true});  // ink=10%, L*=99 (increases - OOPS)
    result.patches.push_back({"P3", 0.20f, 95.0f, true});  // ink=20%, L*=95 (decreases)

    REQUIRE(validate_monotonicity(result, 0.5f) == false);
}

TEST_CASE("validate_monotonicity: handles single patch", "[wf1][measure]") {
    ScanResult result;
    result.success = true;
    result.patches.push_back({"P0", 0.5f, 50.0f, true});

    REQUIRE(validate_monotonicity(result) == true);
}

TEST_CASE("validate_monotonicity: handles empty result", "[wf1][measure]") {
    ScanResult result;
    result.success = true;

    REQUIRE(validate_monotonicity(result) == true);
}

TEST_CASE("ScanResult: default values", "[wf1][measure]") {
    ScanResult result;
    REQUIRE(result.success == false);
    REQUIRE(result.error_message.empty());
    REQUIRE(result.patches.empty());
}