#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "wf2_lut.h"

namespace jcolour {
namespace workflow {

// Print configuration for workflow 2
struct Wf2PrintConfig {
    int dpi = 1440;
    int width_px = 0;
    int height_px = 0;
    float page_width_mm = 210.0f;
    float page_height_mm = 297.0f;
    bool bidirectional = true;
    std::string paper_type;
};

// Print job result
struct Wf2PrintResult {
    bool success = false;
    std::string job_id;
    std::string error_message;
    int bytes_sent = 0;
    int pixels_processed = 0;
};

// Convert grayscale image to 4-channel ink using LUT
std::vector<std::array<float, WF2_NUM_CHANNELS>> grayscale_to_inks(
    const std::vector<std::vector<float>>& gray_image,
    const LStarToInkLUT& lut);

// Apply threshold halftone to single channel
std::vector<uint8_t> threshold_halftone(
    const std::vector<std::array<float, WF2_NUM_CHANNELS>>& inks,
    int width, int height);

// Build raster data for workflow 2
std::vector<uint8_t> build_wf2_raster(
    const std::vector<uint8_t>& binary_data,
    int width, int height, int dpi);

// Print grayscale image through workflow 2
Wf2PrintResult print_wf2_image(
    const std::vector<std::vector<float>>& gray_image,
    const LStarToInkLUT& lut,
    const Wf2PrintConfig& config,
    const std::string& printer_host);

}  // namespace workflow
}  // namespace jcolour