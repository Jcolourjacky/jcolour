// §8.5 OGV recruitment meter — image-dependent (the differentiator vs
// Esko/ColorLogic). Synthetic Lab images: each hue field should light up the
// right meter, neutral should light none.

#include "ogv_meter.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include <cstdio>

using namespace jcolour::gamut;
using Catch::Approx;

namespace {
// Fill an image with one Lab colour (n pixels).
std::vector<float> field(double L, double C, double hue_deg, int n = 4096) {
    std::vector<float> v(n * 3);
    double h = hue_deg * 3.14159265358979 / 180.0;
    for (int i = 0; i < n; ++i) {
        v[i*3+0] = (float)L;
        v[i*3+1] = (float)(C * std::cos(h));
        v[i*3+2] = (float)(C * std::sin(h));
    }
    return v;
}
}

TEST_CASE("OGV meter lights the right channel per hue", "[ogv][meter]") {
    auto orange = field(62, 88, 55);    // vivid orange
    auto green  = field(58, 85, 150);   // vivid green
    auto violet = field(40, 80, 295);   // vivid violet
    auto neutral= field(55,  2, 0);     // near-neutral
    auto dullOr = field(62, 40, 55);    // orange but low chroma (CMYK reaches)

    auto uo = estimate_ogv_usage(orange.data(), orange.size()/3);
    auto ug = estimate_ogv_usage(green.data(),  green.size()/3);
    auto uv = estimate_ogv_usage(violet.data(), violet.size()/3);
    auto un = estimate_ogv_usage(neutral.data(),neutral.size()/3);
    auto ud = estimate_ogv_usage(dullOr.data(), dullOr.size()/3);

    // each vivid field drives its own channel hardest
    REQUIRE(uo.or_pct > 0.5f);  REQUIRE(uo.gr_pct < 0.05f); REQUIRE(uo.v_pct < 0.05f);
    REQUIRE(ug.gr_pct > 0.5f);  REQUIRE(ug.or_pct < 0.05f); REQUIRE(ug.v_pct < 0.05f);
    REQUIRE(uv.v_pct  > 0.5f);  REQUIRE(uv.or_pct < 0.05f); REQUIRE(uv.gr_pct < 0.05f);

    // neutral & in-CMYK-gamut dull orange recruit (almost) nothing
    REQUIRE(un.extension_ratio < 0.01f);
    REQUIRE(ud.or_pct < 0.05f);

    // summaries reflect §8.5 thresholds
    REQUIRE(un.summary.find("基本未用") != std::string::npos);
    REQUIRE(uo.summary.find("充分调用") != std::string::npos);

    std::printf("\n[ogv-meter] vivid-orange : Or %.0f%% Gr %.0f%% V %.0f%%  ext %.0f%%  | %s\n",
                uo.or_pct*100, uo.gr_pct*100, uo.v_pct*100, uo.extension_ratio*100, uo.summary.c_str());
    std::printf("[ogv-meter] vivid-green  : Or %.0f%% Gr %.0f%% V %.0f%%  | %s\n",
                ug.or_pct*100, ug.gr_pct*100, ug.v_pct*100, ug.summary.c_str());
    std::printf("[ogv-meter] vivid-violet : Or %.0f%% Gr %.0f%% V %.0f%%  | %s\n",
                uv.or_pct*100, uv.gr_pct*100, uv.v_pct*100, uv.summary.c_str());
    std::printf("[ogv-meter] neutral      : ext %.0f%%  | %s\n",
                un.extension_ratio*100, un.summary.c_str());
    std::printf("[ogv-meter] dull-orange  : Or %.0f%%  | %s\n", ud.or_pct*100, ud.summary.c_str());
}

TEST_CASE("OGV meter: mixed image gives partial recruitment", "[ogv][meter]") {
    // half vivid orange, half neutral
    auto img = field(62, 88, 55, 2048);
    auto neu = field(55, 2, 0, 2048);
    img.insert(img.end(), neu.begin(), neu.end());
    auto u = estimate_ogv_usage(img.data(), img.size()/3);
    REQUIRE(u.or_pct > 0.2f);
    REQUIRE(u.or_pct < 0.6f);                 // diluted by neutral half
    REQUIRE(u.or_ratio == Approx(0.5f).margin(0.05));
}
