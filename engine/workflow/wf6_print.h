#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "wf6_chart.h"

namespace jcolour {
namespace workflow {

// Print configuration for workflow 6 (ProPhoto RGB + 12 channel)
struct Wf6PrintConfig {
    int dpi = 1440;
    int width_px = 0;
    int height_px = 0;
    float page_width_mm = 210.0f;
    float page_height_mm = 297.0f;
    bool bidirectional = true;
    std::string paper_type;
    bool use_gamut_compression = true;
    bool perceptual_rendering = true;
};

// Print job result
struct Wf6PrintResult {
    bool success = false;
    std::string job_id;
    std::string error_message;
    int bytes_sent = 0;
    int pixels_processed = 0;
};

// Convert RGB image to 12-channel ink using LUT
std::vector<std::array<float, WF6_NUM_CHANNELS>> rgb_to_wf6_inks(
    const std::vector<std::vector<std::array<float, 3>>>& rgb_image,
    const Wf6LUT& lut);

// Apply error diffusion halftone for 12 channels
std::vector<uint8_t> error_diffusion_halftone_wf6(
    const std::vector<std::array<float, WF6_NUM_CHANNELS>>& inks,
    int width, int height);

// Build raster data for workflow 6 (12-channel binary)
std::vector<uint8_t> build_wf6_raster(
    const std::vector<uint8_t>& binary_data,
    int width, int height, int dpi);

// Print RGB image through workflow 6
Wf6PrintResult print_wf6_image(
    const std::vector<std::vector<std::array<float, 3>>>& rgb_image,
    const Wf6LUT& lut,
    const Wf6PrintConfig& config,
    const std::string& printer_host);

}  // namespace workflow
}  // namespace jcolour