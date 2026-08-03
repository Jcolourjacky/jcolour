#include <catch2/catch_test_macros.hpp>
#include "wf1_chart.h"

using namespace jcolour::workflow;

TEST_CASE("generate_wf1_ink_ramp: returns correct patch count", "[wf1][chart]") {
    auto layout = generate_wf1_ink_ramp();
    REQUIRE(layout.patches.size() == 21);
}

TEST_CASE("generate_wf1_ink_ramp: density values are correct", "[wf1][chart]") {
    auto layout = generate_wf1_ink_ramp();
    REQUIRE(layout.patches[0].ink_density == 0.0f);
    REQUIRE(layout.patches[10].ink_density == 0.5f);
    REQUIRE(layout.patches[20].ink_density == 1.0f);
}

TEST_CASE("generate_wf1_ink_ramp: labels are monotonic", "[wf1][chart]") {
    auto layout = generate_wf1_ink_ramp();
    for (int i = 1; i < static_cast<int>(layout.patches.size()); ++i) {
        REQUIRE(layout.patches[i].ink_density > layout.patches[i-1].ink_density);
    }
}

TEST_CASE("generate_wf1_ink_ramp: channel is PK (0)", "[wf1][chart]") {
    auto layout = generate_wf1_ink_ramp();
    for (const auto& patch : layout.patches) {
        REQUIRE(patch.channel == 0);
    }
}

TEST_CASE("generate_wf1_verification: returns 11 patches", "[wf1][chart]") {
    auto layout = generate_wf1_verification();
    REQUIRE(layout.patches.size() == 11);
}

TEST_CASE("generate_wf1_verification: density increments by 10%", "[wf1][chart]") {
    auto layout = generate_wf1_verification();
    REQUIRE(layout.patches[0].ink_density == 0.0f);
    REQUIRE(layout.patches[5].ink_density == 0.5f);
    REQUIRE(layout.patches[10].ink_density == 1.0f);
}

TEST_CASE("ChartLayout: default values", "[wf1][chart]") {
    ChartLayout layout;
    REQUIRE(layout.columns == 6);
    REQUIRE(layout.dpi == 1440);
    REQUIRE(layout.patch_size_mm == 10.0f);
    REQUIRE(layout.patches.empty());
}