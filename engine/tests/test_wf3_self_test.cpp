#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "wf3_self_test.h"
#include "wf3_measure.h"

using namespace jcolour::workflow;

TEST_CASE("wf3_self_test: generate_wf3_self_test_chart", "[wf3][wf3-self-test]") {
    auto chart = generate_wf3_self_test_chart();

    REQUIRE(chart.patches.size() > 0);
    CHECK(chart.columns == 8);
    CHECK(chart.dpi == 1440);
}

TEST_CASE("wf3_self_test: run_wf3_self_test passing", "[wf3][wf3-self-test]") {
    auto chart = generate_wf3_self_test_chart();

    // Simulate measurements matching baseline (ΔE = 0)
    std::vector<WF3Measurement> measurements;
    std::vector<WF3Measurement> baseline;

    for (size_t i = 0; i < 25; ++i) {
        WF3Measurement m;
        m.L_star = 50.0f;
        m.a_star = 0.0f;
        m.b_star = 0.0f;
        measurements.push_back(m);
        baseline.push_back(m);
    }

    auto result = run_wf3_self_test(chart, measurements, baseline);

    CHECK(result.passed == true);
    CHECK(result.avg_delta_e < 0.1);
    CHECK(result.max_delta_e < 0.1);
}

TEST_CASE("wf3_self_test: run_wf3_self_test failing", "[wf3][wf3-self-test]") {
    auto chart = generate_wf3_self_test_chart();

    // Simulate large deviations
    std::vector<WF3Measurement> measurements;
    std::vector<WF3Measurement> baseline;

    for (size_t i = 0; i < 25; ++i) {
        WF3Measurement m;
        m.L_star = 50.0f;
        m.a_star = 0.0f;
        m.b_star = 0.0f;
        measurements.push_back(m);

        WF3Measurement b;
        b.L_star = 30.0f;
        b.a_star = 10.0f;
        b.b_star = 10.0f;
        baseline.push_back(b);
    }

    auto result = run_wf3_self_test(chart, measurements, baseline);

    CHECK(result.passed == false);
    CHECK(result.avg_delta_e > 1.0);
}

TEST_CASE("wf3_self_test: run_wf3_light_recalib", "[wf3][wf3-self-test]") {
    auto chart = generate_wf3_self_test_chart();

    Wf3LUT current_lut;
    current_lut.entries.resize(WF3_LUT_SIZE);

    std::vector<WF3Measurement> measurements;
    for (int i = 0; i < 10; ++i) {
        WF3Measurement m;
        m.L_star = 50.0f + i * 2.0f;
        measurements.push_back(m);
    }

    auto result = run_wf3_light_recalib(chart, measurements, current_lut);

    CHECK(result.success == true);
    CHECK(result.improvement > 0.0);
}
