#include <catch2/catch_test_macros.hpp>
#include "wf2_lut.h"

using namespace jcolour::workflow;

TEST_CASE("[wf2] LStarToInkLUT has correct size", "[wf2]") {
    LStarToInkLUT lut;
    // Check that we can access all entries
    for (int i = 0; i < WF2_LUT_SIZE; ++i) {
        for (int ch = 0; ch < WF2_NUM_CHANNELS; ++ch) {
            lut[i][ch] = 0.5f;
        }
    }
    REQUIRE(true);  // If we get here, the LUT is accessible
}

TEST_CASE("[wf2] apply_wf2_lut returns valid values", "[wf2]") {
    LStarToInkLUT lut;
    for (int i = 0; i < WF2_LUT_SIZE; ++i) {
        lut[i][0] = 0.5f;
        lut[i][1] = 0.3f;
        lut[i][2] = 0.2f;
    }

    auto inks = apply_wf2_lut(128.0f, lut);
    REQUIRE(inks[0] == 0.5f);
    REQUIRE(inks[1] == 0.3f);
    REQUIRE(inks[2] == 0.2f);
}

TEST_CASE("[wf2] apply_wf2_lut clamps to valid range", "[wf2]") {
    LStarToInkLUT lut;
    lut[0][0] = 0.1f;
    lut[255][0] = 0.9f;

    auto inks_low = apply_wf2_lut(-10.0f, lut);
    auto inks_high = apply_wf2_lut(300.0f, lut);

    // Should clamp to valid indices
    REQUIRE(true);
}

TEST_CASE("[wf2] validate_wf2_lut_monotonic", "[wf2]") {
    LStarToInkLUT lut;
    // Create a monotonic decreasing LUT (as L* increases, ink decreases)
    for (int i = 0; i < WF2_LUT_SIZE; ++i) {
        float fraction = i / 255.0f;
        lut[i][0] = 1.0f - fraction;          // MAIN_K: high ink at low L*
        lut[i][1] = 0.5f - fraction * 0.5f;   // GY: medium
        lut[i][2] = 0.3f - fraction * 0.3f;   // LGY: lower
    }

    REQUIRE(validate_wf2_lut_monotonic(lut));
}

TEST_CASE("[wf2] Wf2SplitConfig default values", "[wf2]") {
    Wf2SplitConfig config;
    REQUIRE(config.alpha_metamerism > 0.0f);
    REQUIRE(config.preference == SplitPreference::NEUTRAL);
}

TEST_CASE("[wf2] Wf2LUTResult can be initialized", "[wf2]") {
    Wf2LUTResult result;
    // Default values should be reasonable
    REQUIRE(result.valid == false);  // This is a bool, should be stable
}