#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "wf3_lut.h"

namespace jcolour {
namespace workflow {

// Print configuration for workflow 3
struct Wf3PrintConfig {
    int dpi = 1440;
    int width_px = 0;
    int height_px = 0;
    float page_width_mm = 210.0f;
    float page_height_mm = 297.0f;
    bool bidirectional = true;
    std::string paper_type;
    bool use_split_tone = true;
    TonePreset preset = TonePreset::NEUTRAL;
};

// Print job result
struct Wf3PrintResult {
    bool success = false;
    std::string job_id;
    std::string error_message;
    int bytes_sent = 0;
    int pixels_processed = 0;
};

// Convert grayscale image to 7-channel ink using LUT
std::vector<std::array<float, WF3_NUM_CHANNELS>> grayscale_to_wf3_inks(
    const std::vector<std::vector<float>>& gray_image,
    const Wf3LUT& lut);

// Convert grayscale to split-tone ink using LUT with tint
std::vector<std::array<float, WF3_NUM_CHANNELS>> grayscale_to_split_inks(
    const std::vector<std::vector<float>>& gray_image,
    const Wf3LUT& lut,
    const TintDirection& tint);

// Apply error diffusion halftone for 7 channels
std::vector<uint8_t> error_diffusion_halftone_wf3(
    const std::vector<std::array<float, WF3_NUM_CHANNELS>>& inks,
    int width, int height);

// Build raster data for workflow 3 (7-channel binary)
std::vector<uint8_t> build_wf3_raster(
    const std::vector<uint8_t>& binary_data,
    int width, int height, int dpi);

// Print grayscale image through workflow 3
Wf3PrintResult print_wf3_image(
    const std::vector<std::vector<float>>& gray_image,
    const Wf3LUT& lut,
    const Wf3PrintConfig& config,
    const std::string& printer_host);

}  // namespace workflow
}  // namespace jcolour