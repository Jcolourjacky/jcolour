#pragma once

#include <lcms2.h>
#include <vector>
#include <array>
#include <map>
#include <string>
#include <cstdint>

namespace jcolour {
namespace gamut {

struct SegmentMap {
    enum Class { Skin = 0, Sky, Foliage, Subject, Background, Other, COUNT };
    std::vector<float> masks[COUNT]; // float32, [H*W], 0-1
    std::vector<float> confidence;   // float32, [H*W], 0-1
    int width = 0;
    int height = 0;
    float overall_confidence = 0.0f;
};

struct SegmentIntent {
    int intent;      // LCMS2 intent constant
    bool bpc;
    float strength;  // 0.0=pure perceptual, 1.0=pure relative
};

class SemanticGamutMapper {
public:
    SemanticGamutMapper();
    ~SemanticGamutMapper();

    SegmentMap segment(const uint16_t* image_data, int width, int height,
                       int channels, cmsHPROFILE src_profile);

    std::vector<uint16_t> transform(
        const uint16_t* src_data, int width, int height, int channels,
        cmsHPROFILE src_profile, cmsHPROFILE dst_profile,
        const SegmentMap& segments,
        const std::map<SegmentMap::Class, SegmentIntent>& policy);

    struct Telemetry {
        std::array<float, 6> class_pixel_ratios;
        float overall_confidence;
        double inference_ms;
    };
    Telemetry last_telemetry() const;

private:
    struct Impl;
    Impl* pImpl_;
};

}
}
