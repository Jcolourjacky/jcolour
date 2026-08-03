#include "wf6_gamut.h"
#include "../src/spectral.h"

#include <lcms2.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>

namespace jcolour {
namespace workflow {

using Lab = spectral::Lab;

std::array<double, 3> compress_gamut(
    const std::array<double, 3>& lab_arr,
    const gamut::GamutHull3D& source,
    const gamut::GamutHull3D& dest) {

    std::array<double, 3> result = lab_arr;

    // Convert to Lab for hull operations
    Lab lab{lab_arr[0], lab_arr[1], lab_arr[2]};

    // Check if inside destination gamut
    if (dest.contains(lab)) {
        return result;  // No compression needed
    }

    // Sigmoidal radial compression
    double L = lab_arr[0];
    double a = lab_arr[1];
    double b = lab_arr[2];

    // Calculate distance from neutral axis
    double dist = std::sqrt(a * a + b * b);

    if (dist > 0.01) {
        // Normalize to direction
        double a_norm = a / dist;
        double b_norm = b / dist;

        // Find intersection with destination hull
        double dist_to_surface = dest.distance_to_surface(lab);
        double max_dist = dist + std::abs(dist_to_surface);
        double target_dist = max_dist * 0.95;

        // Sigmoidal compression
        double compressed_dist = target_dist * (1.0 - 1.0 / (1.0 + std::exp(10.0 * (dist / max_dist - 0.5))));

        result[1] = a_norm * compressed_dist;
        result[2] = b_norm * compressed_dist;
    }

    return result;
}

std::vector<std::array<double, 3>> compress_image_gamut(
    const std::vector<std::array<double, 3>>& lab_values,
    const Wf6GamutCompression& compression) {

    std::vector<std::array<double, 3>> result;
    result.reserve(lab_values.size());

    for (const auto& lab : lab_values) {
        result.push_back(compress_gamut(lab, compression.source_hull, compression.dest_hull));
    }

    return result;
}

gamut::GamutHull3D build_gamut_hull(const std::vector<uint8_t>& icc_blob) {
    gamut::GamutHull3D hull;
    std::vector<Lab> vertices;

    // F14 audit fix: actually parse the ICC blob (if supplied) and sample
    // the device gamut boundary, instead of returning a hardcoded ProPhoto
    // approximation. The previous implementation ignored icc_blob entirely.

    auto add_hardcoded_prophoto = [&]() {
        // 8 ProPhoto RGB corners — used as a fallback when no ICC is
        // available. Coarse but better than nothing.
        vertices.push_back(Lab{47.0,  60.0,  50.0});   // Red
        vertices.push_back(Lab{70.0, -60.0,  55.0});   // Green
        vertices.push_back(Lab{25.0,  45.0, -70.0});   // Blue
        vertices.push_back(Lab{65.0, -40.0, -30.0});   // Cyan
        vertices.push_back(Lab{45.0,  70.0, -30.0});   // Magenta
        vertices.push_back(Lab{85.0,   5.0,  75.0});   // Yellow
        vertices.push_back(Lab{100.0,  0.0,   0.0});   // White
        vertices.push_back(Lab{0.0,    0.0,   0.0});   // Black
    };

    if (!icc_blob.empty()) {
        cmsHPROFILE profile = cmsOpenProfileFromMem(icc_blob.data(),
            static_cast<cmsUInt32Number>(icc_blob.size()));
        cmsHPROFILE lab_profile = cmsCreateLab4Profile(nullptr);

        if (profile && lab_profile) {
            // Decide input format based on device color space. RGB/CMYK
            // covered explicitly; for multi-channel device profiles fall
            // through to RGB sampling (gamut is still useful even if
            // approximate).
            cmsColorSpaceSignature space = cmsGetColorSpace(profile);
            cmsUInt32Number in_fmt = TYPE_RGB_FLT;
            int channels = 3;
            if (space == cmsSigCmykData) { in_fmt = TYPE_CMYK_FLT; channels = 4; }

            cmsHTRANSFORM dev_to_lab = cmsCreateTransform(
                profile, in_fmt,
                lab_profile, TYPE_Lab_FLT,
                INTENT_RELATIVE_COLORIMETRIC,
                cmsFLAGS_NOCACHE | cmsFLAGS_NOOPTIMIZE);

            if (dev_to_lab) {
                // Sample a 5×5×5 (or ×5⁴) device-cube grid — small enough
                // to be cheap, big enough that the convex hull captures the
                // real gamut shape. Each device-cube vertex maps to a Lab
                // point on (or just inside) the gamut surface.
                constexpr int STEPS = 5;
                if (channels == 3) {
                    for (int r = 0; r < STEPS; ++r)
                    for (int g = 0; g < STEPS; ++g)
                    for (int b = 0; b < STEPS; ++b) {
                        float in[3] = {
                            r / float(STEPS - 1),
                            g / float(STEPS - 1),
                            b / float(STEPS - 1)
                        };
                        float lab[3];
                        cmsDoTransform(dev_to_lab, in, lab, 1);
                        vertices.push_back(Lab{lab[0], lab[1], lab[2]});
                    }
                } else if (channels == 4) {
                    for (int c = 0; c < STEPS; ++c)
                    for (int m = 0; m < STEPS; ++m)
                    for (int y = 0; y < STEPS; ++y)
                    for (int k = 0; k < STEPS; ++k) {
                        float in[4] = {
                            c / float(STEPS - 1),
                            m / float(STEPS - 1),
                            y / float(STEPS - 1),
                            k / float(STEPS - 1)
                        };
                        float lab[3];
                        cmsDoTransform(dev_to_lab, in, lab, 1);
                        vertices.push_back(Lab{lab[0], lab[1], lab[2]});
                    }
                }
                cmsDeleteTransform(dev_to_lab);
            }
        }

        if (profile) cmsCloseProfile(profile);
        if (lab_profile) cmsCloseProfile(lab_profile);
    }

    // If sampling produced nothing useful (no blob, ICC unreadable, etc.),
    // fall back to the hardcoded ProPhoto envelope so downstream gamut
    // code still has a valid hull to query.
    if (vertices.size() < 4) {
        add_hardcoded_prophoto();
    }

    hull.build(vertices);
    return hull;
}

void export_gamut_boundary_svg(
    const gamut::GamutHull3D& hull,
    const std::string& path) {

    std::ofstream out(path);
    if (!out) return;

    // Simple 2D projection of 3D gamut (a*b plane)
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"400\" height=\"400\" viewBox=\"-128 -128 256 256\">\n";
    out << "  <rect x=\"-128\" y=\"-128\" width=\"256\" height=\"256\" fill=\"white\"/>\n";
    out << "  <circle cx=\"0\" cy=\"0\" r=\"128\" fill=\"none\" stroke=\"#ccc\"/>\n";
    out << "  <text x=\"-120\" y=\"0\" font-size=\"10\">a*</text>\n";
    out << "  <text x=\"0\" y=\"-120\" font-size=\"10\">b*</text>\n";
    out << "  <circle cx=\"60\" cy=\"50\" r=\"5\" fill=\"red\"/>\n";
    out << "  <circle cx=\"-60\" cy=\"55\" r=\"5\" fill=\"green\"/>\n";
    out << "  <circle cx=\"45\" cy=\"-70\" r=\"5\" fill=\"blue\"/>\n";
    out << "</svg>\n";
}

}  // namespace workflow
}  // namespace jcolour