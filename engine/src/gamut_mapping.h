#pragma once

#include "spectral.h"
#include <vector>
#include <array>
#include <cstdint>

namespace jcolour {
namespace gamut {

// ============================================================================
// 3D Gamut Hull (proper convex hull in Lab space)
// ============================================================================

class GamutHull3D {
public:
    GamutHull3D() = default;

    // Build hull from a set of Lab points
    void build(const std::vector<spectral::Lab>& points);

    // Build hull from ICC profile primaries (simplified)
    void build_from_primaries(const std::array<spectral::Lab, 8>& corners);

    // Check if a point is inside the hull
    bool contains(const spectral::Lab& lab) const;

    // Compute distance from point to hull surface (positive = outside)
    double distance_to_surface(const spectral::Lab& lab) const;

    // Find the closest point on the hull surface
    spectral::Lab closest_point(const spectral::Lab& lab) const;

    // Get hull volume (approximate)
    double volume() const { return volume_; }

    // Get hull centroid
    spectral::Lab centroid() const { return centroid_; }

    // Check if hull is valid
    bool is_valid() const { return !faces_.empty(); }

private:
    struct Face {
        int v0, v1, v2;  // Vertex indices
        std::array<double, 3> normal;
        double d;  // Plane equation: normal·x + d = 0
    };

    struct Vertex {
        spectral::Lab lab;
    };

    std::vector<Vertex> vertices_;
    std::vector<Face> faces_;
    double volume_ = 0.0;
    spectral::Lab centroid_{50.0, 0.0, 0.0};

    void compute_centroid();
    void compute_volume();
};

// ============================================================================
// Gamut Compression (sigmoidal radial compression)
// ============================================================================

struct GamutCompressionParams {
    // Source gamut (ProPhoto RGB)
    GamutHull3D source_gamut;

    // Destination gamut (printer + paper)
    GamutHull3D dest_gamut;

    // Compression parameters
    double compress_start = 0.85;  // Start compression at 85% of gamut radius
    double compress_end = 1.0;     // End at gamut boundary
    bool preserve_skin_tones = true;
    bool preserve_memory_colors = true;
    bool preserve_neutral_axis = true;

    // Hue preservation
    bool preserve_hue_angle = true;
    double hue_tolerance_deg = 5.0;

    // Clipping vs compression
    bool use_compression = true;  // false = clip instead
};

// Memory color anchors for protection
struct MemoryColorAnchor {
    spectral::Lab lab;
    const char* name;
    double weight;
};

class GamutCompressor {
public:
    GamutCompressor() = default;

    explicit GamutCompressor(const GamutCompressionParams& params);

    void set_params(const GamutCompressionParams& params);

    // Compress a Lab color from source gamut to dest gamut
    spectral::Lab compress(const spectral::Lab& source) const;

    // Check if color is within destination gamut
    bool is_inside_gamut(const spectral::Lab& lab) const;

    // Get compression info
    double get_compression_ratio(const spectral::Lab& lab) const;

    // Sigmoidal radial compression function
    static double sigmoidal_compress(double x, double start, double end);

private:
    GamutCompressionParams params_;

    // Memory color anchors (approximate values)
    static constexpr MemoryColorAnchor skin_{spectral::Lab{70, 20, 25}, "skin", 0.8};
    static constexpr MemoryColorAnchor sky_{spectral::Lab{60, -5, -25}, "sky", 0.7};
    static constexpr MemoryColorAnchor foliage_{spectral::Lab{45, -25, 25}, "foliage", 0.6};

    // Helper: compute radial distance from neutral axis (L*=50, a*=0, b*=0)
    static double radial_distance(const spectral::Lab& lab);

    // Helper: compute hue angle in degrees
    static double hue_angle(const spectral::Lab& lab);

    // Helper: compress towards neutral axis
    spectral::Lab compress_radial(const spectral::Lab& lab, double ratio) const;

    // Helper: protect memory colors
    bool is_memory_color(const spectral::Lab& lab) const;
    double memory_color_weight(const spectral::Lab& lab) const;

    // Helper: preserve hue angle
    spectral::Lab preserve_hue(const spectral::Lab& lab, double new_chroma) const;
};

// ============================================================================
// Convenience functions
// ============================================================================

// Build gamut hull from printer primaries (CMYKRGB + O/G/V)
GamutHull3D build_printer_gamut_hull(
    const std::vector<spectral::Lab>& printer_primaries);

// Build gamut hull from ProPhoto RGB primaries
GamutHull3D build_prophoto_gamut_hull();

// Compress a color from ProPhoto to printer gamut
spectral::Lab compress_to_printer_gamut(
    const spectral::Lab& source,
    const GamutHull3D& printer_gamut);

// Full perceptual rendering pipeline
class PerceptualRenderer {
public:
    PerceptualRenderer();

    // Configure source and destination gamuts
    void set_source_gamut(const char* profile_name);  // "ProPhotoRGB", "AdobeRGB", etc.
    void set_dest_gamut(const GamutHull3D& gamut);

    // Configure compression params
    void set_compression_params(const GamutCompressionParams& params);

    // Render a single color
    spectral::Lab render(const spectral::Lab& source_lab) const;

    // Render a batch of colors
    std::vector<spectral::Lab> render_batch(const std::vector<spectral::Lab>& colors) const;

private:
    GamutHull3D source_gamut_;
    GamutHull3D dest_gamut_;
    GamutCompressionParams params_;
    GamutCompressor compressor_;
};

// ============================================================================
// Legacy compatibility (deprecated)
// ============================================================================

struct ColorPrimaries {
    std::array<double, 3> red;
    std::array<double, 3> green;
    std::array<double, 3> blue;
    std::array<double, 3> white;
};

enum class RenderingIntent {
    Perceptual = 0,
    Relative = 1,
    Saturation = 2,
    Absolute = 3
};

struct MappingParams {
    RenderingIntent intent = RenderingIntent::Relative;
    bool use_bpc = true;
    bool preserve_sat = false;
    double gamut_warning_threshold = 3.0;
    bool use_extended_gamut = true;
    double saturation_boost = 1.0;
};

// Legacy WideGamutMapper - delegates to new implementation
class WideGamutMapper {
public:
    WideGamutMapper();
    ~WideGamutMapper();

    void set_source_space(const char* space_name);
    void set_destination_profile(const char* icc_path);
    void set_params(const MappingParams& params);

    std::vector<uint16_t> transform(
        const std::vector<uint16_t>& input,
        int width, int height, int channels);

    spectral::Spectrum transform_spectrum(const spectral::Spectrum& spectrum);
    bool is_within_gamut(const spectral::Lab& lab) const;
    double get_gamut_coverage() const;

private:
    PerceptualRenderer renderer_;
    MappingParams legacy_params_;
};

// Utility functions
spectral::XYZ ChromaticAdaptation_adapt(
    const spectral::XYZ& source,
    const std::array<double, 3>& source_white,
    const std::array<double, 3>& dest_white);

}  // namespace gamut
}  // namespace jcolour
