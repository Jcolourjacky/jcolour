#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "wf6_print.h"
#include "wf6_chart.h"

using namespace jcolour::workflow;

TEST_CASE("wf6_print: rgb_to_wf6_inks", "[wf6][wf6-print]") {
    std::vector<std::vector<std::array<float, 3>>> rgb_image = {
        {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}
    };

    Wf6LUT lut;
    lut.size = 33;
    lut.channels = WF6_NUM_CHANNELS;
    lut.data.resize(lut.size * lut.size * lut.size * WF6_NUM_CHANNELS, 0.5f);

    auto inks = rgb_to_wf6_inks(rgb_image, lut);

    REQUIRE(inks.size() == 2);
    REQUIRE(inks[0].size() == WF6_NUM_CHANNELS);
}

TEST_CASE("wf6_print: error_diffusion_halftone_wf6", "[wf6][wf6-print]") {
    std::vector<std::array<float, WF6_NUM_CHANNELS>> inks(100);
    for (auto& ink : inks) {
        for (auto& v : ink) {
            v = 0.5f;
        }
    }

    auto binary = error_diffusion_halftone_wf6(inks, 10, 10);

    // 10×10 = 100 pixels, WF6_NUM_CHANNELS bits/pixel, rounded up to bytes.
    // After the 11-active refactor: 100*11 = 1100 bits = 138 bytes.
    size_t expected = (100UL * WF6_NUM_CHANNELS + 7) / 8;
    REQUIRE(binary.size() == expected);
}

TEST_CASE("wf6_print: print_wf6_image success", "[wf6][wf6-print]") {
    std::vector<std::vector<std::array<float, 3>>> rgb_image(10, std::vector<std::array<float, 3>>(10));
    for (auto& row : rgb_image) {
        for (auto& pixel : row) {
            pixel = {0.5f, 0.5f, 0.5f};
        }
    }

    Wf6LUT lut;
    lut.size = 33;
    lut.channels = WF6_NUM_CHANNELS;
    lut.data.resize(lut.size * lut.size * lut.size * WF6_NUM_CHANNELS, 0.3f);

    Wf6PrintConfig config;
    config.dpi = 1440;

    auto result = print_wf6_image(rgb_image, lut, config, "localhost");

    CHECK(result.success == true);
    CHECK(result.bytes_sent > 0);
    CHECK(result.pixels_processed == 100);
}

TEST_CASE("wf6_print: build_wf6_raster header", "[wf6][wf6-print]") {
    std::vector<uint8_t> data = {0xAA, 0xBB};

    auto raster = build_wf6_raster(data, 8, 8, 1440);

    CHECK(raster[0] == 0x1B);  // ESC
    CHECK(raster[1] == '%');
}
