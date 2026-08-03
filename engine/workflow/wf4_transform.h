#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <array>

#include "wf4_chart.h"
#include "../src/channel_layout.h"

namespace jcolour {

// Forward declare ColorPipeline from color_pipeline.h
class ColorPipeline;

namespace workflow {

// LUT dimensions for 11-channel workflow
constexpr int WF4_LUT_SIZE = 33;

// RGB to 11-channel LUT type
// Access: lut[r][g][b][channel] where r,g,b are 0..32 and channel is 0..10
struct WF4LUT {
    std::vector<std::vector<std::vector<ActiveInks>>> data;
};

// Workflow 4 print configuration
struct WF4PrintConfig {
    std::vector<uint8_t> icc_blob;
    WF4LUT lut;
    std::vector<float> ink_limits;        // Per-channel, length = NUM_ACTIVE_CHANNELS
    float total_ink_limit = 3.3f;          // 300% reasonable for 11-channel PRO12
    BlackKind black_kind = BlackKind::PK;
    bool perceptual_intent = true;
};

// Test RGB image conversion to 11-channel ink
struct WF4ConversionResult {
    std::vector<ActiveInks> ink_values;  // Per-pixel ink coverage (11 channels)
    int width = 0;
    int height = 0;
};

// Apply ICC transform to a 16-bit RGB image (PIPELINE §7 contract: 16-bit
// all the way through). One pixel = 3 uint16_t values, channels packed
// R/G/B per pixel.
WF4ConversionResult apply_wf4_transform(
    const std::vector<uint16_t>& rgb_image,
    int width, int height,
    const WF4PrintConfig& config);

// Validate LUT dimensions
bool validate_wf4_lut(const WF4LUT& lut);

// Generate LUT from ICC profile (placeholder; uses fallback if blob empty)
WF4LUT generate_wf4_lut(const std::vector<uint8_t>& icc_blob);

// Generate LUT from ICC blob using ColorPipeline
WF4LUT generate_wf4_lut_from_icc(const std::vector<uint8_t>& icc_blob);

// Write ICC blob to file
bool write_icc_file(const std::vector<uint8_t>& icc_blob, const std::string& path);

// Read ICC file into blob
std::vector<uint8_t> read_icc_file(const std::string& path);

}  // namespace workflow
}  // namespace jcolour
