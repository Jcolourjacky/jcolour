#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include "charts.h"

using namespace jcolour::calibration;

TEST_CASE("workflow 1 produces 6 patches", "[charts]") {
    auto layout = build_self_test_chart(1);
    REQUIRE(layout.workflow == 1);
    REQUIRE(layout.patches.size() == 6);
}

TEST_CASE("workflow 2 produces 19 patches", "[charts]") {
    auto layout = build_self_test_chart(2);
    REQUIRE(layout.workflow == 2);
    REQUIRE(layout.patches.size() == 19);
}

TEST_CASE("workflow 3 produces 22 patches", "[charts]") {
    auto layout = build_self_test_chart(3);
    REQUIRE(layout.workflow == 3);
    REQUIRE(layout.patches.size() == 22);
}

TEST_CASE("workflow 4 produces 29 patches", "[charts]") {
    auto layout = build_self_test_chart(4);
    REQUIRE(layout.workflow == 4);
    REQUIRE(layout.patches.size() == 29);
}

TEST_CASE("workflow 1 first patch is white", "[charts]") {
    auto layout = build_self_test_chart(1);
    const auto& w = layout.patches[0];
    REQUIRE(w.label == "W");
    REQUIRE(w.cmyk[0] == 0.0f);
    REQUIRE(w.cmyk[1] == 0.0f);
    REQUIRE(w.cmyk[2] == 0.0f);
    REQUIRE(w.cmyk[3] == 0.0f);
}

TEST_CASE("workflow 1 contains C100 patch", "[charts]") {
    auto layout = build_self_test_chart(1);
    bool found = false;
    for (const auto& p : layout.patches) {
        if (p.label == "C100") {
            REQUIRE(p.cmyk[0] == 1.0f);
            REQUIRE(p.cmyk[1] == 0.0f);
            REQUIRE(p.cmyk[2] == 0.0f);
            REQUIRE(p.cmyk[3] == 0.0f);
            found = true;
        }
    }
    REQUIRE(found);
}

TEST_CASE("workflow 4 contains rich black patch", "[charts]") {
    auto layout = build_self_test_chart(4);
    bool found = false;
    for (const auto& p : layout.patches) {
        if (p.label == "RICH_K") {
            found = true;
        }
    }
    REQUIRE(found);
}

TEST_CASE("workflow 4 is superset of workflow 3", "[charts]") {
    auto w3 = build_self_test_chart(3);
    auto w4 = build_self_test_chart(4);
    // Every label in w3 must appear in w4
    for (const auto& p3 : w3.patches) {
        bool found = false;
        for (const auto& p4 : w4.patches) {
            if (p3.label == p4.label) { found = true; break; }
        }
        REQUIRE(found);
    }
}

TEST_CASE("unknown workflow throws", "[charts]") {
    REQUIRE_THROWS_AS(build_self_test_chart(0), std::invalid_argument);
    REQUIRE_THROWS_AS(build_self_test_chart(5), std::invalid_argument);
    REQUIRE_THROWS_AS(build_self_test_chart(-1), std::invalid_argument);
}
