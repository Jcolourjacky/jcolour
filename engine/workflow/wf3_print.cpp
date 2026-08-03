#include "wf3_print.h"
#include <algorithm>
#include <cmath>

namespace jcolour {
namespace workflow {

std::vector<std::array<float, WF3_NUM_CHANNELS>> grayscale_to_wf3_inks(
    const std::vector<std::vector<float>>& gray_image,
    const Wf3LUT& lut) {

    std::vector<std::array<float, WF3_NUM_CHANNELS>> inks;
    inks.reserve(gray_image.size() * gray_image[0].size());

    for (const auto& row : gray_image) {
        for (float gray : row) {
            // gray = 0 (black) → L* = 0 (black); gray = 1 (white) → L* = 100.
            float lstar = 100.0f * gray;
            auto ink = wf3_lut_lookup(lut, lstar, 0.0f);
            inks.push_back(ink);
        }
    }

    return inks;
}

std::vector<std::array<float, WF3_NUM_CHANNELS>> grayscale_to_split_inks(
    const std::vector<std::vector<float>>& gray_image,
    const Wf3LUT& lut,
    const TintDirection& tint) {

    std::vector<std::array<float, WF3_NUM_CHANNELS>> inks;
    inks.reserve(gray_image.size() * gray_image[0].size());

    for (const auto& row : gray_image) {
        for (float gray : row) {
            float lstar = 100.0f * gray;
            auto ink = wf3_lut_lookup(lut, lstar, tint.strength);
            inks.push_back(ink);
        }
    }

    return inks;
}

std::vector<uint8_t> error_diffusion_halftone_wf3(
    const std::vector<std::array<float, WF3_NUM_CHANNELS>>& inks,
    int width, int height) {

    // Floyd-Steinberg error diffusion for WF3_NUM_CHANNELS (= 6) channels.
    // Pack into bits: WF3_NUM_CHANNELS bits per pixel.
    std::vector<uint8_t> binary;
    int total_pixels = width * height;
    binary.resize((total_pixels * WF3_NUM_CHANNELS + 7) / 8, 0);

    // Zero-init array literal — value-init for arbitrary WF3_NUM_CHANNELS
    // (avoids a brace-list that has to be hand-resized whenever the channel
    // count changes).
    std::array<float, WF3_NUM_CHANNELS> zero_row{};
    std::vector<std::array<float, WF3_NUM_CHANNELS>> errors(width + 2, zero_row);

    for (int y = 0; y < height; ++y) {
        std::fill(errors.begin(), errors.end(), zero_row);

        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            auto ink = inks[idx];

            // Add propagated error
            for (int ch = 0; ch < WF3_NUM_CHANNELS; ++ch) {
                ink[ch] += errors[x + 1][ch];
            }

            // Threshold
            std::array<float, WF3_NUM_CHANNELS> quantized;
            std::array<float, WF3_NUM_CHANNELS> err;
            for (int ch = 0; ch < WF3_NUM_CHANNELS; ++ch) {
                quantized[ch] = ink[ch] > 0.5f ? 1.0f : 0.0f;
                err[ch] = ink[ch] - quantized[ch];
            }

            // Floyd-Steinberg: distribute error to neighbors
            for (int ch = 0; ch < WF3_NUM_CHANNELS; ++ch) {
                if (x + 1 < width) errors[x + 1][ch] += err[ch] * 7.0f / 16.0f;
                if (y + 1 < height) {
                    if (x > 0) errors[x - 1][ch] += err[ch] * 3.0f / 16.0f;
                    errors[x][ch] += err[ch] * 5.0f / 16.0f;
                    if (x + 1 < width) errors[x + 1][ch] += err[ch] * 1.0f / 16.0f;
                }
            }

            // Pack into binary
            int bit_pos = idx * WF3_NUM_CHANNELS;
            int byte_idx = bit_pos / 8;
            int bit_offset = bit_pos % 8;

            for (int ch = 0; ch < WF3_NUM_CHANNELS; ++ch) {
                if (quantized[ch] > 0.5f) {
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

std::vector<uint8_t> build_wf3_raster(
    const std::vector<uint8_t>& binary_data,
    int width, int height, int dpi) {

    std::vector<uint8_t> raster;

    // Add PJL header for 7-channel raster
    raster.push_back(0x1B);  // ESC
    raster.push_back('%');
    raster.push_back('-');
    raster.push_back('1');
    raster.push_back('2');
    raster.push_back('X');

    // Add channel count marker
    raster.push_back(0x1B);
    raster.push_back('(');
    raster.push_back('C');
    raster.push_back('0' + WF3_NUM_CHANNELS);

    // Add raster data
    raster.insert(raster.end(), binary_data.begin(), binary_data.end());

    return raster;
}

Wf3PrintResult print_wf3_image(
    const std::vector<std::vector<float>>& gray_image,
    const Wf3LUT& lut,
    const Wf3PrintConfig& config,
    const std::string& printer_host) {

    Wf3PrintResult result;

    if (gray_image.empty() || gray_image[0].empty()) {
        result.error_message = "Empty image";
        return result;
    }

    int height = static_cast<int>(gray_image.size());
    int width = static_cast<int>(gray_image[0].size());

    // Convert grayscale to 7-channel ink
    auto inks = grayscale_to_wf3_inks(gray_image, lut);

    // Apply error diffusion halftone
    auto binary = error_diffusion_halftone_wf3(inks, width, height);

    // Build raster
    auto raster = build_wf3_raster(binary, width, height, config.dpi);

    result.success = true;
    result.bytes_sent = static_cast<int>(raster.size());
    result.pixels_processed = width * height;
    result.job_id = "wf3-job-" + std::to_string(result.bytes_sent);

    return result;
}

}  // namespace workflow
}  // namespace jcolour