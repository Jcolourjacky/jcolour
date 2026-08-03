#include "intent_advisor.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <vector>

namespace jcolour {
namespace gamut {

struct GamutAdvisor::Impl {
    cmsHPROFILE dest_profile = nullptr;
    AnalysisResult last_result{};
};

GamutAdvisor::GamutAdvisor() : pImpl_(new Impl) {}

GamutAdvisor::~GamutAdvisor() { delete pImpl_; }

void GamutAdvisor::set_destination_profile(cmsHPROFILE profile) {
    pImpl_->dest_profile = profile;
}

GamutAdvisor::AnalysisResult GamutAdvisor::last_analysis() const {
    return pImpl_->last_result;
}

std::array<IntentRecommendation, 2> GamutAdvisor::recommend(
    const uint16_t* image_data,
    int width, int height, int channels,
    cmsHPROFILE src_profile) {

    std::array<IntentRecommendation, 2> result{};
    auto& analysis = pImpl_->last_result;
    analysis = {};

    if (!image_data || !src_profile || width <= 0 || height <= 0) {
        result[0] = {INTENT_PERCEPTUAL, true, 0.50f, "Insufficient data — defaulting to perceptual"};
        result[1] = {INTENT_RELATIVE_COLORIMETRIC, true, 0.30f, "Fallback relative colorimetric"};
        return result;
    }

    // Transform image to Lab for analysis
    cmsHPROFILE lab_profile = cmsCreateLab4Profile(nullptr);
    cmsHTRANSFORM to_lab = cmsCreateTransform(
        src_profile, TYPE_RGB_16,
        lab_profile, TYPE_Lab_DBL,
        INTENT_RELATIVE_COLORIMETRIC,
        cmsFLAGS_NOCACHE | cmsFLAGS_NOOPTIMIZE);

    if (!to_lab) {
        cmsCloseProfile(lab_profile);
        result[0] = {INTENT_PERCEPTUAL, true, 0.50f, "Transform creation failed — defaulting to perceptual"};
        result[1] = {INTENT_RELATIVE_COLORIMETRIC, true, 0.30f, "Fallback"};
        return result;
    }

    // Build gamut check transform if dest profile available
    cmsHTRANSFORM gamut_check = nullptr;
    if (pImpl_->dest_profile) {
        gamut_check = cmsCreateProofingTransform(
            src_profile, TYPE_RGB_16,
            lab_profile, TYPE_Lab_DBL,
            pImpl_->dest_profile,
            INTENT_RELATIVE_COLORIMETRIC,
            INTENT_RELATIVE_COLORIMETRIC,
            cmsFLAGS_GAMUTCHECK | cmsFLAGS_NOCACHE);
    }

    size_t pixel_count = static_cast<size_t>(width) * height;
    size_t high_chroma_count = 0;
    size_t oog_count = 0;
    size_t skin_count = 0;
    size_t sky_count = 0;
    size_t foliage_count = 0;

    // Process in scanlines to avoid massive temp buffer
    std::vector<cmsCIELab> lab_row(width);
    int third_height = height / 3;

    for (int y = 0; y < height; ++y) {
        const uint16_t* row = image_data + static_cast<size_t>(y) * width * channels;
        cmsDoTransform(to_lab, row, lab_row.data(), width);

        for (int x = 0; x < width; ++x) {
            const cmsCIELab& lab = lab_row[x];
            double C = std::sqrt(lab.a * lab.a + lab.b * lab.b);

            if (C > 90.0) ++high_chroma_count;

            // Skin detection: a* in [5,25], b* in [10,40], L* in [40,80]
            if (lab.a >= 5.0 && lab.a <= 25.0 &&
                lab.b >= 10.0 && lab.b <= 40.0 &&
                lab.L >= 40.0 && lab.L <= 80.0) {
                ++skin_count;
            }

            // Sky detection: b* < -10, L* > 50, upper 1/3 weighted
            if (lab.b < -10.0 && lab.L > 50.0) {
                sky_count += (y < third_height) ? 2 : 1;
            }

            // Foliage detection: a* < -5, b* > 5, L* in [20,70]
            if (lab.a < -5.0 && lab.b > 5.0 &&
                lab.L >= 20.0 && lab.L <= 70.0) {
                ++foliage_count;
            }
        }

        // Out-of-gamut check via gamut check transform
        if (gamut_check) {
            std::vector<cmsCIELab> proof_row(width);
            cmsDoTransform(gamut_check, row, proof_row.data(), width);
            for (int x = 0; x < width; ++x) {
                double dL = lab_row[x].L - proof_row[x].L;
                double da = lab_row[x].a - proof_row[x].a;
                double db = lab_row[x].b - proof_row[x].b;
                if (std::sqrt(dL*dL + da*da + db*db) > 3.0) ++oog_count;
            }
        }
    }

    cmsDeleteTransform(to_lab);
    if (gamut_check) cmsDeleteTransform(gamut_check);
    cmsCloseProfile(lab_profile);

    // Sky count is inflated by upper-third weighting, normalize
    double effective_sky = static_cast<double>(sky_count) / 1.33;

    analysis.high_chroma_ratio = static_cast<float>(high_chroma_count) / pixel_count;
    analysis.out_of_gamut_ratio = static_cast<float>(oog_count) / pixel_count;
    analysis.skin_ratio = static_cast<float>(skin_count) / pixel_count;
    analysis.sky_ratio = static_cast<float>(effective_sky) / pixel_count;
    analysis.foliage_ratio = static_cast<float>(foliage_count) / pixel_count;
    analysis.memory_color_ratio = std::max(analysis.skin_ratio,
                                           analysis.sky_ratio + analysis.foliage_ratio);

    // Decision logic per spec
    char buf[512];

    if (analysis.high_chroma_ratio > 0.20f && analysis.memory_color_ratio < 0.30f) {
        snprintf(buf, sizeof(buf),
                 "High saturation image (C*>90 pixels: %.0f%%) — relative colorimetric + BPC "
                 "pushes Or/Gr/V to boundary for maximum vibrancy",
                 analysis.high_chroma_ratio * 100.0f);
        result[0] = {INTENT_RELATIVE_COLORIMETRIC, true, 0.85f, buf};

        result[1] = {INTENT_PERCEPTUAL, true, 0.50f,
                     "If hard clipping is undesirable, perceptual provides smoother rolloff"};

    } else if (analysis.memory_color_ratio > 0.30f) {
        snprintf(buf, sizeof(buf),
                 "Memory colors detected (skin/sky/foliage: %.0f%%) — perceptual preserves "
                 "natural tonal relationships",
                 analysis.memory_color_ratio * 100.0f);
        result[0] = {INTENT_PERCEPTUAL, true, 0.80f, buf};

        result[1] = {INTENT_RELATIVE_COLORIMETRIC, true, 0.40f,
                     "Relative colorimetric for higher saturation, but memory colors may shift"};

    } else if (analysis.out_of_gamut_ratio < 0.05f) {
        snprintf(buf, sizeof(buf),
                 "Image nearly entirely within P9500 gamut (out-of-gamut: %.1f%%) — "
                 "relative colorimetric gives 1:1 reproduction",
                 analysis.out_of_gamut_ratio * 100.0f);
        result[0] = {INTENT_RELATIVE_COLORIMETRIC, true, 0.90f, buf};

        result[1] = {INTENT_PERCEPTUAL, true, 0.40f,
                     "Perceptual available but unnecessary for this image"};

    } else {
        result[0] = {INTENT_PERCEPTUAL, true, 0.65f,
                     "Mixed content — perceptual provides balanced gamut mapping"};

        result[1] = {INTENT_RELATIVE_COLORIMETRIC, true, 0.55f,
                     "Relative colorimetric for more vivid output at the cost of some clipping"};
    }

    return result;
}

}
}
