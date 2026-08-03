#include <catch2/catch_test_macros.hpp>
#include "wf4_self_test.h"
#include "wf4_chart.h"
#include <vector>

using namespace jcolour::workflow;

TEST_CASE("[wf4] generate_wf4_self_test_chart has correct patch count", "[wf4]") {
    auto chart = generate_wf4_self_test_chart();
    REQUIRE((int)chart.single_ink_patches.size() >= 29);
}

TEST_CASE("[wf4] self test chart has neutral and color patches", "[wf4]") {
    auto chart = generate_wf4_self_test_chart();

    int neutral_count = 0;
    int color_count = 0;
    for (const auto& p : chart.single_ink_patches) {
        if (p.label.substr(0, 1) == "N") {
            neutral_count++;
        } else {
            color_count++;
        }
    }
    REQUIRE(neutral_count >= 21);  // N0 to N100 in 5% steps
    REQUIRE(color_count >= 8);     // 8 color samples
}

TEST_CASE("[wf4] run_wf4_self_test with matching data passes", "[wf4]") {
    auto chart = generate_wf4_self_test_chart();
    int n = (int)chart.single_ink_patches.size();

    std::vector<std::array<float, 3>> current(n);
    std::vector<std::array<float, 3>> baseline(n);
    for (int i = 0; i < n; ++i) {
        current[i] = {50.0f, 0.0f, 0.0f};
        baseline[i] = {50.0f, 0.0f, 0.0f};  // Identical
    }

    auto result = run_wf4_self_test(chart, current, baseline);
    REQUIRE(result.passed == true);
    REQUIRE(result.avg_delta_e == 0.0f);
}

TEST_CASE("[wf4] run_wf4_self_test with different data fails", "[wf4]") {
    auto chart = generate_wf4_self_test_chart();
    int n = (int)chart.single_ink_patches.size();

    std::vector<std::array<float, 3>> current(n);
    std::vector<std::array<float, 3>> baseline(n);
    for (int i = 0; i < n; ++i) {
        current[i] = {50.0f, 0.0f, 0.0f};
        baseline[i] = {60.0f, 2.0f, 2.0f};  // Different
    }

    auto result = run_wf4_self_test(chart, current, baseline);
    REQUIRE(result.max_delta_e > 0.0f);
}

TEST_CASE("[wf4] run_wf4_light_recalib produces result", "[wf4]") {
    auto chart = generate_wf4_self_test_chart();
    std::vector<std::array<float, 3>> measurements(chart.single_ink_patches.size());

    for (size_t i = 0; i < measurements.size(); ++i) {
        measurements[i] = {static_cast<float>(i) * 5.0f, 0.0f, 0.0f};
    }

    auto result = run_wf4_light_recalib(chart, measurements);
    REQUIRE(result.success == true);
}

TEST_CASE("[wf4] calculate_delta_e is correct", "[wf4]") {
    float de = calculate_delta_e(50.0f, 0.0f, 0.0f, 50.0f, 0.0f, 0.0f);
    REQUIRE(de == 0.0f);

    de = calculate_delta_e(50.0f, 0.0f, 0.0f, 51.0f, 0.0f, 0.0f);
    REQUIRE(de == 1.0f);
}

TEST_CASE("[wf4] determine_calib_state returns correct states", "[wf4]") {
    WF4SelfTestResult result;

    result.passed = true;
    REQUIRE(determine_calib_state(result) == "FRESH");

    result.passed = false;
    result.max_delta_e = 2.5f;
    REQUIRE(determine_calib_state(result) == "STALE_SOFT");

    result.max_delta_e = 5.0f;
    REQUIRE(determine_calib_state(result) == "STALE_HARD");
}