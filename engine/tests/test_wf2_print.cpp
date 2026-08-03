#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "wf2_print.h"
#include <algorithm>

using namespace jcolour::workflow;

TEST_CASE("[wf2] grayscale_to_inks converts correctly", "[wf2]") {
    LStarToInkLUT lut;
    for (int i = 0; i < WF2_LUT_SIZE; ++i) {
        lut[i][0] = 0.1f * i / 255.0f;
        lut[i][1] = 0.2f * i / 255.0f;
        lut[i][2] = 0.3f * i / 255.0f;
    }

    std::vector<std::vector<float>> gray = {{0.5f, 0.5f}, {0.5f, 0.5f}};
    auto inks = grayscale_to_inks(gray, lut);

    REQUIRE((int)inks.size() == 4);  // 2x2 image = 4 pixels
}

// Regression: gray→L* used to be inverted (lstar = 100*(1-gray)) which made
// black inputs print as paper white. These tests pin down the correct
// directional mapping so that regression cannot happen silently.
TEST_CASE("[wf2] grayscale_to_inks: black input maps to LUT[0] (max ink)",
          "[wf2][regression]") {
    LStarToInkLUT lut;
    // Build a monotone LUT where index 0 (L*=0, black) holds the max ink
    // and index 100 (L*=100, paper white) holds zero ink — the convention
    // used by generate_wf2_split_lut.
    for (int i = 0; i < WF2_LUT_SIZE; ++i) {
        float frac = std::max(0.0f, (100.0f - static_cast<float>(i)) / 100.0f);
        lut[i][0] = frac * 0.95f;   // MAIN_K: high at black, zero at white
        lut[i][1] = frac * 0.50f;
        lut[i][2] = frac * 0.30f;
    }

    std::vector<std::vector<float>> black_image = {{0.0f}};
    auto inks = grayscale_to_inks(black_image, lut);

    REQUIRE(inks.size() == 1);
    // Black input must request the LUT[0] entry (max ink), not LUT[100] (zero).
    REQUIRE(inks[0][0] == Catch::Approx(lut[0][0]));
    REQUIRE(inks[0][1] == Catch::Approx(lut[0][1]));
    REQUIRE(inks[0][2] == Catch::Approx(lut[0][2]));
    REQUIRE(inks[0][0] > 0.5f);  // Max ink ≈ 0.95
}

TEST_CASE("[wf2] grayscale_to_inks: white input maps to LUT[100] (zero ink)",
          "[wf2][regression]") {
    LStarToInkLUT lut;
    for (int i = 0; i < WF2_LUT_SIZE; ++i) {
        float frac = std::max(0.0f, (100.0f - static_cast<float>(i)) / 100.0f);
        lut[i][0] = frac * 0.95f;
        lut[i][1] = frac * 0.50f;
        lut[i][2] = frac * 0.30f;
    }

    std::vector<std::vector<float>> white_image = {{1.0f}};
    auto inks = grayscale_to_inks(white_image, lut);

    REQUIRE(inks.size() == 1);
    REQUIRE(inks[0][0] == Catch::Approx(0.0f));
    REQUIRE(inks[0][1] == Catch::Approx(0.0f));
    REQUIRE(inks[0][2] == Catch::Approx(0.0f));
}

TEST_CASE("[wf2] grayscale_to_inks: midtone gives midtone ink", "[wf2][regression]") {
    LStarToInkLUT lut;
    for (int i = 0; i < WF2_LUT_SIZE; ++i) {
        float frac = std::max(0.0f, (100.0f - static_cast<float>(i)) / 100.0f);
        lut[i][0] = frac;
        lut[i][1] = frac * 0.5f;
        lut[i][2] = 0.0f;
    }

    std::vector<std::vector<float>> mid_image = {{0.5f}};
    auto inks = grayscale_to_inks(mid_image, lut);

    REQUIRE(inks.size() == 1);
    // gray=0.5 → L*=50 → LUT[50] → frac = 0.5
    REQUIRE(inks[0][0] == Catch::Approx(0.5f).margin(0.02f));
    REQUIRE(inks[0][1] == Catch::Approx(0.25f).margin(0.02f));
}

TEST_CASE("[wf2] threshold_halftone produces binary output", "[wf2]") {
    std::vector<std::array<float, WF2_NUM_CHANNELS>> inks;
    inks.push_back({0.8f, 0.2f, 0.3f});
    inks.push_back({0.1f, 0.9f, 0.1f});
    inks.push_back({0.5f, 0.5f, 0.5f});
    inks.push_back({0.0f, 0.0f, 0.0f});

    auto binary = threshold_halftone(inks, 2, 2);
    REQUIRE(!binary.empty());
}

TEST_CASE("[wf2] build_wf2_raster includes PJL header", "[wf2]") {
    std::vector<uint8_t> data = {0x00, 0x01, 0x02};
    auto raster = build_wf2_raster(data, 8, 2, 1440);

    REQUIRE(raster.size() > data.size());
    // First bytes should be PJL escape sequence
    REQUIRE(raster[0] == 0x1B);
}

TEST_CASE("[wf2] print_wf2_image invalid input", "[wf2]") {
    LStarToInkLUT lut;
    Wf2PrintConfig config;
    std::vector<std::vector<float>> empty;

    auto result = print_wf2_image(empty, lut, config, "");
    REQUIRE(result.success == false);
    REQUIRE(!result.error_message.empty());
}

TEST_CASE("[wf2] print_wf2_image produces result", "[wf2]") {
    LStarToInkLUT lut;
    for (int i = 0; i < WF2_LUT_SIZE; ++i) {
        lut[i][0] = 0.5f;
        lut[i][1] = 0.3f;
        lut[i][2] = 0.2f;
    }

    Wf2PrintConfig config;
    config.dpi = 1440;

    std::vector<std::vector<float>> image = {{0.5f, 0.5f, 0.5f, 0.5f}};

    auto result = print_wf2_image(image, lut, config, "");
    REQUIRE(result.success == true);
    REQUIRE(result.pixels_processed == 4);
}

TEST_CASE("[wf2] Wf2PrintConfig default values", "[wf2]") {
    Wf2PrintConfig config;
    REQUIRE(config.dpi == 1440);
    REQUIRE(config.page_width_mm == 210.0f);
    REQUIRE(config.page_height_mm == 297.0f);
}

TEST_CASE("[wf2] Wf2PrintResult default values", "[wf2]") {
    Wf2PrintResult result;
    REQUIRE(result.success == false);
    REQUIRE(result.bytes_sent == 0);
}