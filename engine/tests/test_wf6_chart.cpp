#include <catch2/catch_test_macros.hpp>
#include "wf6_chart.h"

using namespace jcolour;
using namespace jcolour::workflow;

TEST_CASE("[wf6] generate_wf6_calibration_chart creates layout", "[wf6]") {
    auto chart = generate_wf6_calibration_chart();

    REQUIRE(chart.base_chart.single_ink_patches.size() > 0);
    REQUIRE(chart.gamut_compression_enabled == true);
}

TEST_CASE("[wf6] generate_prophoto_test_points returns points", "[wf6]") {
    auto points = generate_prophoto_test_points(100);

    REQUIRE(points.size() >= 108);  // 100 random + 8 corners
    REQUIRE(points[0].size() == 3);
}

TEST_CASE("[wf6] compress_gamut_sigmoidal handles in-gamut colors", "[wf6]") {
    Wf6GamutConfig config;
    config.compress_threshold = 0.95f;

    // Neutral gray should pass through unchanged
    std::array<float, 3> lab = {50.0f, 0.0f, 0.0f};
    auto result = compress_gamut_sigmoidal(lab, config);

    REQUIRE(std::abs(result[0] - lab[0]) < 0.01f);
    REQUIRE(std::abs(result[1] - lab[1]) < 0.01f);
    REQUIRE(std::abs(result[2] - lab[2]) < 0.01f);
}

TEST_CASE("[wf6] compress_gamut_batch processes all points", "[wf6]") {
    auto points = generate_prophoto_test_points(10);
    Wf6GamutConfig config;
    config.compress_threshold = 0.95f;

    auto results = compress_gamut_batch(points, config);

    REQUIRE(results.size() == points.size());
    for (const auto& lab : results) {
        REQUIRE(lab[0] >= 0.0f);
        REQUIRE(lab[0] <= 100.0f);
    }
}

TEST_CASE("[wf6] build_wf6_lut creates valid LUT", "[wf6]") {
    std::vector<uint8_t> icc_blob;  // Empty for testing
    Wf6GamutConfig config;
    config.compress_threshold = 0.95f;

    auto lut = build_wf6_lut(icc_blob, config);

    REQUIRE(lut.size == Wf6LUT::SIZE);
    REQUIRE(lut.channels == Wf6LUT::CHANNELS);
    REQUIRE(lut.data.size() == Wf6LUT::SIZE * Wf6LUT::SIZE * Wf6LUT::SIZE * Wf6LUT::CHANNELS);
}

TEST_CASE("[wf6] lut_lookup returns ink values", "[wf6]") {
    std::vector<uint8_t> icc_blob;
    Wf6GamutConfig config;
    config.compress_threshold = 0.95f;

    auto lut = build_wf6_lut(icc_blob, config);
    auto inks = lut_lookup(lut, 0.5f, 0.5f, 0.5f);

    REQUIRE(inks.size() == Wf6LUT::CHANNELS);
    for (float ink : inks) {
        REQUIRE(ink >= 0.0f);
        REQUIRE(ink <= 1.0f);
    }
}
