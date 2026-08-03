#include <catch2/catch_test_macros.hpp>
#include "wf4_measure.h"
#include "wf4_chart.h"

using namespace jcolour::workflow;

TEST_CASE("[wf4] simulate_wf4_measurements returns correct count", "[wf4]") {
    auto chart = generate_wf4_calibration_chart();
    auto measurements = simulate_wf4_measurements(chart);
    REQUIRE(measurements.size() == chart.single_ink_patches.size() + chart.til_patches.size());
}

TEST_CASE("[wf4] simulate_wf4_measurements has valid L* range", "[wf4]") {
    auto chart = generate_wf4_calibration_chart();
    auto measurements = simulate_wf4_measurements(chart);

    for (const auto& m : measurements) {
        REQUIRE(m.L_star >= 0.0);
        REQUIRE(m.L_star <= 100.0);
        REQUIRE(m.spectral_data.size() == 36);
    }
}

TEST_CASE("[wf4] analyze_ink_limit produces valid result", "[wf4]") {
    auto chart = generate_wf4_ink_ramp();
    auto measurements = simulate_wf4_measurements(chart);

    // Get MAIN_K measurements (was "PK" before the 11-active refactor)
    std::vector<WF4Measurement> main_k_measurements;
    for (const auto& m : measurements) {
        if (m.channel == InkChannel::MAIN_K) {
            main_k_measurements.push_back(m);
        }
    }

    auto limit = analyze_ink_limit(main_k_measurements);
    REQUIRE(limit.channel == InkChannel::MAIN_K);
    REQUIRE(limit.limit >= 0.0f);
    REQUIRE(limit.limit <= 1.0f);
}

TEST_CASE("[wf4] analyze_wf4_ink_limits returns all channels", "[wf4]") {
    auto chart = generate_wf4_calibration_chart();
    auto result = analyze_wf4_ink_limits(chart, true);

    REQUIRE(result.per_channel.size() == NUM_INK_CHANNELS);
    REQUIRE(result.til.til >= 0.0f);
}

TEST_CASE("[wf4] analyze_ink_limit is monotonic for MAIN_K", "[wf4]") {
    auto chart = generate_wf4_ink_ramp();
    auto measurements = simulate_wf4_measurements(chart);

    std::vector<WF4Measurement> main_k_measurements;
    for (const auto& m : measurements) {
        if (m.channel == InkChannel::MAIN_K) {
            main_k_measurements.push_back(m);
        }
    }

    auto limit = analyze_ink_limit(main_k_measurements);
    REQUIRE(limit.is_monotonic == true);
}

TEST_CASE("[wf4] WF4InkLimit default initialization", "[wf4]") {
    WF4InkLimit limit;
    REQUIRE(limit.limit == 0.0f);
    REQUIRE(limit.channel == InkChannel::MAIN_K);
}

TEST_CASE("[wf4] WF4TILResult default initialization", "[wf4]") {
    WF4TILResult result;
    REQUIRE(result.til == 0.0f);
}

TEST_CASE("[wf4] WF4AllInkLimits empty initialization", "[wf4]") {
    WF4AllInkLimits limits;
    REQUIRE(limits.per_channel.empty());
    REQUIRE(limits.til.til == 0.0f);
}
