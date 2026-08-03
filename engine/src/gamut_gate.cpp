#include "gamut_gate.h"
#include <cstring>
#include <cmath>

namespace jcolour {
namespace gamut {

namespace {

struct Chromaticity { double x, y; };

struct KnownPrimaries {
    Chromaticity red, green, blue;
};

constexpr KnownPrimaries PROPHOTO_PRIMARIES = {
    {0.7347, 0.2653}, {0.1596, 0.8404}, {0.0366, 0.0001}
};
constexpr KnownPrimaries ADOBERGB_PRIMARIES = {
    {0.6400, 0.3300}, {0.2100, 0.7100}, {0.1500, 0.0600}
};
constexpr KnownPrimaries SRGB_PRIMARIES = {
    {0.6400, 0.3300}, {0.3000, 0.6000}, {0.1500, 0.0600}
};

bool match_primaries(cmsHPROFILE profile, const KnownPrimaries& known, double eps) {
    cmsCIExyYTRIPLE primaries;
    if (!cmsIsMatrixShaper(profile)) return false;

    cmsCIEXYZ* red_xyz = (cmsCIEXYZ*)cmsReadTag(profile, cmsSigRedColorantTag);
    cmsCIEXYZ* grn_xyz = (cmsCIEXYZ*)cmsReadTag(profile, cmsSigGreenColorantTag);
    cmsCIEXYZ* blu_xyz = (cmsCIEXYZ*)cmsReadTag(profile, cmsSigBlueColorantTag);
    if (!red_xyz || !grn_xyz || !blu_xyz) return false;

    cmsCIExyY red_xyY, grn_xyY, blu_xyY;
    cmsXYZ2xyY(&red_xyY, red_xyz);
    cmsXYZ2xyY(&grn_xyY, grn_xyz);
    cmsXYZ2xyY(&blu_xyY, blu_xyz);

    return std::fabs(red_xyY.x - known.red.x) < eps &&
           std::fabs(red_xyY.y - known.red.y) < eps &&
           std::fabs(grn_xyY.x - known.green.x) < eps &&
           std::fabs(grn_xyY.y - known.green.y) < eps &&
           std::fabs(blu_xyY.x - known.blue.x) < eps &&
           std::fabs(blu_xyY.y - known.blue.y) < eps;
}

} // anon

bool GamutGate::matches_prophoto_primaries(cmsHPROFILE profile, double epsilon) {
    return match_primaries(profile, PROPHOTO_PRIMARIES, epsilon);
}

bool GamutGate::matches_adobergb_primaries(cmsHPROFILE profile, double epsilon) {
    return match_primaries(profile, ADOBERGB_PRIMARIES, epsilon);
}

bool GamutGate::matches_srgb_primaries(cmsHPROFILE profile, double epsilon) {
    return match_primaries(profile, SRGB_PRIMARIES, epsilon);
}

GateStatus GamutGate::check_source_profile(cmsHPROFILE profile) {
    GateStatus s;
    if (!profile) {
        s.result = GateResult::Reject;
        s.message = "No source profile embedded";
        return s;
    }

    if (cmsGetColorSpace(profile) != cmsSigRgbData) {
        s.result = GateResult::Reject;
        s.message = "Source profile is not RGB";
        return s;
    }

    if (matches_prophoto_primaries(profile)) {
        s.result = GateResult::Pass;
        s.message = "ProPhoto RGB detected";
        return s;
    }

    if (matches_adobergb_primaries(profile)) {
        s.result = GateResult::Warn;
        s.warn_level = WarnLevel::Yellow;
        s.message = "AdobeRGB detected — truncates ~15% of P9500 yellow-green gamut";
        return s;
    }

    if (matches_srgb_primaries(profile)) {
        s.result = GateResult::Warn;
        s.warn_level = WarnLevel::Red;
        s.message = "sRGB detected — 30% gamut already lost, re-export as ProPhoto recommended";
        return s;
    }

    s.result = GateResult::Warn;
    s.warn_level = WarnLevel::Amber;
    s.message = "Unrecognized color space, treating as sRGB";
    return s;
}

GateStatus GamutGate::check_bit_depth(int bits_per_sample) {
    GateStatus s;
    if (bits_per_sample == 16) {
        s.result = GateResult::Pass;
        s.message = "16-bit confirmed";
    } else {
        s.result = GateResult::Reject;
        s.message = "ProPhoto 8-bit will produce banding — re-export as 16-bit";
    }
    return s;
}

GateStatus GamutGate::check_alpha_clip(const uint16_t* data, int width, int height,
                                        int channels, int extra_samples_type) {
    GateStatus s;
    if (!data) {
        s.result = GateResult::Reject;
        s.message = "No image data";
        return s;
    }

    // TIFF EXTRASAMPLE_ASSOCALPHA = 1
    if (extra_samples_type == 1 && channels > 3) {
        s.result = GateResult::Warn;
        s.warn_level = WarnLevel::Yellow;
        s.message = "Associated alpha (premultiplied) detected — will un-premultiply before processing";
        return s;
    }

    size_t total = static_cast<size_t>(width) * height * channels;
    size_t clip_count = 0;
    for (size_t i = 0; i < total; ++i) {
        if (data[i] == 0 || data[i] == 65535) ++clip_count;
    }

    double clip_ratio = static_cast<double>(clip_count) / total;
    if (clip_ratio > 0.005) {
        s.result = GateResult::Warn;
        s.warn_level = WarnLevel::Yellow;
        s.message = "Hard-clipped pixels (0 or 65535) exceed 0.5% — highlight/shadow detail may be lost";
        return s;
    }

    s.result = GateResult::Pass;
    s.message = "No alpha premultiplication or hard clipping detected";
    return s;
}

GateStatus GamutGate::check_display_gamut(cmsHPROFILE display_profile) {
    GateStatus s;
    if (!display_profile) {
        s.result = GateResult::Warn;
        s.warn_level = WarnLevel::Amber;
        s.message = "No display profile available — softproofing disabled";
        return s;
    }

    // Estimate P3 coverage by sampling saturated Lab colors and checking
    // if the display can reproduce them. Simplified heuristic.
    cmsHPROFILE lab_profile = cmsCreateLab4Profile(nullptr);
    cmsHTRANSFORM to_display = cmsCreateTransform(
        lab_profile, TYPE_Lab_DBL,
        display_profile, TYPE_RGB_16,
        INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_NOCACHE);

    cmsHTRANSFORM back_to_lab = cmsCreateTransform(
        display_profile, TYPE_RGB_16,
        lab_profile, TYPE_Lab_DBL,
        INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_NOCACHE);

    int in_gamut = 0;
    int total_samples = 0;

    if (to_display && back_to_lab) {
        // Sample P3-edge colors in Lab
        for (int h = 0; h < 360; h += 10) {
            for (double C = 40; C <= 120; C += 20) {
                double rad = h * M_PI / 180.0;
                cmsCIELab src_lab = {60.0, C * std::cos(rad), C * std::sin(rad)};
                uint16_t rgb[3];
                cmsCIELab dst_lab;

                cmsDoTransform(to_display, &src_lab, rgb, 1);
                cmsDoTransform(back_to_lab, rgb, &dst_lab, 1);

                double dL = src_lab.L - dst_lab.L;
                double da = src_lab.a - dst_lab.a;
                double db = src_lab.b - dst_lab.b;
                double de = std::sqrt(dL*dL + da*da + db*db);
                if (de < 5.0) ++in_gamut;
                ++total_samples;
            }
        }
    }

    if (to_display) cmsDeleteTransform(to_display);
    if (back_to_lab) cmsDeleteTransform(back_to_lab);
    cmsCloseProfile(lab_profile);

    double coverage = total_samples > 0 ? static_cast<double>(in_gamut) / total_samples : 0.0;

    if (coverage >= 0.95) {
        s.result = GateResult::Pass;
        s.message = "Display covers >= 95% of P3 gamut";
    } else {
        s.result = GateResult::Warn;
        s.warn_level = WarnLevel::Red;
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Display covers only %.0f%% of P3 — softproofing unreliable",
                 coverage * 100.0);
        s.message = buf;
    }
    return s;
}

GateStatus GamutGate::check_dest_icc(cmsHPROFILE dest_profile) {
    GateStatus s;
    if (!dest_profile) {
        s.result = GateResult::Reject;
        s.message = "No destination ICC profile";
        return s;
    }

    bool has_a2b0 = cmsIsTag(dest_profile, cmsSigAToB0Tag) != 0;
    bool has_a2b1 = cmsIsTag(dest_profile, cmsSigAToB1Tag) != 0;
    bool has_b2a0 = cmsIsTag(dest_profile, cmsSigBToA0Tag) != 0;

    if (!has_a2b0) {
        s.result = GateResult::Reject;
        s.message = "Destination ICC missing A2B0 (perceptual) table";
        return s;
    }
    if (!has_a2b1) {
        s.result = GateResult::Reject;
        s.message = "Destination ICC missing A2B1 (relative colorimetric) table";
        return s;
    }
    if (!has_b2a0) {
        s.result = GateResult::Reject;
        s.message = "Destination ICC missing B2A0 (inverse) table — required for proofing";
        return s;
    }

    int device_channels = cmsChannelsOf(cmsGetColorSpace(dest_profile));
    if (device_channels != 12) {
        s.result = GateResult::Warn;
        s.warn_level = WarnLevel::Yellow;
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Device has %d channels (not PRO 12) — extended gamut advantage reduced",
                 device_channels);
        s.message = buf;
        return s;
    }

    s.result = GateResult::Pass;
    s.message = "Destination ICC complete with 12-channel device tables";
    return s;
}

GateReport GamutGate::run_all(cmsHPROFILE src_profile, int bits_per_sample,
                               const uint16_t* data, int width, int height,
                               int channels, int extra_samples_type,
                               cmsHPROFILE display_profile,
                               cmsHPROFILE dest_profile) {
    GateReport report;
    report.g1_source_profile = check_source_profile(src_profile);
    report.g2_bit_depth = check_bit_depth(bits_per_sample);
    report.g3_alpha_clip = check_alpha_clip(data, width, height, channels, extra_samples_type);
    report.g4_display_gamut = check_display_gamut(display_profile);
    report.g5_dest_icc = check_dest_icc(dest_profile);
    return report;
}

}
}
