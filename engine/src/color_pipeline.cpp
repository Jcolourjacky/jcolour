#include "color_pipeline.h"
#include "spectral.h"
#include "tiff_io.h"
#include <lcms2.h>
#include <cstring>
#include <algorithm>
#include <limits>

namespace jcolour {

struct ColorPipeline::Impl {
    cmsHPROFILE source_profile = nullptr;
    cmsHPROFILE dest_profile = nullptr;
    cmsHTRANSFORM transform = nullptr;
    bool transform_dirty = true;

    Config config;
    Stats stats{};

    void rebuild_transform() {
        if (source_profile && dest_profile) {
            if (transform) cmsDeleteTransform(transform);

            cmsUInt32Number flags = cmsFLAGS_NOCACHE;
            int intent = config.rendering_intent;
            if (config.use_bpc) {
                flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
            }

            transform = cmsCreateTransform(
                source_profile,
                TYPE_RGB_16,
                dest_profile,
                TYPE_RGB_16,
                intent,
                flags
            );
            transform_dirty = false;
        }
    }

    ~Impl() {
        if (transform) cmsDeleteTransform(transform);
        if (source_profile) cmsCloseProfile(source_profile);
        if (dest_profile) cmsCloseProfile(dest_profile);
    }
};

ColorPipeline::ColorPipeline() : ColorPipeline(Config{}) {}

ColorPipeline::ColorPipeline(const Config& config) : pImpl_(new Impl) {
    pImpl_->config = config;
}

ColorPipeline::~ColorPipeline() = default;

void ColorPipeline::set_source_profile(const char* profile_path) {
    if (pImpl_->source_profile) {
        cmsCloseProfile(pImpl_->source_profile);
    }
    pImpl_->source_profile = cmsOpenProfileFromFile(profile_path, "r");
    if (pImpl_->source_profile) {
        pImpl_->transform_dirty = true;
    }
}

void ColorPipeline::set_destination_profile(const char* profile_path) {
    if (pImpl_->dest_profile) {
        cmsCloseProfile(pImpl_->dest_profile);
    }
    pImpl_->dest_profile = cmsOpenProfileFromFile(profile_path, "r");
    if (pImpl_->dest_profile) {
        pImpl_->transform_dirty = true;
    }
}

std::vector<uint16_t> ColorPipeline::transform_tiff(
    const std::vector<uint16_t>& input,
    int width, int height, int channels) {

    if (pImpl_->transform_dirty) {
        pImpl_->rebuild_transform();
    }

    std::vector<uint16_t> output(width * height * channels);

    if (pImpl_->transform) {
        cmsDoTransform(pImpl_->transform,
            input.data(), output.data(),
            width * height);
    } else {
        std::memcpy(output.data(), input.data(),
                   input.size() * sizeof(uint16_t));
    }

    return output;
}

DeltaEResult ColorPipeline::measure_accuracy(
    const std::vector<uint16_t>& input,
    int width, int height,
    const char* reference_tiff) {

    DeltaEResult result = {};

    auto ref_image = io::TIFFReader::read(reference_tiff);
    if (!ref_image.data.empty()) {
        auto report = io::verify_against_reference(
            io::TIFFImage{width, height, 16, 3, input},
            reference_tiff, nullptr
        );
        pImpl_->stats.max_de00 = report.max_de00;
        pImpl_->stats.avg_de00 = report.avg_de00;
        result.de00 = report.avg_de00;
        result.valid = true;
    }

    return result;
}

}
