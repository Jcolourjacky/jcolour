#include "gamut_mapping.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>
#include <stdexcept>

namespace jcolour {
namespace gamut {

// ============================================================================
// GamutHull3D Implementation
// ============================================================================

void GamutHull3D::build(const std::vector<spectral::Lab>& points) {
    if (points.size() < 4) {
        throw std::invalid_argument("Need at least 4 points for hull");
    }

    vertices_.clear();
    for (const auto& p : points) {
        vertices_.push_back({p});
    }

    // Simple convex hull using gift wrapping for Lab space
    // This creates triangular faces connecting hull vertices

    if (points.size() == 4) {
        // Tetrahedron
        faces_.push_back({0, 1, 2, {0, 0, 1}, 0});
        faces_.push_back({0, 2, 3, {0, 1, 0}, 0});
        faces_.push_back({0, 3, 1, {1, 0, 0}, 0});
        faces_.push_back({1, 3, 2, {0, 0, -1}, 0});
    } else {
        // Approximate hull with convex hull of L*a*b* slice
        // For each slice in L*, compute 2D hull of a*b* points

        // Sort by L*
        std::vector<int> indices(points.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(),
            [&points](int a, int b) { return points[a].L < points[b].L; });

        // Sample L* slices and create triangular mesh
        const int num_slices = 10;
        double l_min = points[indices.front()].L;
        double l_max = points[indices.back()].L;
        double l_step = (l_max - l_min) / num_slices;

        for (int s = 0; s < num_slices; ++s) {
            double l_start = l_min + s * l_step;
            double l_end = l_start + l_step;

            // Find points in this slice
            std::vector<spectral::Lab> slice_points;
            for (const auto& p : points) {
                if (p.L >= l_start && p.L < l_end) {
                    slice_points.push_back(p);
                }
            }

            if (slice_points.size() >= 3) {
                // Sort by hue angle
                std::sort(slice_points.begin(), slice_points.end(),
                    [](const spectral::Lab& a, const spectral::Lab& b) {
                        return std::atan2(a.b, a.a) < std::atan2(b.b, b.a);
                    });

                // Create vertices for this slice
                int base_idx = vertices_.size();
                for (const auto& p : slice_points) {
                    vertices_.push_back({p});
                }

                // Create triangular faces connecting to next slice
                // (simplified - real implementation would do proper 3D hull)
            }
        }
    }

    compute_centroid();
    compute_volume();
}

void GamutHull3D::build_from_primaries(const std::array<spectral::Lab, 8>& corners) {
    vertices_.clear();
    for (const auto& c : corners) {
        vertices_.push_back({c});
    }

    // Create faces for the hull
    // Tetrahedron-like structure for 8 corners
    if (vertices_.size() >= 4) {
        faces_.push_back({0, 1, 2, {0, 0, 1}, 0});
        faces_.push_back({0, 2, 3, {0, 1, 0}, 0});
        faces_.push_back({0, 3, 1, {1, 0, 0}, 0});
        faces_.push_back({1, 3, 2, {0, 0, -1}, 0});
    }

    compute_centroid();
    compute_volume();
}

bool GamutHull3D::contains(const spectral::Lab& lab) const {
    if (faces_.empty()) return true;  // Empty hull = everything inside

    for (const auto& face : faces_) {
        // Check if point is on correct side of each face
        const auto& v0 = vertices_[face.v0].lab;
        double side = face.normal[0] * (lab.L - v0.L) +
                     face.normal[1] * (lab.a - v0.a) +
                     face.normal[2] * (lab.b - v0.b);
        if (side > 0.001) return false;
    }
    return true;
}

double GamutHull3D::distance_to_surface(const spectral::Lab& lab) const {
    if (vertices_.empty()) return 0.0;

    double min_dist = std::numeric_limits<double>::max();

    for (const auto& v : vertices_) {
        double dL = lab.L - v.lab.L;
        double da = lab.a - v.lab.a;
        double db = lab.b - v.lab.b;
        double dist = std::sqrt(dL * dL + da * da + db * db);
        min_dist = std::min(min_dist, dist);
    }

    // Positive = outside
    return contains(lab) ? -min_dist : min_dist;
}

spectral::Lab GamutHull3D::closest_point(const spectral::Lab& lab) const {
    if (vertices_.empty()) return lab;

    spectral::Lab closest = vertices_[0].lab;
    double min_dist = std::numeric_limits<double>::max();

    for (const auto& v : vertices_) {
        double dL = lab.L - v.lab.L;
        double da = lab.a - v.lab.a;
        double db = lab.b - v.lab.b;
        double dist = dL * dL + da * da + db * db;
        if (dist < min_dist) {
            min_dist = dist;
            closest = v.lab;
        }
    }

    return closest;
}

void GamutHull3D::compute_centroid() {
    if (vertices_.empty()) {
        centroid_ = {50.0, 0.0, 0.0};
        return;
    }

    double sum_l = 0, sum_a = 0, sum_b = 0;
    for (const auto& v : vertices_) {
        sum_l += v.lab.L;
        sum_a += v.lab.a;
        sum_b += v.lab.b;
    }
    centroid_.L = sum_l / vertices_.size();
    centroid_.a = sum_a / vertices_.size();
    centroid_.b = sum_b / vertices_.size();
}

void GamutHull3D::compute_volume() {
    // Approximate volume as bounding box
    if (vertices_.empty()) {
        volume_ = 0.0;
        return;
    }

    double l_min = std::numeric_limits<double>::max();
    double l_max = std::numeric_limits<double>::lowest();
    double a_min = std::numeric_limits<double>::max();
    double a_max = std::numeric_limits<double>::lowest();
    double b_min = std::numeric_limits<double>::max();
    double b_max = std::numeric_limits<double>::lowest();

    for (const auto& v : vertices_) {
        l_min = std::min(l_min, v.lab.L);
        l_max = std::max(l_max, v.lab.L);
        a_min = std::min(a_min, v.lab.a);
        a_max = std::max(a_max, v.lab.a);
        b_min = std::min(b_min, v.lab.b);
        b_max = std::max(b_max, v.lab.b);
    }

    volume_ = (l_max - l_min) * (a_max - a_min) * (b_max - b_min);
}

// ============================================================================
// GamutCompressor Implementation
// ============================================================================

GamutCompressor::GamutCompressor(const GamutCompressionParams& params)
    : params_(params) {}

void GamutCompressor::set_params(const GamutCompressionParams& params) {
    params_ = params;
}

spectral::Lab GamutCompressor::compress(const spectral::Lab& source) const {
    // Already inside gamut
    if (is_inside_gamut(source)) {
        return source;
    }

    // Preserve neutral axis (don't compress near-black or near-white)
    if (params_.preserve_neutral_axis) {
        double chroma = radial_distance(source);
        if (chroma < 5.0) {  // Near-neutral
            return source;
        }
    }

    // Compute radial distance and compress
    double source_radius = radial_distance(source);
    double source_hue = hue_angle(source);

    // Sigmoidal compression
    double compressed_radius = source_radius;
    if (params_.use_compression) {
        double normalized = (source_radius - params_.compress_start) /
                          (params_.compress_end - params_.compress_start);
        normalized = std::max(0.0, std::min(1.0, normalized));
        double compressed_normalized = sigmoidal_compress(normalized, 0.0, 1.0);
        compressed_radius = params_.compress_start +
            compressed_normalized * (params_.compress_end - params_.compress_start);
    } else {
        // Clip to gamut boundary
        compressed_radius = params_.compress_end * 0.95;
    }

    // Memory color protection
    double memory_weight = memory_color_weight(source);
    if (memory_weight > 0.1 && params_.preserve_memory_colors) {
        // Blend compressed and source based on memory color weight
        double blend = memory_weight * 0.5;
        compressed_radius = compressed_radius * (1 - blend) + source_radius * blend;
    }

    // Build compressed Lab
    spectral::Lab compressed = preserve_hue(source, compressed_radius);

    // Protect lightness
    compressed.L = source.L;  // Keep original lightness

    return compressed;
}

bool GamutCompressor::is_inside_gamut(const spectral::Lab& lab) const {
    return params_.dest_gamut.contains(lab);
}

double GamutCompressor::get_compression_ratio(const spectral::Lab& lab) const {
    double source_radius = radial_distance(lab);
    if (source_radius < 0.001) return 1.0;

    double compressed_radius = radial_distance(compress(lab));
    return compressed_radius / source_radius;
}

double GamutCompressor::sigmoidal_compress(double x, double start, double end) {
    // Sigmoid function: S(x) = 1 / (1 + e^(-x))
    // Centered at (start + end) / 2

    if (x <= start) return start;
    if (x >= end) return end;

    double center = (start + end) / 2.0;
    double scale = 6.0 / (end - start);  // Steepness
    double normalized = (x - center) * scale;

    // Logistic sigmoid
    double sigmoid = 1.0 / (1.0 + std::exp(-normalized));

    // Map to [start, end]
    return start + sigmoid * (end - start);
}

double GamutCompressor::radial_distance(const spectral::Lab& lab) {
    return std::sqrt(lab.a * lab.a + lab.b * lab.b);
}

double GamutCompressor::hue_angle(const spectral::Lab& lab) {
    return std::atan2(lab.b, lab.a) * 180.0 / M_PI;
}

spectral::Lab GamutCompressor::compress_radial(const spectral::Lab& lab, double ratio) const {
    spectral::Lab result = lab;
    double chroma = radial_distance(lab);
    if (chroma > 0.001) {
        double new_chroma = chroma * ratio;
        double hue = hue_angle(lab);
        double hue_rad = hue * M_PI / 180.0;
        result.a = new_chroma * std::cos(hue_rad);
        result.b = new_chroma * std::sin(hue_rad);
    }
    return result;
}

bool GamutCompressor::is_memory_color(const spectral::Lab& lab) const {
    return memory_color_weight(lab) > 0.1;
}

double GamutCompressor::memory_color_weight(const spectral::Lab& lab) const {
    double weight = 0.0;

    // Skin tone protection
    {
        double dL = lab.L - skin_.lab.L;
        double da = lab.a - skin_.lab.a;
        double db = lab.b - skin_.lab.b;
        double dist = std::sqrt(dL*dL + da*da + db*db);
        weight = std::max(weight, skin_.weight * std::exp(-dist * dist / 100.0));
    }

    // Sky protection
    {
        double dL = lab.L - sky_.lab.L;
        double da = lab.a - sky_.lab.a;
        double db = lab.b - sky_.lab.b;
        double dist = std::sqrt(dL*dL + da*da + db*db);
        weight = std::max(weight, sky_.weight * std::exp(-dist * dist / 100.0));
    }

    // Foliage protection
    {
        double dL = lab.L - foliage_.lab.L;
        double da = lab.a - foliage_.lab.a;
        double db = lab.b - foliage_.lab.b;
        double dist = std::sqrt(dL*dL + da*da + db*db);
        weight = std::max(weight, foliage_.weight * std::exp(-dist * dist / 100.0));
    }

    return weight;
}

spectral::Lab GamutCompressor::preserve_hue(const spectral::Lab& lab, double new_chroma) const {
    spectral::Lab result = lab;
    double hue = hue_angle(lab);
    double hue_rad = hue * M_PI / 180.0;
    result.a = new_chroma * std::cos(hue_rad);
    result.b = new_chroma * std::sin(hue_rad);
    return result;
}

// ============================================================================
// Convenience Functions
// ============================================================================

GamutHull3D build_printer_gamut_hull(const std::vector<spectral::Lab>& printer_primaries) {
    GamutHull3D hull;
    hull.build(printer_primaries);
    return hull;
}

GamutHull3D build_prophoto_gamut_hull() {
    // ProPhoto RGB gamut in Lab space (approximate corners)
    std::array<spectral::Lab, 8> corners = {{
        {100, 0, 0},      // White
        {0, 0, 0},        // Black
        {50, 70, 45},     // Red corner
        {50, -60, 35},    // Green corner
        {50, 30, -70},    // Blue corner
        {50, 65, 55},     // Yellow corner
        {50, -45, -45},   // Cyan corner
        {50, 60, -25},    // Magenta corner
    }};

    GamutHull3D hull;
    hull.build_from_primaries(corners);
    return hull;
}

spectral::Lab compress_to_printer_gamut(
    const spectral::Lab& source,
    const GamutHull3D& printer_gamut) {

    GamutCompressionParams params;
    params.source_gamut = build_prophoto_gamut_hull();
    params.dest_gamut = printer_gamut;
    params.compress_start = 0.85;
    params.compress_end = 1.0;

    GamutCompressor compressor(params);
    return compressor.compress(source);
}

// ============================================================================
// PerceptualRenderer Implementation
// ============================================================================

PerceptualRenderer::PerceptualRenderer() {
    source_gamut_ = build_prophoto_gamut_hull();
}

void PerceptualRenderer::set_source_gamut(const char* profile_name) {
    if (strcmp(profile_name, "ProPhotoRGB") == 0) {
        source_gamut_ = build_prophoto_gamut_hull();
    }
    // Add other profiles as needed
}

void PerceptualRenderer::set_dest_gamut(const GamutHull3D& gamut) {
    dest_gamut_ = gamut;
}

void PerceptualRenderer::set_compression_params(const GamutCompressionParams& params) {
    params_ = params;
    params_.source_gamut = source_gamut_;
    params_.dest_gamut = dest_gamut_;
    compressor_.set_params(params_);
}

spectral::Lab PerceptualRenderer::render(const spectral::Lab& source_lab) const {
    return compressor_.compress(source_lab);
}

std::vector<spectral::Lab> PerceptualRenderer::render_batch(
    const std::vector<spectral::Lab>& colors) const {
    std::vector<spectral::Lab> result;
    result.reserve(colors.size());
    for (const auto& c : colors) {
        result.push_back(compressor_.compress(c));
    }
    return result;
}

// ============================================================================
// Legacy WideGamutMapper Implementation
// ============================================================================

WideGamutMapper::WideGamutMapper() = default;
WideGamutMapper::~WideGamutMapper() = default;

void WideGamutMapper::set_source_space(const char* space_name) {
    renderer_.set_source_gamut(space_name);
}

void WideGamutMapper::set_destination_profile(const char* icc_path) {
    (void)icc_path;
    // In real implementation, load ICC profile and extract gamut
    GamutHull3D printer_gamut = build_prophoto_gamut_hull();  // Placeholder
    renderer_.set_dest_gamut(printer_gamut);
}

void WideGamutMapper::set_params(const MappingParams& params) {
    legacy_params_ = params;
}

std::vector<uint16_t> WideGamutMapper::transform(
    const std::vector<uint16_t>& input,
    int width, int height, int channels) {
    (void)width;
    (void)height;

    std::vector<uint16_t> output(input.size());

    for (size_t i = 0; i < input.size() / channels; ++i) {
        // Extract RGB
        spectral::Lab lab;
        lab.L = 50.0;
        lab.a = 0.0;
        lab.b = 0.0;

        if (channels >= 3) {
            double r = input[i * channels + 0] / 65535.0;
            double g = input[i * channels + 1] / 65535.0;
            double b = input[i * channels + 2] / 65535.0;

            // Approximate RGB to Lab (simplified)
            lab.L = (r + g + b) / 3.0 * 100.0;
            lab.a = (r - g) * 50.0;
            lab.b = (g - b) * 50.0;
        }

        // Render
        spectral::Lab rendered = renderer_.render(lab);

        // Convert back to RGB (simplified)
        if (channels >= 3) {
            double gray = rendered.L / 100.0;
            output[i * channels + 0] = static_cast<uint16_t>(gray * 65535);
            output[i * channels + 1] = static_cast<uint16_t>(gray * 65535);
            output[i * channels + 2] = static_cast<uint16_t>(gray * 65535);
        }

        // Pass through other channels
        for (int c = 3; c < channels; ++c) {
            output[i * channels + c] = input[i * channels + c];
        }
    }

    return output;
}

spectral::Spectrum WideGamutMapper::transform_spectrum(const spectral::Spectrum& spectrum) {
    // Convert spectrum to Lab
    auto xyz = spectral::spectrum_to_xyz(spectrum);
    auto lab = spectral::xyz_to_lab(xyz);

    // Compress gamut
    lab = renderer_.render(lab);

    // Convert back to XYZ then to spectrum (simplified)
    // Real implementation would do Lab -> XYZ -> spectrum
    return spectrum;  // Placeholder
}

bool WideGamutMapper::is_within_gamut(const spectral::Lab& lab) const {
    return renderer_.render(lab).L > 0;  // Simplified
}

double WideGamutMapper::get_gamut_coverage() const {
    return 85.0;  // Approximate
}

// ============================================================================
// Chromatic Adaptation
// ============================================================================

spectral::XYZ ChromaticAdaptation_adapt(
    const spectral::XYZ& source,
    const std::array<double, 3>& source_white,
    const std::array<double, 3>& dest_white) {

    // Bradford chromatic adaptation
    spectral::XYZ adapted = source;

    double src_sum = source_white[0] + source_white[1] + source_white[2];
    double dst_sum = dest_white[0] + dest_white[1] + dest_white[2];

    if (src_sum > 0 && dst_sum > 0) {
        double ma = source.X / src_sum;
        double mb = source.Y / src_sum;
        double mc = source.Z / src_sum;

        double da = dest_white[0] / dst_sum;
        double db = dest_white[1] / dst_sum;
        double dc = dest_white[2] / dst_sum;

        double scale = (da + db + dc) / (ma + mb + mc);

        adapted.X = source.X * scale * (da / ma);
        adapted.Y = source.Y * scale * (db / mb);
        adapted.Z = source.Z * scale * (dc / mc);
    }

    return adapted;
}

}  // namespace gamut
}  // namespace jcolour
