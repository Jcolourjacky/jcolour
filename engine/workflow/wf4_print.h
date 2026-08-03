#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <array>
#include "../src/halftone.h"
#include "../src/channel_layout.h"
#include "wf4_transform.h"
#include "wf4_chart.h"

namespace jcolour {
namespace workflow {

// Print configuration for workflow 4
struct WF4PrintConfigEx {
    int dpi = 1440;
    int width_px = 0;
    int height_px = 0;
    float page_width_mm = 210.0f;
    float page_height_mm = 297.0f;
    bool bidirectional = true;
    std::string paper_type;
    std::vector<float> ink_limits;  // 11 channels (active)
    float total_ink_limit = 3.3f;
    BlackKind black_kind = BlackKind::PK;
};

// Print job result
struct WF4PrintResult {
    bool success = false;
    std::string job_id;
    std::string error_message;
    int bytes_sent = 0;
    int pixels_processed = 0;
};

// Apply vector error diffusion to 11-channel active ink coverage.
// Returns a binary multi-channel image with NUM_ACTIVE_CHANNELS planes.
halftone::ImageMB halftone_active(const std::vector<ActiveInks>& ink_values,
                                   int width, int height,
                                   const std::vector<float>& ink_limits,
                                   float total_ink_limit);

// Convert active 11-channel binary image to PJL physical 12-channel raster.
// Expands each pixel via expand_to_physical(active, kind) before packing.
std::vector<uint8_t> build_raster_data(const halftone::ImageMB& binary_image,
                                        int dpi, int width_px, int height_px,
                                        BlackKind black_kind);

// Print 16-bit RGB image through workflow 4 pipeline
// pipeline: RGB(16-bit) → ICC LUT → 11ch → vector ED → expand to 12ch → raster → PJL
WF4PrintResult print_wf4_image(
    const std::vector<uint16_t>& rgb_image,
    int width, int height,
    const WF4PrintConfig& config,  // from wf4_transform.h
    const WF4PrintConfigEx& print_config,
    const std::string& printer_host);

// Generate test color gradient for printing (16-bit per channel)
std::vector<uint16_t> generate_color_gradient(int width, int height);

// Build PJL header for ESC/P-R print job
std::vector<uint8_t> build_pjl_header(
    int width_px, int height_px,
    int dpi, const std::string& paper_type,
    BlackKind black_kind);

// Convert 11-channel binary image to print-ready format with PK/MK expansion
std::vector<uint8_t> pack_binary_to_raster(const halftone::ImageMB& img,
                                            BlackKind black_kind);

}  // namespace workflow
}  // namespace jcolour
