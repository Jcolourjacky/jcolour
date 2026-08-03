#pragma once

#include <vector>
#include <string>
#include "wf4_chart.h"
#include "../src/gamut_mapping.h"
#include "../src/channel_layout.h"

namespace jcolour {
namespace workflow {

// Workflow 6: ProPhoto RGB → 11 active channels + gamut compression.
// Extends workflow 4 with perceptual rendering. Lives in the same 11-active
// model as wf4; PK/MK is selected per-job via base_chart.black_kind, with
// physical 12-slot expansion happening at the PJL boundary.

constexpr int WF6_NUM_CHANNELS = NUM_ACTIVE_CHANNELS;  // = 11

// Workflow 6 chart layout (extends WF4)
struct Wf6ChartLayout {
    WF4ChartLayout base_chart;
    gamut::GamutHull3D gamut_hull;
    bool gamut_compression_enabled = true;
};

// Gamut compression configuration.
//
// compress_threshold is the fraction of the destination gamut radius at
// which sigmoidal compression starts kicking in. Audit F15 had it set to
// 0.95, which combined with a hardcoded r_dest_max = 50 Lab units meant
// the threshold = 47.5 — close to the boundary of a typical printer gamut,
// so compression barely triggered. 0.70 is a more realistic value:
// noticeable headroom for blending while still leaving most in-gamut
// colors untouched.
struct Wf6GamutConfig {
    gamut::GamutHull3D source_hull;      // ProPhoto RGB gamut
    gamut::GamutHull3D dest_hull;         // Printer gamut
    float compress_threshold = 0.70f;      // Start compression at 70% of dest gamut radius
    bool protect_skin = true;
    bool protect_sky = true;
    bool protect_memory_colors = true;
    bool gamut_compression_enabled = true;   // Enable gamut compression
    float skin_hue_min = 0.0f;
    float skin_hue_max = 60.0f;
    float sky_hue_min = 180.0f;
    float sky_hue_max = 270.0f;
    // Compression strength reduction inside protected hue ranges (0 = no
    // protection, 1 = no compression at all in the protected range).
    float protect_strength = 0.6f;
};

// LUT type: RGB -> 11 active-channel ink
struct Wf6LUT {
    static constexpr int SIZE = 33;  // 33^3 grid
    static constexpr int CHANNELS = NUM_ACTIVE_CHANNELS;  // = 11
    std::vector<float> data;  // SIZE^3 * CHANNELS floats
    int size = SIZE;
    int channels = CHANNELS;
};

// Generate workflow 6 calibration chart
Wf6ChartLayout generate_wf6_calibration_chart();

// Generate ProPhoto RGB test points
std::vector<std::array<float, 3>> generate_prophoto_test_points(int count = 1000);

// Compress gamut using sigmoidal radial compression
std::array<float, 3> compress_gamut_sigmoidal(
    const std::array<float, 3>& lab,
    const Wf6GamutConfig& config);

// Apply gamut compression to Lab values
std::vector<std::array<float, 3>> compress_gamut_batch(
    const std::vector<std::array<float, 3>>& lab_values,
    const Wf6GamutConfig& config);

// Build LUT from ICC profile with gamut compression
Wf6LUT build_wf6_lut(
    const std::vector<uint8_t>& icc_blob,
    const Wf6GamutConfig& config);

// Lookup ink values from LUT
std::vector<float> lut_lookup(
    const Wf6LUT& lut,
    float r, float g, float b);  // 0..1 normalized

}  // namespace workflow
}  // namespace jcolour
