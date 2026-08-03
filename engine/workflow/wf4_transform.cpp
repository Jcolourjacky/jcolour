#include "wf4_transform.h"
#include "../src/color_pipeline.h"
#include <algorithm>
#include <fstream>

namespace jcolour {
namespace workflow {

bool validate_wf4_lut(const WF4LUT& lut) {
    if (lut.data.empty()) return false;
    if (lut.data[0].empty()) return false;
    if (lut.data[0][0].empty()) return false;
    return true;
}

bool write_icc_file(const std::vector<uint8_t>& icc_blob, const std::string& path) {
    if (icc_blob.empty()) return false;
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(icc_blob.data()), icc_blob.size());
    return out.good();
}

std::vector<uint8_t> read_icc_file(const std::string& path) {
    std::vector<uint8_t> blob;
    std::ifstream in(path, std::ios::binary);
    if (!in) return blob;
    in.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(in.tellg());
    if (size == 0) return blob;
    blob.resize(size);
    in.seekg(0, std::ios::beg);
    in.read(reinterpret_cast<char*>(blob.data()), size);
    return blob;
}

// Fallback CMYK-style separation when no ICC is available.
// Writes to MAIN_K + C + VM + Y only. The remaining 7 active channels
// (LC, VLM, Or, GY, LGY, V, Gr) stay at zero.
//
// This is intentionally a 4-channel fallback. A "true" 11-channel
// separation requires either an ICC or a separation model (K-M / CYNSN);
// neither is appropriate to fake at this layer. The fallback exists so a
// missing-ICC config still produces *something* on the page rather than
// crashing.
static ActiveInks fallback_separate(float r, float g, float b) {
    float c = 1.0f - r;
    float m = 1.0f - g;
    float y = 1.0f - b;
    float k = std::min({c, m, y});

    ActiveInks inks{};
    inks[idx(ActiveChannel::MAIN_K)] = k;
    inks[idx(ActiveChannel::C)]      = std::max(0.0f, c - k);
    inks[idx(ActiveChannel::VM)]     = std::max(0.0f, m - k);
    inks[idx(ActiveChannel::Y)]      = std::max(0.0f, y - k);
    return inks;
}

WF4LUT generate_wf4_lut_from_icc(const std::vector<uint8_t>& icc_blob) {
    WF4LUT lut;

    if (icc_blob.empty()) {
        return generate_wf4_lut(icc_blob);
    }

    // Write ICC to temp file for LCMS2 to load via ColorPipeline
    std::string temp_path = "/tmp/wf4_lut.icc";
    if (!write_icc_file(icc_blob, temp_path)) {
        return generate_wf4_lut(icc_blob);
    }

    ColorPipeline pipeline;
    pipeline.set_destination_profile(temp_path.c_str());

    // Build LUT by transforming RGB grid through ICC, staying in 16-bit
    // throughout (PIPELINE §7 contract).
    lut.data.resize(WF4_LUT_SIZE);
    for (int r = 0; r < WF4_LUT_SIZE; ++r) {
        lut.data[r].resize(WF4_LUT_SIZE);
        for (int g = 0; g < WF4_LUT_SIZE; ++g) {
            lut.data[r][g].resize(WF4_LUT_SIZE);
            for (int b = 0; b < WF4_LUT_SIZE; ++b) {
                std::vector<uint16_t> rgb_pixel = {
                    static_cast<uint16_t>(r * 65535 / (WF4_LUT_SIZE - 1)),
                    static_cast<uint16_t>(g * 65535 / (WF4_LUT_SIZE - 1)),
                    static_cast<uint16_t>(b * 65535 / (WF4_LUT_SIZE - 1))
                };

                // Transform through ICC. ColorPipeline returns CMYK (or
                // however the destination profile is configured) — for the
                // PRO12 profile it should return the full 11-channel device
                // values, but until ColorPipeline is extended for that, we
                // fall back to a CMYK→11ch shim.
                //
                // TODO: extend ColorPipeline to expose all output channels
                // of an N-channel destination profile. Right now it assumes
                // 4-channel CMYK output, which is the F4 limitation flagged
                // in the audit.
                auto cmyk = pipeline.transform_tiff(rgb_pixel, 1, 1, 3);

                ActiveInks inks{};
                if (cmyk.size() >= 4) {
                    float c = cmyk[0] / 65535.0f;
                    float m = cmyk[1] / 65535.0f;
                    float y = cmyk[2] / 65535.0f;
                    float k = cmyk[3] / 65535.0f;

                    inks[idx(ActiveChannel::MAIN_K)] = k * 0.95f;
                    inks[idx(ActiveChannel::C)]      = c * 0.9f;
                    inks[idx(ActiveChannel::VM)]     = m * 0.9f;
                    inks[idx(ActiveChannel::Y)]      = y * 0.9f;
                }

                lut.data[r][g][b] = inks;
            }
        }
    }

    return lut;
}

WF4LUT generate_wf4_lut(const std::vector<uint8_t>& /*icc_blob*/) {
    WF4LUT lut;
    lut.data.resize(WF4_LUT_SIZE);

    for (int r = 0; r < WF4_LUT_SIZE; ++r) {
        lut.data[r].resize(WF4_LUT_SIZE);
        for (int g = 0; g < WF4_LUT_SIZE; ++g) {
            lut.data[r][g].resize(WF4_LUT_SIZE);
            for (int b = 0; b < WF4_LUT_SIZE; ++b) {
                float rf = r / static_cast<float>(WF4_LUT_SIZE - 1);
                float gf = g / static_cast<float>(WF4_LUT_SIZE - 1);
                float bf = b / static_cast<float>(WF4_LUT_SIZE - 1);
                lut.data[r][g][b] = fallback_separate(rf, gf, bf);
            }
        }
    }

    return lut;
}

WF4ConversionResult apply_wf4_transform(
    const std::vector<uint16_t>& rgb_image,
    int width, int height,
    const WF4PrintConfig& config) {

    WF4ConversionResult result;
    result.width = width;
    result.height = height;

    if (width <= 0 || height <= 0 || rgb_image.empty()) {
        return result;
    }

    // Use LUT from config, or generate fallback
    WF4LUT lut_to_use = config.lut;
    if (lut_to_use.data.empty()) {
        lut_to_use = generate_wf4_lut(config.icc_blob);
    }

    // Trilinear interpolation LUT lookup, 16-bit input throughout.
    for (size_t i = 0; i + 2 < rgb_image.size(); i += 3) {
        float rf = rgb_image[i]     / 65535.0f;
        float gf = rgb_image[i + 1] / 65535.0f;
        float bf = rgb_image[i + 2] / 65535.0f;

        float r_idx_f = rf * (WF4_LUT_SIZE - 1);
        float g_idx_f = gf * (WF4_LUT_SIZE - 1);
        float b_idx_f = bf * (WF4_LUT_SIZE - 1);

        int r0 = static_cast<int>(r_idx_f);
        int g0 = static_cast<int>(g_idx_f);
        int b0 = static_cast<int>(b_idx_f);
        int r1 = std::min(r0 + 1, WF4_LUT_SIZE - 1);
        int g1 = std::min(g0 + 1, WF4_LUT_SIZE - 1);
        int b1 = std::min(b0 + 1, WF4_LUT_SIZE - 1);

        float r_frac = r_idx_f - r0;
        float g_frac = g_idx_f - g0;
        float b_frac = b_idx_f - b0;

        ActiveInks interp_inks{};
        for (int ch = 0; ch < NUM_ACTIVE_CHANNELS; ++ch) {
            if (r0 >= static_cast<int>(lut_to_use.data.size()) ||
                g0 >= static_cast<int>(lut_to_use.data[r0].size()) ||
                b0 >= static_cast<int>(lut_to_use.data[r0][g0].size())) {
                interp_inks[ch] = 0.0f;
                continue;
            }

            float v000 = lut_to_use.data[r0][g0][b0][ch];
            float v001 = lut_to_use.data[r0][g0][b1][ch];
            float v010 = lut_to_use.data[r0][g1][b0][ch];
            float v011 = lut_to_use.data[r0][g1][b1][ch];
            float v100 = lut_to_use.data[r1][g0][b0][ch];
            float v101 = lut_to_use.data[r1][g0][b1][ch];
            float v110 = lut_to_use.data[r1][g1][b0][ch];
            float v111 = lut_to_use.data[r1][g1][b1][ch];

            float v00 = v000 * (1 - r_frac) + v100 * r_frac;
            float v01 = v001 * (1 - r_frac) + v101 * r_frac;
            float v10 = v010 * (1 - r_frac) + v110 * r_frac;
            float v11 = v011 * (1 - r_frac) + v111 * r_frac;

            float v0 = v00 * (1 - g_frac) + v10 * g_frac;
            float v1 = v01 * (1 - g_frac) + v11 * g_frac;

            float v = v0 * (1 - b_frac) + v1 * b_frac;
            interp_inks[ch] = std::max(0.0f, std::min(1.0f, v));
        }

        result.ink_values.push_back(interp_inks);
    }

    return result;
}

}  // namespace workflow
}  // namespace jcolour
