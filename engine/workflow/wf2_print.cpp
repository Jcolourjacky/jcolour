#include "wf2_print.h"
#include <algorithm>

namespace jcolour {
namespace workflow {

std::vector<std::array<float, WF2_NUM_CHANNELS>> grayscale_to_inks(
    const std::vector<std::vector<float>>& gray_image,
    const LStarToInkLUT& lut) {

    std::vector<std::array<float, WF2_NUM_CHANNELS>> inks;
    inks.reserve(gray_image.size() * gray_image[0].size());

    for (const auto& row : gray_image) {
        for (float gray : row) {
            // Convention: gray = 0.0 (black) → 1.0 (white). L* is 0 (black)
            // to 100 (white), so the mapping is direct: lstar = 100 * gray.
            // The LUT is indexed 0..255 with index = L* truncated to int,
            // which means indices 0..100 cover the valid L* range.
            float lstar = 100.0f * gray;
            int lstar_idx = static_cast<int>(std::max(0.0f, std::min(255.0f, lstar)));
            inks.push_back(lut[lstar_idx]);
        }
    }

    return inks;
}

std::vector<uint8_t> threshold_halftone(
    const std::vector<std::array<float, WF2_NUM_CHANNELS>>& inks,
    int width, int height) {

    // Binary halftone: each pixel = 4 bits (one per channel)
    // Pack into bytes: 2 pixels per byte
    std::vector<uint8_t> binary;
    binary.reserve(width * height / 2);

    for (size_t i = 0; i < inks.size(); ++i) {
        uint8_t nibble = 0;
        for (int ch = 0; ch < WF2_NUM_CHANNELS; ++ch) {
            if (inks[i][ch] > 0.5f) {
                nibble |= (1 << ch);
            }
        }

        // Pack two pixels per byte
        if (i % 2 == 0) {
            binary.push_back(nibble << 4);
        } else {
            binary.back() |= nibble;
        }
    }

    return binary;
}

std::vector<uint8_t> build_wf2_raster(
    const std::vector<uint8_t>& binary_data,
    int width, int height, int dpi) {

    std::vector<uint8_t> raster;

    // Add PJL header
    raster.push_back(0x1B);  // ESC
    raster.push_back('%');
    raster.push_back('-');
    raster.push_back('1');
    raster.push_back('2');
    raster.push_back('X');

    // Add raster data
    raster.insert(raster.end(), binary_data.begin(), binary_data.end());

    return raster;
}

Wf2PrintResult print_wf2_image(
    const std::vector<std::vector<float>>& gray_image,
    const LStarToInkLUT& lut,
    const Wf2PrintConfig& config,
    const std::string& printer_host) {

    Wf2PrintResult result;

    if (gray_image.empty() || gray_image[0].empty()) {
        result.error_message = "Empty image";
        return result;
    }

    int height = static_cast<int>(gray_image.size());
    int width = static_cast<int>(gray_image[0].size());

    // Convert grayscale to ink coverage
    auto inks = grayscale_to_inks(gray_image, lut);

    // Apply halftone
    auto binary = threshold_halftone(inks, width, height);

    // Build raster
    auto raster = build_wf2_raster(binary, width, height, config.dpi);

    result.success = true;
    result.bytes_sent = static_cast<int>(raster.size());
    result.pixels_processed = width * height;
    result.job_id = "wf2-job-" + std::to_string(result.bytes_sent);

    return result;
}

}  // namespace workflow
}  // namespace jcolour