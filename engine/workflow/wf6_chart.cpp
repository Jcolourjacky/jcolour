#include "wf6_chart.h"
#include "../src/spectral.h"
#include "../src/delta_e.h"
#include "../src/color_pipeline.h"

#include <lcms2.h>
#include <algorithm>
#include <cmath>
#include <random>

namespace jcolour {
namespace workflow {

Wf6ChartLayout generate_wf6_calibration_chart() {
    Wf6ChartLayout layout;

    // Reuse workflow 4 chart as base
    layout.base_chart = generate_wf4_calibration_chart();

    // Workflow 6 gamut is the union of:
    // 1. ProPhoto RGB gamut (very large)
    // 2. Printer achievable gamut
    // For now, initialize empty hull
    layout.gamut_compression_enabled = true;

    return layout;
}

std::vector<std::array<float, 3>> generate_prophoto_test_points(int count) {
    std::vector<std::array<float, 3>> points;

    // Generate points that explore the full ProPhoto gamut
    // ProPhoto RGB primaries:
    // R: (0.7347, 0.2653), G: (0.1596, 0.8404), B: (0.0366, 0.0001)
    // White point: D50 (0.9642, 1.0, 0.8249)

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int i = 0; i < count; ++i) {
        float r = dist(gen);
        float g = dist(gen);
        float b = dist(gen);
        points.push_back({r, g, b});
    }

    // Add corner cases
    points.push_back({0.0f, 0.0f, 0.0f});  // Black
    points.push_back({1.0f, 1.0f, 1.0f});  // White
    points.push_back({1.0f, 0.0f, 0.0f});  // Red
    points.push_back({0.0f, 1.0f, 0.0f});  // Green
    points.push_back({0.0f, 0.0f, 1.0f});  // Blue
    points.push_back({1.0f, 1.0f, 0.0f});  // Yellow
    points.push_back({1.0f, 0.0f, 1.0f});  // Magenta
    points.push_back({0.0f, 1.0f, 1.0f});  // Cyan

    return points;
}

// Returns the chroma radius of the destination gamut at the given Lab
// point (treating L* slice as the lightness, finding max chroma along the
// (a*, b*) direction that stays inside dest_hull). Falls back to a
// reasonable photo-printer-typical radius if dest_hull is uninitialised.
static float dest_hull_radius_at(const gamut::GamutHull3D& hull,
                                  const spectral::Lab& center,
                                  float hue_rad) {
    constexpr float DEFAULT_RADIUS = 60.0f;  // photo paper typical
    constexpr float MIN_RADIUS     = 20.0f;
    constexpr float MAX_RADIUS     = 130.0f;
    constexpr float STEP           = 2.0f;

    // March outward from neutral until we leave the hull. If hull is not
    // populated we have nothing to measure against, return the default.
    spectral::Lab probe = center;
    float dir_a = std::cos(hue_rad);
    float dir_b = std::sin(hue_rad);
    float r = MIN_RADIUS;
    bool started_inside = false;
    while (r <= MAX_RADIUS) {
        probe.a = center.a + dir_a * r;
        probe.b = center.b + dir_b * r;
        bool inside = hull.contains(probe);
        if (!started_inside && inside) started_inside = true;
        if (started_inside && !inside) {
            return r - STEP;  // last in-gamut radius
        }
        r += STEP;
    }
    return started_inside ? MAX_RADIUS : DEFAULT_RADIUS;
}

// Hue-aware protection: returns a multiplier in [0,1] that scales down
// compression strength when the point's hue falls into a protected band.
// 1.0 = no protection (full compression); lower values reduce compression.
static float protection_strength(float hue_deg, const Wf6GamutConfig& cfg) {
    float reduce = 0.0f;
    auto in_band = [&](float lo, float hi) {
        if (lo <= hi) return hue_deg >= lo && hue_deg <= hi;
        // wraparound (e.g., 330-30 around red)
        return hue_deg >= lo || hue_deg <= hi;
    };
    if (cfg.protect_skin && in_band(cfg.skin_hue_min, cfg.skin_hue_max)) {
        reduce = std::max(reduce, cfg.protect_strength);
    }
    if (cfg.protect_sky && in_band(cfg.sky_hue_min, cfg.sky_hue_max)) {
        reduce = std::max(reduce, cfg.protect_strength);
    }
    // protect_memory_colors: tagged green foliage range too
    if (cfg.protect_memory_colors && in_band(90.0f, 150.0f)) {
        reduce = std::max(reduce, cfg.protect_strength * 0.5f);
    }
    return 1.0f - reduce;
}

std::array<float, 3> compress_gamut_sigmoidal(
    const std::array<float, 3>& lab,
    const Wf6GamutConfig& config) {

    std::array<float, 3> result = lab;

    if (!config.gamut_compression_enabled) return result;

    float dist_from_neutral = std::sqrt(lab[1] * lab[1] + lab[2] * lab[2]);
    if (dist_from_neutral < 0.001f) return result;  // achromatic — nothing to do

    // F14/F15 audit: r_dest_max now comes from the destination hull at the
    // point's hue, not a hardcoded 50. Compression threshold is also
    // interpreted in destination-radius units (0.70 = 70% of available
    // chroma), not absolute Lab units.
    float hue_rad = std::atan2(lab[2], lab[1]);
    float hue_deg = hue_rad * 180.0f / static_cast<float>(M_PI);
    if (hue_deg < 0.0f) hue_deg += 360.0f;

    float r_dest_max = dest_hull_radius_at(
        config.dest_hull,
        spectral::Lab{lab[0], 0.0, 0.0},
        hue_rad);
    float r_start = config.compress_threshold * r_dest_max;

    if (dist_from_neutral <= r_start) return result;  // inside compression zone

    // Sigmoidal compression maps [r_start, ∞) onto [r_start, r_dest_max].
    float normalized = (dist_from_neutral - r_start) / std::max(1.0f, r_dest_max - r_start);
    float compressed = 1.0f / (1.0f + std::exp(-6.0f * (normalized - 1.0f)));
    float new_r = r_start + compressed * (r_dest_max - r_start);

    // F16 audit: protected memory-color ranges (skin, sky, foliage) get
    // compressed less aggressively — they're the colors the viewer notices
    // most when they shift.
    float keep = protection_strength(hue_deg, config);
    new_r = dist_from_neutral + keep * (new_r - dist_from_neutral);

    float scale = new_r / dist_from_neutral;
    result[1] = lab[1] * scale;
    result[2] = lab[2] * scale;
    return result;
}

std::vector<std::array<float, 3>> compress_gamut_batch(
    const std::vector<std::array<float, 3>>& lab_values,
    const Wf6GamutConfig& config) {

    std::vector<std::array<float, 3>> results;
    results.reserve(lab_values.size());

    for (const auto& lab : lab_values) {
        results.push_back(compress_gamut_sigmoidal(lab, config));
    }

    return results;
}

Wf6LUT build_wf6_lut(
    const std::vector<uint8_t>& /*icc_blob*/,
    const Wf6GamutConfig& config) {

    Wf6LUT lut;
    lut.data.resize(Wf6LUT::SIZE * Wf6LUT::SIZE * Wf6LUT::SIZE * Wf6LUT::CHANNELS, 0.0f);

    // F13 audit fix: build sRGB and Lab profiles via LCMS2 once, then walk
    // the RGB grid converting each sample to Lab properly. Replaces the
    // "very rough" linear approximation that produced nonsense Lab values.
    cmsHPROFILE srgb = cmsCreate_sRGBProfile();
    cmsHPROFILE lab_profile = cmsCreateLab4Profile(nullptr);
    cmsHTRANSFORM rgb_to_lab = cmsCreateTransform(
        srgb, TYPE_RGB_FLT,
        lab_profile, TYPE_Lab_FLT,
        INTENT_RELATIVE_COLORIMETRIC,
        cmsFLAGS_NOCACHE | cmsFLAGS_NOOPTIMIZE);

    for (int b = 0; b < Wf6LUT::SIZE; ++b) {
        for (int g = 0; g < Wf6LUT::SIZE; ++g) {
            for (int r = 0; r < Wf6LUT::SIZE; ++r) {
                float rf = r / static_cast<float>(Wf6LUT::SIZE - 1);
                float gf = g / static_cast<float>(Wf6LUT::SIZE - 1);
                float bf = b / static_cast<float>(Wf6LUT::SIZE - 1);

                std::array<float, 3> lab{};
                if (rgb_to_lab) {
                    float rgb[3] = {rf, gf, bf};
                    float out[3];
                    cmsDoTransform(rgb_to_lab, rgb, out, 1);
                    lab = {out[0], out[1], out[2]};
                } else {
                    // Emergency fallback if LCMS2 transform failed to
                    // construct. Produces a grayscale gradient, not a real
                    // separation — only here to keep the function total.
                    lab = {100.0f * rf, 0.0f, 0.0f};
                }

                // Apply gamut compression (hull-aware — see F14/F15 fixes).
                lab = compress_gamut_sigmoidal(lab, config);

                // TODO(D5+): replace this with a real Lab → 11-active
                // separation via the printer ICC. Emit a neutral MAIN_K
                // grayscale so the LUT is a valid 11-channel object whose
                // L* semantics line up with real Lab values.
                int idx = (b * Wf6LUT::SIZE * Wf6LUT::SIZE +
                           g * Wf6LUT::SIZE + r) * Wf6LUT::CHANNELS;
                float main_k = std::max(0.0f, 1.0f - lab[0] / 100.0f);
                for (int ch = 0; ch < Wf6LUT::CHANNELS; ++ch) {
                    lut.data[idx + ch] = (ch == 0) ? main_k : 0.0f;
                }
            }
        }
    }

    if (rgb_to_lab) cmsDeleteTransform(rgb_to_lab);
    cmsCloseProfile(srgb);
    cmsCloseProfile(lab_profile);

    return lut;
}

std::vector<float> lut_lookup(
    const Wf6LUT& lut,
    float r, float g, float b) {

    std::vector<float> inks(Wf6LUT::CHANNELS, 0.0f);

    // Trilinear interpolation
    float fr = r * (lut.size - 1);
    float fg = g * (lut.size - 1);
    float fb = b * (lut.size - 1);

    int r0 = static_cast<int>(fr);
    int g0 = static_cast<int>(fg);
    int b0 = static_cast<int>(fb);
    int r1 = std::min(r0 + 1, lut.size - 1);
    int g1 = std::min(g0 + 1, lut.size - 1);
    int b1 = std::min(b0 + 1, lut.size - 1);

    float dr = fr - r0;
    float dg = fg - g0;
    float db = fb - b0;

    for (int ch = 0; ch < lut.channels; ++ch) {
        // Get 8 corner values
        float c000 = lut.data[(b0 * Wf6LUT::SIZE * Wf6LUT::SIZE + g0 * Wf6LUT::SIZE + r0) * Wf6LUT::CHANNELS + ch];
        float c001 = lut.data[(b0 * Wf6LUT::SIZE * Wf6LUT::SIZE + g0 * Wf6LUT::SIZE + r1) * Wf6LUT::CHANNELS + ch];
        float c010 = lut.data[(b0 * Wf6LUT::SIZE * Wf6LUT::SIZE + g1 * Wf6LUT::SIZE + r0) * Wf6LUT::CHANNELS + ch];
        float c011 = lut.data[(b0 * Wf6LUT::SIZE * Wf6LUT::SIZE + g1 * Wf6LUT::SIZE + r1) * Wf6LUT::CHANNELS + ch];
        float c100 = lut.data[(b1 * Wf6LUT::SIZE * Wf6LUT::SIZE + g0 * Wf6LUT::SIZE + r0) * Wf6LUT::CHANNELS + ch];
        float c101 = lut.data[(b1 * Wf6LUT::SIZE * Wf6LUT::SIZE + g0 * Wf6LUT::SIZE + r1) * Wf6LUT::CHANNELS + ch];
        float c110 = lut.data[(b1 * Wf6LUT::SIZE * Wf6LUT::SIZE + g1 * Wf6LUT::SIZE + r0) * Wf6LUT::CHANNELS + ch];
        float c111 = lut.data[(b1 * Wf6LUT::SIZE * Wf6LUT::SIZE + g1 * Wf6LUT::SIZE + r1) * Wf6LUT::CHANNELS + ch];

        // Trilinear interpolation
        float c00 = c000 * (1 - dr) + c001 * dr;
        float c01 = c010 * (1 - dr) + c011 * dr;
        float c10 = c100 * (1 - dr) + c101 * dr;
        float c11 = c110 * (1 - dr) + c111 * dr;

        float c0 = c00 * (1 - dg) + c01 * dg;
        float c1 = c10 * (1 - dg) + c11 * dg;

        inks[ch] = c0 * (1 - db) + c1 * db;
    }

    return inks;
}

}  // namespace workflow
}  // namespace jcolour
