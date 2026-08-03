#pragma once

#include <lcms2.h>
#include <array>
#include <string>
#include <cstdint>

namespace jcolour {
namespace gamut {

struct IntentRecommendation {
    int intent;           // LCMS2: 0=Perceptual, 1=Relative, 2=Saturation, 3=Absolute
    bool bpc;
    float confidence;     // 0.0–1.0
    std::string rationale;
};

class GamutAdvisor {
public:
    GamutAdvisor();
    ~GamutAdvisor();

    void set_destination_profile(cmsHPROFILE profile);

    std::array<IntentRecommendation, 2> recommend(
        const uint16_t* image_data,
        int width, int height, int channels,
        cmsHPROFILE src_profile);

    struct AnalysisResult {
        float high_chroma_ratio;
        float out_of_gamut_ratio;
        float skin_ratio;
        float sky_ratio;
        float foliage_ratio;
        float memory_color_ratio;
    };
    AnalysisResult last_analysis() const;

private:
    struct Impl;
    Impl* pImpl_;
};

}
}
