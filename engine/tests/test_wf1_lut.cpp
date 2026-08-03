#include <catch2/catch_test_macros.hpp>
#include "wf1_lut.h"
#include "wf1_chart.h"
#include "wf1_measure.h"
#include "wf1_ink_limit.h"

using namespace jcolour::workflow;

TEST_CASE("LStarToInkLUT: construction from measurements", "[wf1][lut]") {
    std::vector<PatchMeasurement> measurements;
    for (int i = 0; i <= 10; ++i) {
        PatchMeasurement m;
        m.measured_lstar = 100.0f - static_cast<float>(i) * 5.0f;
        m.ink_density = static_cast<float>(i) / 10.0f;
        measurements.push_back(m);
    }

    REQUIRE_NOTHROW(LStarToInkLUT(measurements));
}

TEST_CASE("LStarToInkLUT: throws on too few measurements", "[wf1][lut]") {
    std::vector<PatchMeasurement> measurements;
    measurements.push_back({"P0", 0.0f, 100.0f, true});

    REQUIRE_THROWS_AS(LStarToInkLUT(measurements), std::invalid_argument);
}

TEST_CASE("LStarToInkLUT: lookup returns value within range", "[wf1][lut]") {
    std::vector<PatchMeasurement> measurements;
    for (int i = 0; i <= 10; ++i) {
        PatchMeasurement m;
        m.measured_lstar = 100.0f - static_cast<float>(i) * 5.0f;
        m.ink_density = static_cast<float>(i) / 10.0f;
        measurements.push_back(m);
    }

    LStarToInkLUT lut(measurements);

    float result = lut.lookup(75.0f);
    REQUIRE(result >= 0.0f);
    REQUIRE(result <= 1.0f);
}

TEST_CASE("LStarToInkLUT: lookup at boundaries", "[wf1][lut]") {
    std::vector<PatchMeasurement> measurements;
    measurements.push_back({"P0", 0.0f, 100.0f, true});  // ink=0%, L*=100 (white)
    measurements.push_back({"P1", 1.0f, 0.0f, true});     // ink=100%, L*=0 (black)

    LStarToInkLUT lut(measurements);

    // At L*=0 (darkest) should return max ink
    float at_zero = lut.lookup(0.0f);
    REQUIRE(at_zero >= 0.99f);  // Close to 1.0

    // At L*=100 (lightest) should return min ink
    float at_hundred = lut.lookup(100.0f);
    REQUIRE(at_hundred <= 0.01f);  // Close to 0.0
}

TEST_CASE("LStarToInkLUT: is_monotonic returns true for valid data", "[wf1][lut]") {
    std::vector<PatchMeasurement> measurements;
    // More ink → lower L* (darker). L* ascending, ink ascending.
    for (int i = 0; i <= 10; ++i) {
        PatchMeasurement m;
        m.measured_lstar = static_cast<float>(i) * 10.0f;       // 0, 10, 20, ... 100 (ascending)
        m.ink_density = static_cast<float>(i) / 10.0f;         // 0.0, 0.1, 0.2, ... 1.0 (ascending)
        measurements.push_back(m);
    }

    LStarToInkLUT lut(measurements);
    REQUIRE(lut.is_monotonic() == true);
}

TEST_CASE("LStarToInkLUT: is_monotonic returns false for non-monotonic data", "[wf1][lut]") {
    std::vector<PatchMeasurement> measurements;
    measurements.push_back({"P0", 0.0f, 100.0f, true});
    measurements.push_back({"P1", 0.3f, 50.0f, true});  // Out of order
    measurements.push_back({"P2", 0.5f, 60.0f, true});  // Should be < 50

    LStarToInkLUT lut(measurements);
    REQUIRE(lut.is_monotonic() == false);
}

TEST_CASE("LStarToInkLUT: to_array returns 256 values", "[wf1][lut]") {
    std::vector<PatchMeasurement> measurements;
    for (int i = 0; i <= 10; ++i) {
        PatchMeasurement m;
        m.measured_lstar = 100.0f - static_cast<float>(i) * 5.0f;
        m.ink_density = static_cast<float>(i) / 10.0f;
        measurements.push_back(m);
    }

    LStarToInkLUT lut(measurements);
    auto arr = lut.to_array();

    REQUIRE(arr.size() == 256);
    for (float v : arr) {
        REQUIRE(v >= 0.0f);
        REQUIRE(v <= 1.0f);
    }
}

TEST_CASE("build_lut: from simulated data", "[wf1][lut]") {
    auto chart = generate_wf1_ink_ramp();
    auto measurements = simulate_wf1_measurement(chart, 0.0f);
    auto ink_result = find_ink_limit(measurements);

    auto lut = build_lut(measurements, ink_result);

    REQUIRE(lut.is_monotonic() == true);

    // L*=50 should give roughly 50% ink (our model: L* = 100 - 50*ink)
    // So ink = (100 - L*) / 50 = 1.0 at L*=0, 0.0 at L*=100
    float ink_at_50 = lut.lookup(50.0f);
    REQUIRE(ink_at_50 >= 0.0f);   // Should be valid
    REQUIRE(ink_at_50 <= 1.0f);   // Should be valid
    REQUIRE(ink_at_50 > 0.0f);    // Should not be zero
}

TEST_CASE("build_lut: throws on invalid measurements", "[wf1][lut]") {
    ScanResult bad;
    bad.success = false;
    InkLimitResult ink;
    ink.success = true;
    ink.ink_limit = 0.85f;

    REQUIRE_THROWS_AS(build_lut(bad, ink), std::invalid_argument);
}

TEST_CASE("validate_lut: passes valid LUT", "[wf1][lut]") {
    auto chart = generate_wf1_ink_ramp();
    auto measurements = simulate_wf1_measurement(chart, 0.0f);
    auto ink_result = find_ink_limit(measurements);
    auto lut = build_lut(measurements, ink_result);

    auto validation = validate_lut(lut);

    REQUIRE(validation.monotonic == true);
}

TEST_CASE("validate_lut: fails non-monotonic LUT", "[wf1][lut]") {
    std::vector<PatchMeasurement> bad_data;
    bad_data.push_back({"P0", 0.0f, 100.0f, true});
    bad_data.push_back({"P1", 0.3f, 40.0f, true});  // Drops instead of rises
    bad_data.push_back({"P2", 0.5f, 60.0f, true});

    LStarToInkLUT lut(bad_data);
    auto validation = validate_lut(lut);

    REQUIRE(validation.monotonic == false);
}

TEST_CASE("LUTValidation: default values", "[wf1][lut]") {
    LUTValidation v;
    REQUIRE(v.monotonic == false);
    REQUIRE(v.smooth == false);
    REQUIRE(v.max_derivative == 0.0f);
    REQUIRE(v.error_message.empty());
}