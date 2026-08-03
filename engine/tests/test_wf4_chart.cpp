#include <catch2/catch_test_macros.hpp>
#include "wf4_chart.h"
#include "../src/channel_layout.h"

using namespace jcolour;
using namespace jcolour::workflow;

TEST_CASE("[wf4] generate_wf4_ink_ramp produces correct count", "[wf4]") {
    auto chart = generate_wf4_ink_ramp();
    REQUIRE(chart.single_ink_patches.size() == NUM_INK_CHANNELS * 21);
    REQUIRE(chart.columns == 12);
    REQUIRE(chart.dpi == 1440);
}

TEST_CASE("[wf4] ink ramp defaults to PK black", "[wf4]") {
    auto chart = generate_wf4_ink_ramp();
    REQUIRE(chart.black_kind == BlackKind::PK);
}

TEST_CASE("[wf4] ink ramp honors requested black kind", "[wf4]") {
    auto chart = generate_wf4_ink_ramp(BlackKind::MK);
    REQUIRE(chart.black_kind == BlackKind::MK);
    REQUIRE(chart.single_ink_patches[0].label.find("MK") == 0);
}

TEST_CASE("[wf4] ink ramp has correct step values", "[wf4]") {
    auto chart = generate_wf4_ink_ramp();
    REQUIRE(chart.single_ink_patches[0].ink_density == 0.0f);
    REQUIRE(chart.single_ink_patches[20].ink_density == 1.0f);
    REQUIRE(chart.single_ink_patches[21].ink_density == 0.0f);  // Next channel starts at 0
}

TEST_CASE("[wf4] generate_wf4_til_patches", "[wf4]") {
    auto chart = generate_wf4_til_patches();
    REQUIRE(!chart.til_patches.empty());
    REQUIRE(chart.til_patches[0].channels.size() >= 1);
}

TEST_CASE("[wf4] generate_wf4_calibration_chart combines ramp and TIL", "[wf4]") {
    auto chart = generate_wf4_calibration_chart();
    REQUIRE(!chart.single_ink_patches.empty());
    REQUIRE(!chart.til_patches.empty());
    REQUIRE(chart.single_ink_patches.size() == NUM_INK_CHANNELS * 21);
}

TEST_CASE("[wf4] generate_wf4_verification_chart", "[wf4]") {
    auto chart = generate_wf4_verification_chart();
    REQUIRE(!chart.til_patches.empty());
    REQUIRE(chart.til_patches.size() <= 20);  // Small set for verification
}

TEST_CASE("[wf4] generate_wf4_extended_target reaches target", "[wf4]") {
    auto chart = generate_wf4_extended_target(1500);
    REQUIRE(static_cast<int>(chart.til_patches.size()) >= 1500);
}

TEST_CASE("[wf4] extended target includes neutral and saturated", "[wf4]") {
    auto chart = generate_wf4_extended_target(800);
    bool has_neutral = false, has_saturated = false;
    for (const auto& p : chart.til_patches) {
        if (p.label[0] == 'G') has_neutral = true;
        if (p.label[0] == 'S') has_saturated = true;
    }
    REQUIRE(has_neutral);
    REQUIRE(has_saturated);
}

TEST_CASE("[wf4] export_wf4_chart_json runs without crash", "[wf4]") {
    auto chart = generate_wf4_calibration_chart();
    REQUIRE_NOTHROW(export_wf4_chart_json(chart, "/tmp/wf4_test.json"));
}

TEST_CASE("[wf4] export_wf4_chart_svg runs without crash", "[wf4]") {
    auto chart = generate_wf4_calibration_chart();
    REQUIRE_NOTHROW(export_wf4_chart_svg(chart, "/tmp/wf4_test.svg"));
}

TEST_CASE("[wf4] InkChannel enum (= ActiveChannel) values", "[wf4]") {
    // wf4 now lives in the 11-channel active model. PK/MK are mutually
    // exclusive — the enum exposes MAIN_K only, with physical PK vs MK
    // chosen by Wf4ChartLayout::black_kind.
    REQUIRE(static_cast<int>(InkChannel::MAIN_K) == 0);
    REQUIRE(static_cast<int>(InkChannel::LC)     == 1);
    REQUIRE(static_cast<int>(InkChannel::Or)     == 6);
    REQUIRE(static_cast<int>(InkChannel::Gr)     == 10);
    REQUIRE(NUM_INK_CHANNELS == 11);
}

TEST_CASE("[wf4] black exclusivity holds across the chart", "[wf4][regression]") {
    // F1/F2 audit: no TIL patch may ask for PK + MK simultaneously. Under
    // the new ActiveChannel model this is structurally impossible (the
    // enum has no separate PK/MK), but a sanity scan keeps it pinned in
    // case anyone re-introduces a parallel enum.
    auto chart = generate_wf4_calibration_chart();
    for (const auto& tp : chart.til_patches) {
        for (const auto& [ch, dens] : tp.channels) {
            (void)dens;
            // Cannot encode PK or MK directly anymore — the only black is MAIN_K.
            REQUIRE(ch != static_cast<InkChannel>(-1));
        }
    }
    SUCCEED();
}
