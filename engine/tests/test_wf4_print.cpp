#include <catch2/catch_test_macros.hpp>
#include "wf4_print.h"
#include "../src/halftone.h"
#include "../src/channel_layout.h"

using namespace jcolour;
using namespace jcolour::workflow;

namespace {
// Helper: build an ActiveInks initialised to a constant value.
ActiveInks all(float v) {
    ActiveInks a{};
    a.fill(v);
    return a;
}
}  // namespace

TEST_CASE("[wf4-print] halftone_active basic test", "[wf4]") {
    std::vector<ActiveInks> ink_values = {
        all(0.0f),
        all(1.0f),
        all(0.5f),
    };

    std::vector<float> limits(NUM_ACTIVE_CHANNELS, 1.0f);
    // Use a high TIL so it doesn't kick in for this small test.
    auto binary = halftone_active(ink_values, 3, 1, limits, 99.0f);

    REQUIRE(binary.width == 3);
    REQUIRE(binary.height == 1);
    REQUIRE(binary.channels == NUM_ACTIVE_CHANNELS);

    // First pixel: all off
    for (int ch = 0; ch < NUM_ACTIVE_CHANNELS; ++ch) {
        REQUIRE(binary.get(0, 0, ch) == 0);
    }
    // Second pixel: all on (1.0 > 0.5)
    for (int ch = 0; ch < NUM_ACTIVE_CHANNELS; ++ch) {
        REQUIRE(binary.get(1, 0, ch) == 1);
    }
    // Third pixel: ink == 0.5, threshold (> 0.5) → off
    for (int ch = 0; ch < NUM_ACTIVE_CHANNELS; ++ch) {
        REQUIRE(binary.get(2, 0, ch) == 0);
    }
}

TEST_CASE("[wf4-print] pack_binary_to_raster respects black exclusivity", "[wf4][regression]") {
    halftone::ImageMB img(16, 1, NUM_ACTIVE_CHANNELS);
    // Light up MAIN_K (active channel 0)
    img.at(0, 0, idx(ActiveChannel::MAIN_K)) = 1;
    img.at(15, 0, idx(ActiveChannel::MAIN_K)) = 1;

    auto raster = pack_binary_to_raster(img, BlackKind::PK);

    // 16 pixels = 2 bytes per channel per row, 12 physical channels = 24 bytes
    REQUIRE((int)raster.size() == 2 * NUM_PHYSICAL_CHANNELS);

    // Re-pack with MK and confirm the bytes assigned to MK slot now carry
    // the data and PK slot is empty — F2/F3 audit fix.
    auto raster_mk = pack_binary_to_raster(img, BlackKind::MK);
    REQUIRE((int)raster_mk.size() == 2 * NUM_PHYSICAL_CHANNELS);

    // PK slot (index 0) in PK pack: first 2 bytes carry the MAIN_K data.
    // PK slot in MK pack: first 2 bytes should be zero (PK silent).
    REQUIRE(raster[0] != 0);          // PK active in PK config
    REQUIRE(raster_mk[0] == 0);        // PK silent in MK config
    // MK slot (index 1, bytes 2-3) — mirror situation.
    REQUIRE(raster[2] == 0);           // MK silent in PK config
    REQUIRE(raster_mk[2] != 0);        // MK active in MK config
}

TEST_CASE("[wf4-print] build_pjl_header has INKMODE for active black", "[wf4]") {
    auto header_pk = build_pjl_header(1440, 1000, 1440, "A4", BlackKind::PK);
    auto header_mk = build_pjl_header(1440, 1000, 1440, "A4", BlackKind::MK);

    REQUIRE(!header_pk.empty());
    REQUIRE(!header_mk.empty());

    // Headers should differ — INKMODE switches between PHOTOBLACK and MATTE.
    REQUIRE(header_pk != header_mk);

    auto contains = [](const std::vector<uint8_t>& buf, const std::string& s) {
        std::string hay(buf.begin(), buf.end());
        return hay.find(s) != std::string::npos;
    };
    REQUIRE(contains(header_pk, "PHOTOBLACK"));
    REQUIRE(contains(header_mk, "MATTE"));
}

TEST_CASE("[wf4-print] generate_color_gradient produces 16-bit data", "[wf4]") {
    auto gradient = generate_color_gradient(100, 50);
    REQUIRE((int)gradient.size() == 100 * 50 * 3);
}

TEST_CASE("[wf4-print] print_wf4_image basic (16-bit input)", "[wf4]") {
    // 16-bit RGB: red, green, blue
    std::vector<uint16_t> rgb = {65535, 0, 0, 0, 65535, 0, 0, 0, 65535};
    int width = 3;
    int height = 1;

    WF4PrintConfig config;
    config.lut = generate_wf4_lut({});

    WF4PrintConfigEx print_config;
    print_config.dpi = 1440;

    auto result = print_wf4_image(rgb, width, height, config, print_config, "");

    REQUIRE(result.success == true);
    REQUIRE(result.pixels_processed == 3);
    REQUIRE(result.bytes_sent > 0);
}

TEST_CASE("[wf4-print] print_wf4_image invalid input", "[wf4]") {
    WF4PrintConfig config;
    WF4PrintConfigEx print_config;

    std::vector<uint16_t> empty;

    auto result = print_wf4_image(empty, 0, 0, config, print_config, "");

    REQUIRE(result.success == false);
    REQUIRE(!result.error_message.empty());
}

TEST_CASE("[wf4-print] halftone respects ink limits", "[wf4]") {
    std::vector<ActiveInks> ink_values = {{ActiveInks{}}};
    ink_values[0][idx(ActiveChannel::MAIN_K)] = 0.9f;

    // Limit MAIN_K to 0.5
    std::vector<float> limits(NUM_ACTIVE_CHANNELS, 1.0f);
    limits[idx(ActiveChannel::MAIN_K)] = 0.5f;

    auto binary = halftone_active(ink_values, 1, 1, limits, 99.0f);

    // After clamping to 0.5, threshold (> 0.5) → off
    REQUIRE(binary.get(0, 0, idx(ActiveChannel::MAIN_K)) == 0);
}

TEST_CASE("[wf4-print] halftone enforces TIL (F12 regression)", "[wf4][regression]") {
    // Make a pixel that asks for 0.6 ink on every channel — sum = 6.6,
    // way above any sane TIL.
    std::vector<ActiveInks> ink_values = {all(0.6f)};
    std::vector<float> limits(NUM_ACTIVE_CHANNELS, 1.0f);

    // TIL of 3.0 means total can't exceed 300%.
    auto binary = halftone_active(ink_values, 1, 1, limits, 3.0f);

    // Each channel scaled to ~3.0/6.6 * 0.6 = 0.272, below 0.5 threshold,
    // so every channel should be off after TIL enforcement.
    int on_count = 0;
    for (int ch = 0; ch < NUM_ACTIVE_CHANNELS; ++ch) {
        if (binary.get(0, 0, ch) == 1) on_count++;
    }
    REQUIRE(on_count == 0);
}

TEST_CASE("[wf4-print] build_raster_data sizes correctly", "[wf4]") {
    halftone::ImageMB img(8, 2, NUM_ACTIVE_CHANNELS);
    img.at(0, 0, idx(ActiveChannel::MAIN_K)) = 1;

    auto raster = build_raster_data(img, 1440, 8, 2, BlackKind::PK);
    // 8 pixels = 1 byte/channel/row; 2 rows × 12 physical channels = 24
    REQUIRE((int)raster.size() == 24);
}

TEST_CASE("[wf4-print] WF4PrintConfigEx default values", "[wf4]") {
    WF4PrintConfigEx config;
    REQUIRE(config.dpi == 1440);
    REQUIRE(config.page_width_mm == 210.0f);
    REQUIRE(config.page_height_mm == 297.0f);
    REQUIRE(config.black_kind == BlackKind::PK);
}

TEST_CASE("[wf4-print] WF4PrintResult default values", "[wf4]") {
    WF4PrintResult result;
    REQUIRE(result.success == false);
    REQUIRE(result.bytes_sent == 0);
    REQUIRE(result.pixels_processed == 0);
}
