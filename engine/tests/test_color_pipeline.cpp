#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "color_pipeline.h"
#include "tiff_io.h"
#include "spectral.h"
#include <lcms2.h>
#include <vector>
#include <cstring>

TEST_CASE("ColorPipeline construction", "[pipeline]") {
    auto pipeline = jcolour::ColorPipeline();
    auto stats = pipeline.get_stats();
    REQUIRE(stats.avg_de00 == 0.0);
}

TEST_CASE("ColorPipeline with custom config", "[pipeline]") {
    jcolour::ColorPipeline::Config config;
    config.use_36_channel = true;
    config.rendering_intent = 1; // Relative
    config.use_bpc = true;

    auto pipeline = jcolour::ColorPipeline(config);
    auto stats = pipeline.get_stats();

    REQUIRE(stats.avg_de00 == 0.0);
    REQUIRE(stats.max_de00 == 0.0);
}

TEST_CASE("ColorPipeline transform without profiles", "[pipeline]") {
    auto pipeline = jcolour::ColorPipeline();
    std::vector<uint16_t> input(100 * 100 * 3);
    std::fill(input.begin(), input.end(), 32768); // Mid gray

    auto output = pipeline.transform_tiff(input, 100, 100, 3);
    REQUIRE(output.size() == input.size());
}

TEST_CASE("ColorPipeline stats initialization", "[pipeline]") {
    auto pipeline = jcolour::ColorPipeline();
    auto stats = pipeline.get_stats();

    REQUIRE(stats.max_de00 == 0.0);
    REQUIRE(stats.avg_de00 == 0.0);
    REQUIRE(stats.gray_avg_de00 == 0.0);
}

TEST_CASE("DeltaEResult default values", "[pipeline]") {
    jcolour::DeltaEResult result;
    REQUIRE(result.de00 == 0.0);
    REQUIRE(result.de94 == 0.0);
    REQUIRE(result.de76 == 0.0);
    REQUIRE(result.valid == false);
}

TEST_CASE("ColorPipeline Config struct", "[pipeline]") {
    jcolour::ColorPipeline::Config config;

    REQUIRE(config.use_36_channel == true);  // default
    REQUIRE(config.rendering_intent == 1);    // default
    REQUIRE(config.use_bpc == true);         // default
}

TEST_CASE("ColorPipeline measure_accuracy with invalid path", "[pipeline]") {
    auto pipeline = jcolour::ColorPipeline();
    std::vector<uint16_t> input(100 * 100 * 3, 32768);

    auto result = pipeline.measure_accuracy(input, 100, 100, "/nonexistent.tiff");
    REQUIRE(result.valid == false);
}

TEST_CASE("ColorPipeline Stats struct", "[pipeline]") {
    auto pipeline = jcolour::ColorPipeline();
    auto stats = pipeline.get_stats();

    REQUIRE(stats.max_de00 == 0.0);
    REQUIRE(stats.avg_de00 == 0.0);
    REQUIRE(stats.gray_avg_de00 == 0.0);
    REQUIRE(stats.skin_avg_de00 == 0.0);
    REQUIRE(stats.saturated_avg_de00 == 0.0);
}

TEST_CASE("sRGB white converts to Lab ~[100,0,0]", "[pipeline][lcms2]") {
    cmsHPROFILE srgb_profile = cmsCreate_sRGBProfile();
    cmsHPROFILE lab_profile  = cmsCreateLab4Profile(nullptr);  // D50

    cmsHTRANSFORM xform = cmsCreateTransform(
        srgb_profile, TYPE_RGB_16,
        lab_profile,  TYPE_Lab_DBL,
        INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOCACHE
    );

    uint16_t white[3] = {65535, 65535, 65535};
    cmsCIELab result{};
    cmsDoTransform(xform, white, &result, 1);

    cmsDeleteTransform(xform);
    cmsCloseProfile(srgb_profile);
    cmsCloseProfile(lab_profile);

    REQUIRE(result.L == Catch::Approx(100.0).margin(1.0));
    REQUIRE(result.a == Catch::Approx(0.0).margin(2.0));
    REQUIRE(result.b == Catch::Approx(0.0).margin(2.0));
}

TEST_CASE("sRGB black converts to Lab ~[0,0,0]", "[pipeline][lcms2]") {
    cmsHPROFILE srgb_profile = cmsCreate_sRGBProfile();
    cmsHPROFILE lab_profile  = cmsCreateLab4Profile(nullptr);

    cmsHTRANSFORM xform = cmsCreateTransform(
        srgb_profile, TYPE_RGB_16,
        lab_profile,  TYPE_Lab_DBL,
        INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOCACHE
    );

    uint16_t black[3] = {0, 0, 0};
    cmsCIELab result{};
    cmsDoTransform(xform, black, &result, 1);

    cmsDeleteTransform(xform);
    cmsCloseProfile(srgb_profile);
    cmsCloseProfile(lab_profile);

    REQUIRE(result.L == Catch::Approx(0.0).margin(1.0));
    REQUIRE(result.a == Catch::Approx(0.0).margin(2.0));
    REQUIRE(result.b == Catch::Approx(0.0).margin(2.0));
}

TEST_CASE("sRGB mid-gray converts to Lab L~50", "[pipeline][lcms2]") {
    cmsHPROFILE srgb_profile = cmsCreate_sRGBProfile();
    cmsHPROFILE lab_profile  = cmsCreateLab4Profile(nullptr);

    cmsHTRANSFORM xform = cmsCreateTransform(
        srgb_profile, TYPE_RGB_16,
        lab_profile,  TYPE_Lab_DBL,
        INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOCACHE
    );

    // sRGB 128/255 ≈ 0.502; after gamma removal ≈ 0.216; L* ≈ 53.4
    uint16_t gray[3] = {32896, 32896, 32896};  // 128/255 * 65535
    cmsCIELab result{};
    cmsDoTransform(xform, gray, &result, 1);

    cmsDeleteTransform(xform);
    cmsCloseProfile(srgb_profile);
    cmsCloseProfile(lab_profile);

    REQUIRE(result.L > 40.0);
    REQUIRE(result.L < 65.0);
    REQUIRE(result.a == Catch::Approx(0.0).margin(3.0));
    REQUIRE(result.b == Catch::Approx(0.0).margin(3.0));
}
