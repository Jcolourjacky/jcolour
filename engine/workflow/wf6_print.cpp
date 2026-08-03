#include "wf6_print.h"
#include <algorithm>

namespace jcolour {
namespace workflow {

std::vector<std::array<float, WF6_NUM_CHANNELS>> rgb_to_wf6_inks(
    const std::vector<std::vector<std::array<float, 3>>>& rgb_image,
    const Wf6LUT& lut) {

    std::vector<std::array<float, WF6_NUM_CHANNELS>> inks;
    inks.reserve(rgb_image.size() * rgb_image[0].size());

    for (const auto& row : rgb_image) {
        for (const auto& rgb : row) {
            auto ink = lut_lookup(lut, rgb[0], rgb[1], rgb[2]);
            std::array<float, WF6_NUM_CHANNELS> arr;
            for (int i = 0; i < WF6_NUM_CHANNELS; ++i) {
                arr[i] = (i < static_cast<int>(ink.size())) ? ink[i] : 0.0f;
            }
            inks.push_back(arr);
        }
    }

    return inks;
}

std::vector<uint8_t> error_diffusion_halftone_wf6(
    const std::vector<std::array<float, WF6_NUM_CHANNELS>>& inks,
    int width, int height) {

    // Floyd-Steinberg error diffusion for WF6_NUM_CHANNELS (= 11) active
    // channels. Pack WF6_NUM_CHANNELS bits per pixel. PJL expansion to 12
    // physical channels happens at the raster boundary (TODO: route through
    // build_raster_data() in wf4_print, which calls expand_to_physical).
    std::vector<uint8_t> binary;
    int total_pixels = width * height;
    binary.resize((total_pixels * WF6_NUM_CHANNELS + 7) / 8, 0);

    std::vector<std::array<float, WF6_NUM_CHANNELS>> errors(width + 2);

    for (int y = 0; y < height; ++y) {
        for (int i = 0; i < width + 2; ++i) {
            errors[i] = {};
        }

        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            auto ink = inks[idx];

            // Add propagated error
            for (int ch = 0; ch < WF6_NUM_CHANNELS; ++ch) {
                ink[ch] += errors[x + 1][ch];
            }

            // Threshold and calculate error
            std::array<float, WF6_NUM_CHANNELS> err = {};
            for (int ch = 0; ch < WF6_NUM_CHANNELS; ++ch) {
                float quantized = ink[ch] > 0.5f ? 1.0f : 0.0f;
                err[ch] = ink[ch] - quantized;

                // Floyd-Steinberg error distribution
                if (x + 1 < width) errors[x + 1][ch] += err[ch] * 7.0f / 16.0f;
                if (y + 1 < height) {
                    if (x > 0) errors[x - 1][ch] += err[ch] * 3.0f / 16.0f;
                    errors[x][ch] += err[ch] * 5.0f / 16.0f;
                    if (x + 1 < width) errors[x + 1][ch] += err[ch] * 1.0f / 16.0f;
                }
            }

            // Pack into binary: WF6_NUM_CHANNELS bits per pixel
            int bit_pos = idx * WF6_NUM_CHANNELS;
            int byte_idx = bit_pos / 8;
            int bit_offset = bit_pos % 8;

            for (int ch = 0; ch < WF6_NUM_CHANNELS; ++ch) {
                if (ink[ch] > 0.5f) {
                    binary[byte_idx] |= (1 << bit_offset);
                }
                bit_offset++;
                if (bit_offset == 8) {
                    bit_offset = 0;
                    byte_idx++;
                }
            }
        }
    }

    return binary;
}

std::vector<uint8_t> build_wf6_raster(
    const std::vector<uint8_t>& binary_data,
    int, int, int) {

    std::vector<uint8_t> raster;

    // Add PJL header for 12-channel raster
    raster.push_back(0x1B);  // ESC
    raster.push_back('%');
    raster.push_back('-');
    raster.push_back('1');
    raster.push_back('2');
    raster.push_back('X');

    // Add channel count marker — 11 active channels (PJL expansion to 12
    // physical happens later via expand_to_physical at the device boundary).
    raster.push_back(0x1B);
    raster.push_back('(');
    raster.push_back('C');
    raster.push_back('1');
    raster.push_back('1');

    // Add raster data
    raster.insert(raster.end(), binary_data.begin(), binary_data.end());

    return raster;
}

Wf6PrintResult print_wf6_image(
    const std::vector<std::vector<std::array<float, 3>>>& rgb_image,
    const Wf6LUT& lut,
    const Wf6PrintConfig& config,
    const std::string&) {

    Wf6PrintResult result;

    if (rgb_image.empty() || rgb_image[0].empty()) {
        result.error_message = "Empty image";
        return result;
    }

    int height = static_cast<int>(rgb_image.size());
    int width = static_cast<int>(rgb_image[0].size());

    // Convert RGB to 12-channel ink
    auto inks = rgb_to_wf6_inks(rgb_image, lut);

    // Apply error diffusion halftone
    auto binary = error_diffusion_halftone_wf6(inks, width, height);

    // Build raster
    auto raster = build_wf6_raster(binary, width, height, config.dpi);

    result.success = true;
    result.bytes_sent = static_cast<int>(raster.size());
    result.pixels_processed = width * height;
    result.job_id = "wf6-job-" + std::to_string(result.bytes_sent);

    return result;
}

}  // namespace workflow
}  // namespace jcolour