#include <catch2/catch_test_macros.hpp>
#include "wf2_chart.h"

using namespace jcolour::workflow;

TEST_CASE("[wf2] generate_wf2_ink_ramp produces correct count", "[wf2]") {
    auto chart = generate_wf2_ink_ramp();
    REQUIRE(chart.patches.size() == WF2_NUM_CHANNELS * 21);  // 3 channels × 21 steps = 63
    REQUIRE(chart.columns == 8);
    REQUIRE(chart.dpi == 1440);
}

TEST_CASE("[wf2] ink ramp defaults to PK black", "[wf2]") {
    auto chart = generate_wf2_ink_ramp();
    REQUIRE(chart.black_kind == BlackKind::PK);
}

TEST_CASE("[wf2] ink ramp honors requested black kind", "[wf2]") {
    auto chart = generate_wf2_ink_ramp(BlackKind::MK);
    REQUIRE(chart.black_kind == BlackKind::MK);

    // First patch label should reflect MK
    REQUIRE(chart.patches[0].label.find("MK") == 0);
}

TEST_CASE("[wf2] ink ramp has correct step values", "[wf2]") {
    auto chart = generate_wf2_ink_ramp();

    // First channel starts at 0
    REQUIRE(chart.patches[0].ink_density == 0.0f);
    REQUIRE(chart.patches[20].ink_density == 1.0f);

    // Second channel starts at 0 again
    REQUIRE(chart.patches[21].ink_density == 0.0f);
}

TEST_CASE("[wf2] ink ramp has all 3 channels", "[wf2]") {
    auto chart = generate_wf2_ink_ramp();

    bool has_main_k = false, has_gy = false, has_lgy = false;
    for (const auto& p : chart.patches) {
        if (p.channel == Wf2Channel::MAIN_K) has_main_k = true;
        if (p.channel == Wf2Channel::GY)     has_gy = true;
        if (p.channel == Wf2Channel::LGY)    has_lgy = true;
    }
    REQUIRE(has_main_k);
    REQUIRE(has_gy);
    REQUIRE(has_lgy);
}

TEST_CASE("[wf2] generate_wf2_neugebauer_primaries", "[wf2]") {
    auto primaries = generate_wf2_neugebauer_primaries();
    // 3 single + 3 double + 1 triple = 7
    REQUIRE(primaries.primaries.size() == 7);
}

TEST_CASE("[wf2] neugebauer has correct combinations", "[wf2]") {
    auto primaries = generate_wf2_neugebauer_primaries();

    // First 3 should be single-channel
    for (int i = 0; i < 3; ++i) {
        REQUIRE(primaries.primaries[i].size() == 1);
    }

    // Last one (index 6) should be triple (all channels)
    REQUIRE(primaries.primaries[6].size() == 3);
}

TEST_CASE("[wf2] generate_wf2_verification", "[wf2]") {
    auto chart = generate_wf2_verification();
    REQUIRE(!chart.patches.empty());
}

TEST_CASE("[wf2] generate_wf2_calibration_chart combines ramp and verification", "[wf2]") {
    auto chart = generate_wf2_calibration_chart();
    REQUIRE(!chart.patches.empty());

    // Should have more patches than ramp alone (has verification added)
    auto ramp = generate_wf2_ink_ramp();
    REQUIRE(chart.patches.size() > ramp.patches.size());
}

TEST_CASE("[wf2] export_wf2_chart_json runs without crash", "[wf2]") {
    auto chart = generate_wf2_ink_ramp();
    REQUIRE_NOTHROW(export_wf2_chart_json(chart, "/tmp/wf2_test.json"));
}

TEST_CASE("[wf2] Wf2Channel enum values", "[wf2]") {
    REQUIRE(static_cast<int>(Wf2Channel::MAIN_K) == 0);
    REQUIRE(static_cast<int>(Wf2Channel::GY)     == 1);
    REQUIRE(static_cast<int>(Wf2Channel::LGY)    == 2);
    REQUIRE(WF2_NUM_CHANNELS == 3);
}

TEST_CASE("[wf2] Wf2Patch default initialization", "[wf2]") {
    Wf2Patch patch;
    REQUIRE(patch.ink_density == 0.0f);
    REQUIRE(patch.channel == Wf2Channel::MAIN_K);
}

TEST_CASE("[wf2] Wf2ChartLayout default initialization", "[wf2]") {
    Wf2ChartLayout layout;
    REQUIRE(layout.patches.empty());
    REQUIRE(layout.black_kind == BlackKind::PK);
    REQUIRE(layout.columns == 8);
    REQUIRE(layout.dpi == 1440);
}
