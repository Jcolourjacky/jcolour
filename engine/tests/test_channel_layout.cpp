#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "channel_layout.h"

using jcolour::ActiveChannel;
using jcolour::ActiveInks;
using jcolour::BlackKind;
using jcolour::PhysicalInks;
using jcolour::NUM_ACTIVE_CHANNELS;
using jcolour::NUM_PHYSICAL_CHANNELS;
using jcolour::PHYSICAL_PK_INDEX;
using jcolour::PHYSICAL_MK_INDEX;

TEST_CASE("Active and physical channel counts are 11 and 12", "[channel_layout]") {
    REQUIRE(NUM_ACTIVE_CHANNELS == 11);
    REQUIRE(NUM_PHYSICAL_CHANNELS == 12);
    REQUIRE(PHYSICAL_PK_INDEX == 0);
    REQUIRE(PHYSICAL_MK_INDEX == 1);
}

TEST_CASE("ActiveChannel enum order is stable", "[channel_layout]") {
    // The order is part of the project contract — any change must be made in
    // engine/workflow/README.md too and is a breaking change.
    REQUIRE(static_cast<int>(ActiveChannel::MAIN_K) == 0);
    REQUIRE(static_cast<int>(ActiveChannel::LC)     == 1);
    REQUIRE(static_cast<int>(ActiveChannel::C)      == 2);
    REQUIRE(static_cast<int>(ActiveChannel::VLM)    == 3);
    REQUIRE(static_cast<int>(ActiveChannel::VM)     == 4);
    REQUIRE(static_cast<int>(ActiveChannel::Y)      == 5);
    REQUIRE(static_cast<int>(ActiveChannel::Or)     == 6);
    REQUIRE(static_cast<int>(ActiveChannel::GY)     == 7);
    REQUIRE(static_cast<int>(ActiveChannel::LGY)    == 8);
    REQUIRE(static_cast<int>(ActiveChannel::V)      == 9);
    REQUIRE(static_cast<int>(ActiveChannel::Gr)     == 10);
}

TEST_CASE("expand_to_physical routes MAIN_K to PK slot for PK BlackKind",
          "[channel_layout]") {
    ActiveInks a{};
    a[jcolour::idx(ActiveChannel::MAIN_K)] = 0.7f;

    auto p = jcolour::expand_to_physical(a, BlackKind::PK);

    REQUIRE(p[PHYSICAL_PK_INDEX] == Catch::Approx(0.7f));
    REQUIRE(p[PHYSICAL_MK_INDEX] == Catch::Approx(0.0f));
}

TEST_CASE("expand_to_physical routes MAIN_K to MK slot for MK BlackKind",
          "[channel_layout]") {
    ActiveInks a{};
    a[jcolour::idx(ActiveChannel::MAIN_K)] = 0.7f;

    auto p = jcolour::expand_to_physical(a, BlackKind::MK);

    REQUIRE(p[PHYSICAL_PK_INDEX] == Catch::Approx(0.0f));
    REQUIRE(p[PHYSICAL_MK_INDEX] == Catch::Approx(0.7f));
}

TEST_CASE("expand_to_physical preserves all non-black channels", "[channel_layout]") {
    ActiveInks a{};
    a[jcolour::idx(ActiveChannel::LC)]  = 0.10f;
    a[jcolour::idx(ActiveChannel::C)]   = 0.20f;
    a[jcolour::idx(ActiveChannel::VLM)] = 0.30f;
    a[jcolour::idx(ActiveChannel::VM)]  = 0.40f;
    a[jcolour::idx(ActiveChannel::Y)]   = 0.50f;
    a[jcolour::idx(ActiveChannel::Or)]  = 0.60f;
    a[jcolour::idx(ActiveChannel::GY)]  = 0.70f;
    a[jcolour::idx(ActiveChannel::LGY)] = 0.80f;
    a[jcolour::idx(ActiveChannel::V)]   = 0.90f;
    a[jcolour::idx(ActiveChannel::Gr)]  = 0.95f;

    auto p = jcolour::expand_to_physical(a, BlackKind::PK);

    // Physical layout: 0:PK 1:MK 2:LC 3:C 4:VLM 5:VM 6:Y 7:Or 8:GY 9:LGY 10:V 11:Gr
    REQUIRE(p[2]  == Catch::Approx(0.10f));
    REQUIRE(p[3]  == Catch::Approx(0.20f));
    REQUIRE(p[4]  == Catch::Approx(0.30f));
    REQUIRE(p[5]  == Catch::Approx(0.40f));
    REQUIRE(p[6]  == Catch::Approx(0.50f));
    REQUIRE(p[7]  == Catch::Approx(0.60f));
    REQUIRE(p[8]  == Catch::Approx(0.70f));
    REQUIRE(p[9]  == Catch::Approx(0.80f));
    REQUIRE(p[10] == Catch::Approx(0.90f));
    REQUIRE(p[11] == Catch::Approx(0.95f));
}

TEST_CASE("collapse_from_physical is the inverse of expand_to_physical (PK)",
          "[channel_layout]") {
    ActiveInks original{};
    for (int i = 0; i < NUM_ACTIVE_CHANNELS; ++i) {
        original[i] = 0.05f + i * 0.08f;
    }

    auto physical = jcolour::expand_to_physical(original, BlackKind::PK);
    auto round_trip = jcolour::collapse_from_physical(physical, BlackKind::PK);

    for (int i = 0; i < NUM_ACTIVE_CHANNELS; ++i) {
        REQUIRE(round_trip[i] == Catch::Approx(original[i]));
    }
}

TEST_CASE("collapse_from_physical is the inverse of expand_to_physical (MK)",
          "[channel_layout]") {
    ActiveInks original{};
    for (int i = 0; i < NUM_ACTIVE_CHANNELS; ++i) {
        original[i] = 0.1f + i * 0.05f;
    }

    auto physical = jcolour::expand_to_physical(original, BlackKind::MK);
    auto round_trip = jcolour::collapse_from_physical(physical, BlackKind::MK);

    for (int i = 0; i < NUM_ACTIVE_CHANNELS; ++i) {
        REQUIRE(round_trip[i] == Catch::Approx(original[i]));
    }
}

TEST_CASE("obeys_black_exclusivity accepts valid layouts", "[channel_layout]") {
    PhysicalInks ok{};
    ok[PHYSICAL_PK_INDEX] = 0.8f;  // PK active
    ok[PHYSICAL_MK_INDEX] = 0.0f;  // MK silent
    REQUIRE(jcolour::obeys_black_exclusivity(ok));

    PhysicalInks ok2{};
    ok2[PHYSICAL_PK_INDEX] = 0.0f;
    ok2[PHYSICAL_MK_INDEX] = 0.5f;
    REQUIRE(jcolour::obeys_black_exclusivity(ok2));

    PhysicalInks ok3{};  // both zero — also valid (paper white)
    REQUIRE(jcolour::obeys_black_exclusivity(ok3));
}

TEST_CASE("obeys_black_exclusivity rejects PK+MK simultaneous output",
          "[channel_layout]") {
    PhysicalInks bad{};
    bad[PHYSICAL_PK_INDEX] = 0.5f;
    bad[PHYSICAL_MK_INDEX] = 0.3f;
    REQUIRE_FALSE(jcolour::obeys_black_exclusivity(bad));
}

TEST_CASE("expand_to_physical output always obeys black exclusivity",
          "[channel_layout]") {
    // Random ActiveInks fed through expand_to_physical must always satisfy
    // the PK XOR MK rule by construction — this is the whole point of the
    // type system.
    ActiveInks a{};
    for (int i = 0; i < NUM_ACTIVE_CHANNELS; ++i) {
        a[i] = static_cast<float>(i + 1) / NUM_ACTIVE_CHANNELS;
    }

    auto p_pk = jcolour::expand_to_physical(a, BlackKind::PK);
    auto p_mk = jcolour::expand_to_physical(a, BlackKind::MK);

    REQUIRE(jcolour::obeys_black_exclusivity(p_pk));
    REQUIRE(jcolour::obeys_black_exclusivity(p_mk));
}
