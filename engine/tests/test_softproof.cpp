#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "softproof.h"

#include <lcms2.h>
#include <vector>

using jcolour::gamut::SoftproofConfig;
using jcolour::gamut::SoftproofEngine;
using jcolour::gamut::SoftproofResult;

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 6;

std::vector<uint16_t> make_gradient_rgb(int w, int h) {
    std::vector<uint16_t> img(static_cast<size_t>(w) * h * 3);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t idx = (static_cast<size_t>(y) * w + x) * 3;
            img[idx + 0] = static_cast<uint16_t>(x * 65535 / (w - 1));
            img[idx + 1] = static_cast<uint16_t>(y * 65535 / (h - 1));
            img[idx + 2] = 32768;
        }
    }
    return img;
}

}  // namespace

TEST_CASE("SoftproofEngine construction and config", "[softproof]") {
    SoftproofEngine engine;
    SoftproofConfig cfg;
    cfg.paper_white_simulation = true;
    cfg.paper_name = "Hahnemühle Photo Rag";
    cfg.gamut_warning_enabled = true;
    REQUIRE_NOTHROW(engine.set_config(cfg));
}

TEST_CASE("SoftproofEngine render returns empty on bad inputs", "[softproof]") {
    SoftproofEngine engine;
    auto img = make_gradient_rgb(kWidth, kHeight);
    cmsHPROFILE srgb = cmsCreate_sRGBProfile();

    SoftproofResult r = engine.render(nullptr, kWidth, kHeight, 3,
                                       srgb, srgb, srgb, INTENT_PERCEPTUAL, true);
    REQUIRE(r.proofed_image.empty());
    REQUIRE(r.width == kWidth);
    REQUIRE(r.height == kHeight);

    r = engine.render(img.data(), 0, kHeight, 3,
                      srgb, srgb, srgb, INTENT_PERCEPTUAL, true);
    REQUIRE(r.proofed_image.empty());

    cmsCloseProfile(srgb);
}

TEST_CASE("SoftproofEngine render produces output of expected shape", "[softproof]") {
    SoftproofEngine engine;
    SoftproofConfig cfg;
    cfg.paper_white_simulation = true;
    cfg.gamut_warning_enabled = true;
    engine.set_config(cfg);

    auto img = make_gradient_rgb(kWidth, kHeight);
    cmsHPROFILE srgb = cmsCreate_sRGBProfile();

    // src=printer=display all sRGB — degenerate but valid configuration:
    // no real gamut mismatch, but pipeline must still produce well-formed
    // buffers and not crash.
    SoftproofResult r = engine.render(img.data(), kWidth, kHeight, 3,
                                       srgb, srgb, srgb,
                                       INTENT_RELATIVE_COLORIMETRIC, true);

    REQUIRE(r.width == kWidth);
    REQUIRE(r.height == kHeight);
    REQUIRE(r.proofed_image.size() == static_cast<size_t>(kWidth) * kHeight * 3);
    REQUIRE(r.gamut_warning_mask.size() == static_cast<size_t>(kWidth) * kHeight);
    REQUIRE(r.out_of_gamut_percent >= 0.0f);
    REQUIRE(r.out_of_gamut_percent <= 100.0f);

    // sRGB→sRGB has no extended-gamut device, so meter must stay zero.
    REQUIRE(r.ink_coverage_ogv[0] == Catch::Approx(0.0f));
    REQUIRE(r.ink_coverage_ogv[1] == Catch::Approx(0.0f));
    REQUIRE(r.ink_coverage_ogv[2] == Catch::Approx(0.0f));

    cmsCloseProfile(srgb);
}

TEST_CASE("SoftproofEngine custom threshold falls back to default when zero", "[softproof]") {
    SoftproofEngine engine;
    SoftproofConfig cfg;
    cfg.gamut_warning_enabled = true;
    cfg.gamut_warning_threshold = 0.0f;  // should trigger 3.0 fallback
    engine.set_config(cfg);

    auto img = make_gradient_rgb(kWidth, kHeight);
    cmsHPROFILE srgb = cmsCreate_sRGBProfile();
    SoftproofResult r = engine.render(img.data(), kWidth, kHeight, 3,
                                       srgb, srgb, srgb,
                                       INTENT_RELATIVE_COLORIMETRIC, true);

    // Identical src/printer → ΔE should be ~0 → no OOG even with default 3.0.
    REQUIRE(r.out_of_gamut_percent < 1.0f);

    cmsCloseProfile(srgb);
}

TEST_CASE("SoftproofEngine paper-tint shift runs without crashing", "[softproof]") {
    SoftproofEngine engine;
    SoftproofConfig cfg;
    cfg.paper_white_simulation = true;
    cfg.paper_white_a = 1.5f;
    cfg.paper_white_b = 3.0f;
    engine.set_config(cfg);

    auto img = make_gradient_rgb(kWidth, kHeight);
    cmsHPROFILE srgb = cmsCreate_sRGBProfile();
    SoftproofResult r = engine.render(img.data(), kWidth, kHeight, 3,
                                       srgb, srgb, srgb,
                                       INTENT_RELATIVE_COLORIMETRIC, true);

    REQUIRE(r.proofed_image.size() == static_cast<size_t>(kWidth) * kHeight * 3);

    cmsCloseProfile(srgb);
}

TEST_CASE("is_display_adequate handles null", "[softproof]") {
    REQUIRE(SoftproofEngine::is_display_adequate(nullptr) == false);
    REQUIRE(SoftproofEngine::check_display_coverage(nullptr) == Catch::Approx(0.0f));
}

TEST_CASE("check_display_coverage returns 0–1", "[softproof]") {
    cmsHPROFILE srgb = cmsCreate_sRGBProfile();
    float c = SoftproofEngine::check_display_coverage(srgb);
    REQUIRE(c >= 0.0f);
    REQUIRE(c <= 1.0f);
    cmsCloseProfile(srgb);
}
