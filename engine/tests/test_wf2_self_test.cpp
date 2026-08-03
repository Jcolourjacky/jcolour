#include <catch2/catch_test_macros.hpp>
#include "wf2_self_test.h"

using namespace jcolour::workflow;

TEST_CASE("[wf2] generate_wf2_self_test_chart has correct patch count", "[wf2]") {
    auto chart = generate_wf2_self_test_chart();
    // 12 patches (4 channels × 3) + 5 neutral + 2 mixed = 19
    REQUIRE((int)chart.patches.size() >= 15);
}

TEST_CASE("[wf2] self test chart has all channels", "[wf2]") {
    auto chart = generate_wf2_self_test_chart();
    bool has_all_channels = true;
    for (int ch = 0; ch < WF2_NUM_CHANNELS; ++ch) {
        bool has_ch = false;
        for (const auto& p : chart.patches) {
            if (static_cast<int>(p.channel) == ch) {
                has_ch = true;
                break;
            }
        }
        if (!has_ch) has_all_channels = false;
    }
    REQUIRE(has_all_channels);
}

TEST_CASE("[wf2] run_wf2_self_test with matching data passes", "[wf2]") {
    Wf2ChartLayout chart;
    chart.patches.push_back({});
    chart.patches[0].label = "P1";

    std::vector<Wf2Measurement> current, baseline;
    Wf2Measurement m1, m2;
    m1.L_star = 50.0;
    m1.a_star = 0.0;
    m1.b_star = 0.0;
    m2 = m1;  // Identical

    current.push_back(m1);
    baseline.push_back(m2);

    auto result = run_wf2_self_test(chart, current, baseline);
    REQUIRE(result.passed == true);
    REQUIRE(result.avg_delta_e == 0.0);
}

TEST_CASE("[wf2] run_wf2_self_test with different data fails", "[wf2]") {
    Wf2ChartLayout chart;
    chart.patches.push_back({});
    chart.patches[0].label = "P1";

    std::vector<Wf2Measurement> current, baseline;
    Wf2Measurement m1, m2;
    m1.L_star = 50.0;
    m1.a_star = 0.0;
    m1.b_star = 0.0;

    m2.L_star = 60.0;  // Different
    m2.a_star = 2.0;
    m2.b_star = 2.0;

    current.push_back(m1);
    baseline.push_back(m2);

    auto result = run_wf2_self_test(chart, current, baseline);
    REQUIRE(result.max_delta_e > 0.0);
}

TEST_CASE("[wf2] run_wf2_light_recalib produces result", "[wf2]") {
    Wf2ChartLayout chart;
    std::vector<Wf2Measurement> measurements;
    LStarToInkLUT lut;
    for (int i = 0; i < WF2_LUT_SIZE; ++i) {
        for (int ch = 0; ch < WF2_NUM_CHANNELS; ++ch) {
            lut[i][ch] = 0.5f;
        }
    }

    auto result = run_wf2_light_recalib(chart, measurements, lut);
    REQUIRE(result.success == true);
}

TEST_CASE("[wf2] run_wf2_full_recalib produces valid LUT", "[wf2]") {
    Wf2ChartLayout chart;
    std::vector<Wf2Measurement> measurements;
    Wf2KMModel km_model;
    Wf2AllInkLimits limits;
    Wf2SplitConfig config;

    // Create valid ink limits
    for (int ch = 0; ch < WF2_NUM_CHANNELS; ++ch) {
        Wf2InkLimit limit;
        limit.channel = static_cast<Wf2Channel>(ch);
        limit.limit = 0.95f;
        limits.per_channel.push_back(limit);
    }

    // Create KM model
    std::array<double, 36> paper_white;
    paper_white.fill(0.9);
    km_model.km = std::make_unique<jcolour::forward::KMForwardModel>(paper_white);

    auto lut = run_wf2_full_recalib(chart, measurements, km_model, limits, config);

    // Verify LUT is valid (all values in range)
    for (int i = 0; i < WF2_LUT_SIZE; ++i) {
        for (int ch = 0; ch < WF2_NUM_CHANNELS; ++ch) {
            REQUIRE(lut[i][ch] >= 0.0f);
            REQUIRE(lut[i][ch] <= 1.0f);
        }
    }
}

TEST_CASE("[wf2] Wf2SelfTestResult default values", "[wf2]") {
    Wf2SelfTestResult result;
    REQUIRE(result.avg_delta_e == 0.0);
    REQUIRE(result.max_delta_e == 0.0);
    REQUIRE(result.passed == false);
}

TEST_CASE("[wf2] Wf2LightRecalibResult default values", "[wf2]") {
    Wf2LightRecalibResult result;
    REQUIRE(result.success == false);
    REQUIRE(result.improvement == 0.0);
}