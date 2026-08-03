#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "wf6_self_test.h"
#include "wf6_chart.h"

using namespace jcolour::workflow;

TEST_CASE("wf6_self_test: generate_wf6_self_test_chart", "[wf6][wf6-self-test]") {
    auto chart = generate_wf6_self_test_chart();

    REQUIRE(chart.base_chart.single_ink_patches.size() > 0);
    CHECK(chart.gamut_compression_enabled == true);
}

TEST_CASE("wf6_self_test: run_wf6_self_test passing", "[wf6][wf6-self-test]") {
    // Simulate measurements matching baseline
    std::vector<std::array<float, 3>> measurements;
    std::vector<std::array<float, 3>> baseline;

    for (int i = 0; i < 20; ++i) {
        measurements.push_back({50.0f, 0.0f, 0.0f});
        baseline.push_back({50.0f, 0.0f, 0.0f});
    }

    Wf6GamutConfig config;
    config.gamut_compression_enabled = true;

    auto result = run_wf6_self_test({}, measurements, baseline, config);

    CHECK(result.passed == true);
    CHECK(result.avg_delta_e < 0.1);
}

TEST_CASE("wf6_self_test: run_wf6_self_test with gamut compression", "[wf6][wf6-self-test]") {
    std::vector<std::array<float, 3>> measurements;
    std::vector<std::array<float, 3>> baseline;

    for (int i = 0; i < 20; ++i) {
        measurements.push_back({50.0f, 5.0f, 5.0f});  // Slightly compressed
        baseline.push_back({50.0f, 0.0f, 0.0f});
    }

    Wf6GamutConfig config;
    config.gamut_compression_enabled = true;

    auto result = run_wf6_self_test({}, measurements, baseline, config);

    CHECK(result.passed == false);
    CHECK(result.gamut_compressed_count > 0);
}

TEST_CASE("wf6_self_test: run_wf6_light_recalib", "[wf6][wf6-self-test]") {
    Wf6LUT current_lut;
    current_lut.size = 33;
    current_lut.channels = WF6_NUM_CHANNELS;
    current_lut.data.resize(current_lut.size * current_lut.size * current_lut.size * WF6_NUM_CHANNELS, 0.5f);

    std::vector<std::array<float, 3>> measurements;
    for (int i = 0; i < 10; ++i) {
        measurements.push_back({50.0f, 0.0f, 0.0f});
    }

    auto result = run_wf6_light_recalib({}, measurements, current_lut);

    CHECK(result.success == true);
    CHECK(result.improvement > 0.0);
}
