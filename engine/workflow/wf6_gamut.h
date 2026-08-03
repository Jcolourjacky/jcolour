#pragma once

#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include "../src/gamut_mapping.h"

namespace jcolour {
namespace workflow {

// Workflow 6 extends workflow 4 with gamut compression
// Used for ProPhoto RGB -> 12 channel with perceptual rendering

// Gamut compression configuration for workflow 6
struct Wf6GamutCompression {
    gamut::GamutHull3D source_hull;
    gamut::GamutHull3D dest_hull;
    float compress_start_hue = 0.0f;  // Start hue angle for compression
    float compress_end_hue = 360.0f;  // End hue angle
    bool protect_skin = true;
    bool protect_sky = true;
    bool protect_memory_colors = true;
};

// Workflow 6 configuration
struct Wf6Config {
    // Base workflow 4 ICC/LUT
    std::vector<uint8_t> icc_blob;

    // Gamut compression settings
    bool use_gamut_compression = true;
    Wf6GamutCompression compression;

    // Paper simulation
    bool simulate_paper_white = true;
    std::array<double, 36> paper_white_spectrum;
};

// Compress Lab color from source gamut to destination gamut
std::array<double, 3> compress_gamut(
    const std::array<double, 3>& lab,
    const gamut::GamutHull3D& source,
    const gamut::GamutHull3D& dest);

// Apply gamut compression to an image
std::vector<std::array<double, 3>> compress_image_gamut(
    const std::vector<std::array<double, 3>>& lab_values,
    const Wf6GamutCompression& compression);

// Build gamut hull from ICC profile
gamut::GamutHull3D build_gamut_hull(const std::vector<uint8_t>& icc_blob);

// Export gamut boundary visualization
void export_gamut_boundary_svg(
    const gamut::GamutHull3D& hull,
    const std::string& path);

}  // namespace workflow
}  // namespace jcolour