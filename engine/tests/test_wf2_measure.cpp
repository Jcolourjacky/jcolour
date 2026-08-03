#include <catch2/catch_test_macros.hpp>
#include "wf2_measure.h"
#include "../src/km.h"

using namespace jcolour;
using namespace jcolour::workflow;
using namespace jcolour::forward;

TEST_CASE("[wf2] simulate_wf2_measurements returns correct count", "[wf2]") {
    auto chart = generate_wf2_ink_ramp();
    auto measurements = simulate_wf2_measurements(chart);
    REQUIRE(measurements.size() == chart.patches.size());
}

TEST_CASE("[wf2] simulate_wf2_measurements has valid L* range", "[wf2]") {
    auto chart = generate_wf2_ink_ramp();
    auto measurements = simulate_wf2_measurements(chart);

    for (const auto& m : measurements) {
        REQUIRE(m.L_star >= 0.0);
        REQUIRE(m.L_star <= 100.0);
    }
}

TEST_CASE("[wf2] analyze_wf2_ink_limits returns all channels", "[wf2]") {
    auto chart = generate_wf2_ink_ramp();
    auto result = analyze_wf2_ink_limits(chart, true);

    REQUIRE((int)result.per_channel.size() == WF2_NUM_CHANNELS);
}

TEST_CASE("[wf2] analyze_wf2_ink_limit is monotonic for MAIN_K", "[wf2]") {
    auto chart = generate_wf2_ink_ramp();
    auto measurements = simulate_wf2_measurements(chart);

    std::vector<Wf2Measurement> main_k_measurements;
    for (const auto& m : measurements) {
        if (m.channel == Wf2Channel::MAIN_K) {
            main_k_measurements.push_back(m);
        }
    }

    auto limit = analyze_wf2_ink_limit(main_k_measurements);
    REQUIRE(limit.is_monotonic == true);
}

TEST_CASE("[wf2] Wf2KMModel can be created", "[wf2]") {
    Wf2KMModel model;
    REQUIRE(model.km == nullptr);
    REQUIRE(model.yule_nielsen_n > 0.0f);
}

TEST_CASE("[wf2] fit_km_model creates valid model", "[wf2]") {
    auto chart = generate_wf2_ink_ramp();
    auto measurements = simulate_wf2_measurements(chart);
    auto primaries = generate_wf2_neugebauer_primaries();

    std::array<double, 36> paper_white;
    paper_white.fill(0.9);

    auto model = fit_km_model(measurements, primaries, paper_white);
    REQUIRE(model.km != nullptr);
    REQUIRE(model.yule_nielsen_n > 0.0f);
}

TEST_CASE("[wf2] run_wf2_calibration produces result", "[wf2]") {
    auto chart = generate_wf2_calibration_chart();
    auto measurements = simulate_wf2_measurements(chart);

    auto result = run_wf2_calibration(chart, measurements, true);

    REQUIRE((int)result.ink_limits.per_channel.size() == WF2_NUM_CHANNELS);
    REQUIRE(result.total_ink_limit > 0.0f);
}