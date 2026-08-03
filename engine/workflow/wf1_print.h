#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "wf1_lut.h"

namespace jcolour {
namespace workflow {

// Print job configuration
struct PrintConfig {
    int         dpi = 1440;
    int         width_px = 0;
    int         height_px = 0;
    float       page_width_mm = 210.0f;   // A4 width
    float       page_height_mm = 297.0f;  // A4 height
    bool        bidirectional = true;
    std::string paper_type;
};

// Result of a print job
struct PrintResult {
    bool        success = false;
    std::string job_id;
    std::string error_message;
    int         bytes_sent = 0;
};

// Convert grayscale image to binary halftone using the LUT
std::vector<uint8_t> gray_to_binary(const std::vector<float>& gray_values,
                                     const LStarToInkLUT& lut);

// Print pipeline: gray input → LUT lookup → halftone → PJL send
PrintResult print_gray_image(const std::vector<std::vector<float>>& gray_image,
                             const LStarToInkLUT& lut,
                             const PrintConfig& config,
                             const std::string& printer_host);

// Generate test gradient image (for testing without real image input)
std::vector<std::vector<float>> generate_gradient_test(int width, int height);

}  // namespace workflow
}  // namespace jcolour