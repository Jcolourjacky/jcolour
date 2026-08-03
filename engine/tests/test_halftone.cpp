#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "halftone.h"
#include <cmath>
#include <numeric>

using namespace jcolour::halftone;

TEST_CASE("halftone: all-zero input produces all-zero output", "[halftone]") {
    ImageF in(32, 32, 0.0);
    auto out = floyd_steinberg(in);
    for (uint8_t v : out.data) REQUIRE(v == 0);
}

TEST_CASE("halftone: all-one input produces all-one output", "[halftone]") {
    ImageF in(32, 32, 1.0);
    auto out = floyd_steinberg(in);
    for (uint8_t v : out.data) REQUIRE(v == 1);
}

TEST_CASE("halftone: dot density matches input coverage (50%)", "[halftone]") {
    ImageF in(32, 32, 0.5);
    auto out = floyd_steinberg(in);
    double density = dot_density(out);
    REQUIRE(density == Catch::Approx(0.5).margin(0.02));
}

TEST_CASE("halftone: dot density matches input coverage (25%)", "[halftone]") {
    ImageF in(32, 32, 0.25);
    auto out = floyd_steinberg(in);
    double density = dot_density(out);
    REQUIRE(density == Catch::Approx(0.25).margin(0.02));
}

TEST_CASE("halftone: dot density matches input coverage (75%)", "[halftone]") {
    ImageF in(32, 32, 0.75);
    auto out = floyd_steinberg(in);
    double density = dot_density(out);
    REQUIRE(density == Catch::Approx(0.75).margin(0.02));
}

TEST_CASE("halftone: output is strictly binary (0 or 1)", "[halftone]") {
    ImageF in(32, 32, 0.3);
    auto out = floyd_steinberg(in);
    for (uint8_t v : out.data) REQUIRE((v == 0 || v == 1));
}

TEST_CASE("halftone: gradient image has monotone density", "[halftone]") {
    // Each row has a different constant coverage; density should increase
    int w = 32, h = 10;
    ImageF in(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            in.at(x, y) = static_cast<double>(y) / (h - 1);

    auto out = floyd_steinberg(in);

    // Row 0 should have lower density than row h-1
    int ones_first = 0, ones_last = 0;
    for (int x = 0; x < w; ++x) {
        ones_first += out.get(x, 0);
        ones_last  += out.get(x, h - 1);
    }
    REQUIRE(ones_last >= ones_first);
}

TEST_CASE("halftone: dot_density of all-zero is 0", "[halftone]") {
    ImageB b(16, 16);
    REQUIRE(dot_density(b) == Catch::Approx(0.0).margin(1e-12));
}

TEST_CASE("halftone: dot_density of all-one is 1", "[halftone]") {
    ImageB b(16, 16);
    for (auto& v : b.data) v = 1;
    REQUIRE(dot_density(b) == Catch::Approx(1.0).margin(1e-12));
}

// ---- Vector Error Diffusion (A16) ----

TEST_CASE("ved: 4-channel all-zero produces all-zero output", "[ved]") {
    ImageMF in(64, 64, 4, 0.0);
    auto out = vector_error_diffusion(in);
    for (uint8_t v : out.data) REQUIRE(v == 0);
}

TEST_CASE("ved: 4-channel all-one produces all-one output", "[ved]") {
    ImageMF in(64, 64, 4, 1.0);
    auto out = vector_error_diffusion(in);
    for (uint8_t v : out.data) REQUIRE(v == 1);
}

TEST_CASE("ved: 4-channel density matches coverage within 1%", "[ved]") {
    // Each channel has a different constant coverage
    std::vector<double> targets = {0.2, 0.4, 0.6, 0.8};
    int w = 64, h = 64;
    ImageMF in(w, h, 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < 4; ++c)
                in.at(x, y, c) = targets[c];

    auto out = vector_error_diffusion(in);

    for (int c = 0; c < 4; ++c) {
        double density = dot_density(out, c);
        REQUIRE(density == Catch::Approx(targets[c]).margin(0.01));
    }
}

TEST_CASE("ved: output is strictly binary", "[ved]") {
    ImageMF in(64, 64, 4, 0.35);
    auto out = vector_error_diffusion(in);
    for (uint8_t v : out.data) REQUIRE((v == 0 || v == 1));
}

TEST_CASE("ved: channels are independent (different coverages)", "[ved]") {
    int w = 64, h = 64;
    ImageMF in(w, h, 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            in.at(x, y, 0) = 0.1;
            in.at(x, y, 1) = 0.3;
            in.at(x, y, 2) = 0.7;
            in.at(x, y, 3) = 0.9;
        }

    auto out = vector_error_diffusion(in);

    REQUIRE(dot_density(out, 0) == Catch::Approx(0.1).margin(0.01));
    REQUIRE(dot_density(out, 1) == Catch::Approx(0.3).margin(0.01));
    REQUIRE(dot_density(out, 2) == Catch::Approx(0.7).margin(0.01));
    REQUIRE(dot_density(out, 3) == Catch::Approx(0.9).margin(0.01));
}
