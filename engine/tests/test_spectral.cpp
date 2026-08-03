#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "spectral.h"
#include "delta_e.h"
#include <cmath>
#include <array>

using namespace jcolour::spectral;
using namespace jcolour::delta_e;

TEST_CASE("Illuminant D50", "[spectral][illuminant]") {
    auto d50 = Illuminant::D50();
    double sum = 0;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        sum += d50[i];
    }
    REQUIRE(sum > 0);
    REQUIRE(sum < 100);
}

TEST_CASE("Illuminant D65", "[spectral][illuminant]") {
    auto d65 = Illuminant::D65();
    double sum = 0;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        sum += d65[i];
    }
    REQUIRE(sum > 0);
    REQUIRE(sum < 100);
}

TEST_CASE("Illuminant F2", "[spectral][illuminant]") {
    auto f2 = Illuminant::F2();
    double sum = 0;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        sum += f2[i];
        REQUIRE(f2[i] >= 0);
    }
    REQUIRE(sum > 0);
    REQUIRE(sum < 100);
    // F2 normalized at 560nm (index 18): value should be ~0.0754
    REQUIRE(f2[18] == Catch::Approx(0.0754).margin(0.001));
}

TEST_CASE("Illuminant F11", "[spectral][illuminant]") {
    auto f11 = Illuminant::F11();
    double sum = 0;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        sum += f11[i];
        REQUIRE(f11[i] >= 0);
    }
    REQUIRE(sum > 0);
    REQUIRE(sum < 100);
}

TEST_CASE("Illuminant LED3000K", "[spectral][illuminant]") {
    auto led = Illuminant::LED3000K();
    double sum = 0;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        sum += led[i];
        REQUIRE(led[i] >= 0);
    }
    REQUIRE(sum > 0);
    // LED 3000K should peak in warm (long wavelength) region
    REQUIRE(led[18] > led[5]);  // 560nm > 430nm
}

TEST_CASE("Illuminant LED5000K", "[spectral][illuminant]") {
    auto led = Illuminant::LED5000K();
    double sum = 0;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        sum += led[i];
        REQUIRE(led[i] >= 0);
    }
    REQUIRE(sum > 0);
    // LED 5000K should have significant output across visible range
    REQUIRE(led[18] > 0.9);  // 560nm near peak
}

TEST_CASE("All illuminants produce valid XYZ", "[spectral][illuminant]") {
    // Perfect white reflectance (all 1.0) under any illuminant should give Y≈1
    Spectrum white;
    white.fill(1.0);
    std::array<Spectrum, 5> illums = {
        Illuminant::D50(), Illuminant::D65(), Illuminant::A(),
        Illuminant::F2(), Illuminant::F11()
    };
    for (const auto& illum : illums) {
        auto xyz = spectrum_to_xyz(white, illum);
        REQUIRE(xyz.Y == Catch::Approx(1.0).margin(0.05));
    }
}

TEST_CASE("RGB to spectrum conversion", "[spectral][rgb]") {
    auto spec = rgb_to_spectrum({1.0, 1.0, 1.0});
    double sum = 0;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        sum += spec[i];
    }
    REQUIRE(sum > 0);
}

TEST_CASE("Spectrum to XYZ", "[spectral][xyz]") {
    auto d50 = Illuminant::D50();
    auto xyz = spectrum_to_xyz(d50);
    REQUIRE(xyz.X > 0);
    REQUIRE(xyz.Y > 0);
    REQUIRE(xyz.Z > 0);
}

TEST_CASE("XYZ to Lab conversion", "[spectral][lab]") {
    auto d65 = Illuminant::D65();
    auto xyz = spectrum_to_xyz(d65);
    auto lab = xyz_to_lab(xyz);
    REQUIRE(lab.L > 0);
    REQUIRE(lab.L < 200);
    REQUIRE(std::abs(lab.a) < 100);
    REQUIRE(std::abs(lab.b) < 100);
}

TEST_CASE("Spectrum roundtrip through Lab", "[spectral]") {
    auto spec = rgb_to_spectrum({0.5, 0.3, 0.8});
    auto xyz = spectrum_to_xyz(spec);
    auto lab = xyz_to_lab(xyz);
    REQUIRE(lab.L >= 0);
    REQUIRE(lab.L <= 100);
}

TEST_CASE("Neutral gray Lab values", "[spectral][lab]") {
    for (double gray : {0.0, 0.2, 0.5, 0.8, 1.0}) {
        auto spec = rgb_to_spectrum({gray, gray, gray});
        auto lab = xyz_to_lab(spectrum_to_xyz(spec));
        REQUIRE(std::abs(lab.a) < 0.5);
        REQUIRE(std::abs(lab.b) < 0.5);
    }
}

TEST_CASE("Pure red spectrum", "[spectral][rgb]") {
    auto spec = rgb_to_spectrum({1.0, 0.0, 0.0});
    auto lab = xyz_to_lab(spectrum_to_xyz(spec));
    REQUIRE(lab.a > 20);
    REQUIRE(lab.b > -10);
}

TEST_CASE("Pure blue spectrum", "[spectral][rgb]") {
    auto spec = rgb_to_spectrum({0.0, 0.0, 1.0});
    auto lab = xyz_to_lab(spectrum_to_xyz(spec));
    REQUIRE(lab.b < -20);
}

TEST_CASE("Wavelength array consistency", "[spectral]") {
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        REQUIRE(WAVELENGTHS[i] == Catch::Approx(WAVELENGTH_MIN + i * WAVELENGTH_STEP));
    }
    REQUIRE(NUM_CHANNELS == 36);
}
