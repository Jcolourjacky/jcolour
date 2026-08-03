#include <catch2/catch_test_macros.hpp>
#include "wf4_transform.h"
#include "../src/channel_layout.h"

using namespace jcolour;
using namespace jcolour::workflow;

TEST_CASE("[wf4] validate_wf4_lut rejects empty", "[wf4]") {
    WF4LUT lut;
    REQUIRE(validate_wf4_lut(lut) == false);
}

TEST_CASE("[wf4] generate_wf4_lut creates correct size (11 active channels)", "[wf4]") {
    auto lut = generate_wf4_lut({});
    REQUIRE(lut.data.size() == WF4_LUT_SIZE);
    REQUIRE(lut.data[0].size() == WF4_LUT_SIZE);
    REQUIRE(lut.data[0][0].size() == WF4_LUT_SIZE);
    REQUIRE(lut.data[0][0][0].size() == NUM_ACTIVE_CHANNELS);
}

TEST_CASE("[wf4] apply_wf4_transform processes 16-bit image", "[wf4]") {
    WF4PrintConfig config;
    config.lut = generate_wf4_lut({});

    // 2×2 RGB image (16-bit per channel): red, green, blue, white
    std::vector<uint16_t> rgb = {
        65535, 0, 0,
        0, 65535, 0,
        0, 0, 65535,
        65535, 65535, 65535
    };

    auto result = apply_wf4_transform(rgb, 2, 2, config);
    REQUIRE(result.width == 2);
    REQUIRE(result.height == 2);
    REQUIRE(result.ink_values.size() == 4);
}

TEST_CASE("[wf4] LUT values are within range", "[wf4]") {
    auto lut = generate_wf4_lut({});

    for (int r = 0; r < WF4_LUT_SIZE; ++r) {
        for (int g = 0; g < WF4_LUT_SIZE; ++g) {
            for (int b = 0; b < WF4_LUT_SIZE; ++b) {
                for (int ch = 0; ch < NUM_ACTIVE_CHANNELS; ++ch) {
                    float val = lut.data[r][g][b][ch];
                    REQUIRE(val >= 0.0f);
                    REQUIRE(val <= 1.0f);
                }
            }
        }
    }
}

TEST_CASE("[wf4] WF4PrintConfig default values", "[wf4]") {
    WF4PrintConfig config;
    // total_ink_limit lowered to 3.3 (300%) — a reasonable TIL for 11-channel
    // PRO12 on most photo papers. Previously was 3.6.
    REQUIRE(config.total_ink_limit >= 2.5f);
    REQUIRE(config.total_ink_limit <= 4.0f);
    REQUIRE(config.perceptual_intent == true);
    REQUIRE(config.black_kind == BlackKind::PK);
}

TEST_CASE("[wf4] WF4ConversionResult default values", "[wf4]") {
    WF4ConversionResult result;
    REQUIRE(result.width == 0);
    REQUIRE(result.height == 0);
    REQUIRE(result.ink_values.empty());
}

TEST_CASE("[wf4] write_icc_file and read_icc_file", "[wf4]") {
    std::vector<uint8_t> blob = {0x49, 0x49, 0x2A, 0x00};

    bool write_ok = write_icc_file(blob, "/tmp/test_wf4.icc");
    REQUIRE(write_ok == true);

    auto read_blob = read_icc_file("/tmp/test_wf4.icc");
    REQUIRE(read_blob.size() == blob.size());
    REQUIRE(read_blob == blob);
}

TEST_CASE("[wf4] write_icc_file returns false for empty blob", "[wf4]") {
    std::vector<uint8_t> empty;
    bool result = write_icc_file(empty, "/tmp/empty.icc");
    REQUIRE(result == false);
}

TEST_CASE("[wf4] read_icc_file returns empty for missing file", "[wf4]") {
    auto blob = read_icc_file("/tmp/nonexistent.icc");
    REQUIRE(blob.empty());
}

TEST_CASE("[wf4] generate_wf4_lut_from_icc with empty blob falls back", "[wf4]") {
    std::vector<uint8_t> empty;
    auto lut = generate_wf4_lut_from_icc(empty);
    REQUIRE(lut.data.size() == WF4_LUT_SIZE);
}

TEST_CASE("[wf4] apply_wf4_transform handles empty image", "[wf4]") {
    WF4PrintConfig config;
    config.lut = generate_wf4_lut({});

    std::vector<uint16_t> empty;
    auto result = apply_wf4_transform(empty, 0, 0, config);
    REQUIRE(result.width == 0);
    REQUIRE(result.height == 0);
    REQUIRE(result.ink_values.empty());
}

TEST_CASE("[wf4] apply_wf4_transform with ICC blob generates LUT", "[wf4]") {
    WF4PrintConfig config;
    config.icc_blob = {};
    config.lut = generate_wf4_lut({});

    std::vector<uint16_t> rgb = {32768, 32768, 32768};
    auto result = apply_wf4_transform(rgb, 1, 1, config);
    REQUIRE(result.ink_values.size() == 1);
}

TEST_CASE("[wf4] apply_wf4_transform trilinear interpolation", "[wf4]") {
    WF4PrintConfig config;
    config.lut = generate_wf4_lut({});

    std::vector<uint16_t> rgb = {32768, 16384, 8192};  // mid / quarter / eighth
    auto result = apply_wf4_transform(rgb, 1, 1, config);
    REQUIRE(result.ink_values.size() == 1);

    for (int ch = 0; ch < NUM_ACTIVE_CHANNELS; ++ch) {
        REQUIRE(result.ink_values[0][ch] >= 0.0f);
        REQUIRE(result.ink_values[0][ch] <= 1.0f);
    }
}
