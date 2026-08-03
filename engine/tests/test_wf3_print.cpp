#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "wf3_print.h"
#include "wf3_lut.h"
#include "wf3_measure.h"

using namespace jcolour::workflow;

TEST_CASE("wf3_print: grayscale_to_wf3_inks", "[wf3][wf3-print]") {
    std::vector<std::vector<float>> gray_image = {
        {0.0f, 0.5f, 1.0f}  // black, mid-gray, white
    };

    Wf3LUT lut;
    lut.entries.resize(256);
    for (int i = 0; i < 256; ++i) {
        float lstar = 100.0f * (1.0f - i / 255.0f);
        for (int ch = 0; ch < WF3_NUM_CHANNELS; ++ch) {
            lut.entries[i].inks[ch] = 0.01f * i;
        }
    }

    auto inks = grayscale_to_wf3_inks(gray_image, lut);

    REQUIRE(inks.size() == 3);
    REQUIRE(inks[0].size() == WF3_NUM_CHANNELS);
    REQUIRE(inks[1].size() == WF3_NUM_CHANNELS);
    REQUIRE(inks[2].size() == WF3_NUM_CHANNELS);
}

TEST_CASE("wf3_print: build_wf3_raster", "[wf3][wf3-print]") {
    std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC};

    auto raster = build_wf3_raster(data, 8, 8, 1440);

    REQUIRE(raster.size() > data.size());
    // Check PJL header
    CHECK(raster[0] == 0x1B);  // ESC
    CHECK(raster[1] == '%');
}

TEST_CASE("wf3_print: print_wf3_image success", "[wf3][wf3-print]") {
    std::vector<std::vector<float>> gray_image(10, std::vector<float>(10, 0.5f));

    Wf3LUT lut;
    lut.entries.resize(256);
    for (int i = 0; i < 256; ++i) {
        for (int ch = 0; ch < WF3_NUM_CHANNELS; ++ch) {
            lut.entries[i].inks[ch] = 0.5f;
        }
    }

    Wf3PrintConfig config;
    config.dpi = 1440;
    config.width_px = 10;
    config.height_px = 10;

    auto result = print_wf3_image(gray_image, lut, config, "localhost");

    CHECK(result.success == true);
    CHECK(result.bytes_sent > 0);
    CHECK(result.pixels_processed == 100);
    CHECK(!result.job_id.empty());
}

TEST_CASE("wf3_print: print_wf3_image empty image", "[wf3][wf3-print]") {
    std::vector<std::vector<float>> empty;

    Wf3LUT lut;
    Wf3PrintConfig config;

    auto result = print_wf3_image(empty, lut, config, "localhost");

    CHECK(result.success == false);
    CHECK(result.error_message == "Empty image");
}
