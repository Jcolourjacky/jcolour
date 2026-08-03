#include "wf1_print.h"
#include "../epson/epson.h"
#include <algorithm>

namespace jcolour {
namespace workflow {

std::vector<uint8_t> gray_to_binary(const std::vector<float>& gray_values,
                                     const LStarToInkLUT& lut) {
    std::vector<uint8_t> binary;
    binary.reserve(gray_values.size());

    for (float gray : gray_values) {
        // Convention: gray = 0.0 (black) → 1.0 (white). The LUT is indexed
        // by L*, where L*=0 is black and L*=100 is white, so the mapping is
        // direct: lstar = 100 * gray.
        float lstar = 100.0f * gray;
        float ink = lut.lookup(lstar);

        // Simple threshold for binary output (no actual halftone yet)
        binary.push_back(ink > 0.5f ? 255 : 0);
    }

    return binary;
}

PrintResult print_gray_image(const std::vector<std::vector<float>>& gray_image,
                             const LStarToInkLUT& lut,
                             const PrintConfig& config,
                             const std::string& printer_host) {
    PrintResult result;

    // Validate inputs
    if (gray_image.empty() || gray_image[0].empty()) {
        result.error_message = "Empty image";
        return result;
    }

    int height = static_cast<int>(gray_image.size());
    int width = static_cast<int>(gray_image[0].size());

    // Build binary data row by row
    std::vector<uint8_t> raster_data;
    raster_data.reserve(width * height / 8);  // 1-bit so divide by 8

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float gray = gray_image[y][x];
            // Clamp to valid range
            gray = std::clamp(gray, 0.0f, 1.0f);

            // Convert to L* then to ink via LUT (gray=0 black → L*=0;
            // gray=1 white → L*=100). See gray_to_binary() above.
            float lstar = 100.0f * gray;
            float ink = lut.lookup(lstar);

            // Convert to binary (threshold at 0.5)
            uint8_t bit = (ink > 0.5f) ? 1 : 0;

            // Pack into bytes (MSB first)
            if (x % 8 == 0) {
                raster_data.push_back(0);
            }
            if (bit) {
                raster_data[raster_data.size() - 1] |= (0x80 >> (x % 8));
            }
        }
    }

    result.bytes_sent = static_cast<int>(raster_data.size());

    // TODO: Send via epson::Printer when we have a real printer connection
    // For now, return success with byte count
    result.success = true;
    result.job_id = "mock-job-" + std::to_string(result.bytes_sent);

    (void)config;
    (void)printer_host;

    return result;
}

std::vector<std::vector<float>> generate_gradient_test(int width, int height) {
    std::vector<std::vector<float>> image;
    image.reserve(height);

    for (int y = 0; y < height; ++y) {
        std::vector<float> row;
        row.reserve(width);

        for (int x = 0; x < width; ++x) {
            // Horizontal gradient from white (1.0) to black (0.0)
            float gray = 1.0f - (static_cast<float>(x) / static_cast<float>(width));
            row.push_back(gray);
        }

        image.push_back(row);
    }

    return image;
}

}  // namespace workflow
}  // namespace jcolour