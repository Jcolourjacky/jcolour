#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "delta_e.h"
#include <cmath>

using namespace jcolour::delta_e;
using namespace jcolour::spectral;

TEST_CASE("ΔE2000 identical colors", "[delta_e][de00]") {
    Lab lab1{50.0, 0.0, 0.0};
    Lab lab2{50.0, 0.0, 0.0};
    REQUIRE(delta_e_2000(lab1, lab2) == Catch::Approx(0.0).margin(0.001));
}

TEST_CASE("ΔE2000 near-identical colors", "[delta_e][de00]") {
    Lab lab1{50.0, 0.0, 0.0};
    Lab lab2{50.0, 0.1, 0.1};
    double de = delta_e_2000(lab1, lab2);
    REQUIRE(de < 1.0);
}

TEST_CASE("ΔE2000 significantly different colors", "[delta_e][de00]") {
    Lab lab1{50.0, 0.0, 0.0};
    Lab lab2{70.0, 30.0, -20.0};
    double de = delta_e_2000(lab1, lab2);
    REQUIRE(de > 5.0);
}

TEST_CASE("ΔE2000 for gray axis", "[delta_e][de00]") {
    Lab lab1{50.0, 0.0, 0.0};
    Lab lab2{45.0, 0.0, 0.0};
    double de = delta_e_2000(lab1, lab2);
    REQUIRE(de < 10.0);
    REQUIRE(de > 0);
}

TEST_CASE("ΔE76 identical colors", "[delta_e][de76]") {
    Lab lab1{50.0, 10.0, 20.0};
    Lab lab2{50.0, 10.0, 20.0};
    REQUIRE(delta_e_76(lab1, lab2) == Catch::Approx(0.0).margin(0.001));
}

TEST_CASE("ΔE94 identical colors", "[delta_e][de94]") {
    Lab lab1{50.0, 10.0, 20.0};
    Lab lab2{50.0, 10.0, 20.0};
    REQUIRE(delta_e_94(lab1, lab2) == Catch::Approx(0.0).margin(0.001));
}

TEST_CASE("ΔE2000 ≤ 1.0 acceptance criteria", "[delta_e][acceptance]") {
    Lab lab1{50.0, 5.0, 5.0};
    Lab lab2{50.5, 5.2, 5.1};
    double de = delta_e_2000(lab1, lab2);
    REQUIRE(de <= 1.0);
}

TEST_CASE("ΔE2000 gray stage ≤ 0.5 acceptance criteria", "[delta_e][acceptance]") {
    for (double L : {20.0, 40.0, 60.0, 80.0}) {
        Lab lab1{L, 0.0, 0.0};
        Lab lab2{L + 0.3, 0.0, 0.0};
        double de = delta_e_2000(lab1, lab2);
        REQUIRE(de <= 0.5);
    }
}

TEST_CASE("ΔE2000 skin tone tolerance ≤ 1.5", "[delta_e][acceptance]") {
    Lab skin1{60.0, 15.0, 25.0};
    Lab skin2{60.5, 15.5, 25.5};
    double de = delta_e_2000(skin1, skin2);
    REQUIRE(de <= 1.5);
}

TEST_CASE("ΔE2000 red line: > 3.0 indicates issue", "[delta_e][redline]") {
    Lab lab1{50.0, 0.0, 0.0};
    Lab lab2{30.0, 40.0, 30.0};
    double de = delta_e_2000(lab1, lab2);
    REQUIRE(de > 3.0);
}

TEST_CASE("ΔE2000 symmetry", "[delta_e]") {
    Lab lab1{55.0, 10.0, 15.0};
    Lab lab2{60.0, -5.0, 20.0};
    REQUIRE(delta_e_2000(lab1, lab2) == Catch::Approx(delta_e_2000(lab2, lab1)).margin(0.001));
}

TEST_CASE("ΔE2000 triangle inequality approximately holds", "[delta_e]") {
    Lab lab1{50.0, 10.0, 20.0};
    Lab lab2{60.0, -5.0, 25.0};
    Lab lab3{55.0, 5.0, 15.0};

    double de12 = delta_e_2000(lab1, lab2);
    double de23 = delta_e_2000(lab2, lab3);
    double de13 = delta_e_2000(lab1, lab3);

    double max_sum = de12 + de23;
    REQUIRE(de13 <= max_sum);
}

TEST_CASE("interpolate_lab returns valid Lab", "[delta_e]") {
    auto spec = Illuminant::D50();
    auto lab = interpolate_lab(spec);
    REQUIRE(lab.L >= 0);
    REQUIRE(lab.L <= 150);
    REQUIRE(std::abs(lab.a) < 100);
    REQUIRE(std::abs(lab.b) < 100);
}

TEST_CASE("ΔE2000 vs ΔE94 relationship", "[delta_e]") {
    Lab lab1{50.0, 10.0, 20.0};
    Lab lab2{55.0, 15.0, 25.0};

    double de00 = delta_e_2000(lab1, lab2);
    double de94 = delta_e_94(lab1, lab2);

    REQUIRE(de00 > 0);
    REQUIRE(de94 > 0);
    REQUIRE(std::abs(de00 - de94) < 20);
}

TEST_CASE("max_delta_e_under: identical spectra = 0", "[delta_e][minimax]") {
    auto spec = rgb_to_spectrum({0.5, 0.3, 0.7});
    double de = max_delta_e_under_standard(spec, spec);
    REQUIRE(de == Catch::Approx(0.0).margin(0.001));
}

TEST_CASE("max_delta_e_under: metamers differ across illuminants", "[delta_e][minimax]") {
    // Two spectra that look similar under D50 but differ under other illuminants
    auto spec1 = rgb_to_spectrum({0.5, 0.5, 0.5});
    auto spec2 = rgb_to_spectrum({0.52, 0.48, 0.51});
    double de_d50 = delta_e_2000(
        xyz_to_lab(spectrum_to_xyz(spec1)),
        xyz_to_lab(spectrum_to_xyz(spec2)));
    double de_max = max_delta_e_under_standard(spec1, spec2);
    // max across illuminants >= ΔE under D50
    REQUIRE(de_max >= de_d50 - 0.001);
}

TEST_CASE("max_delta_e_under: clearly different colors", "[delta_e][minimax]") {
    auto spec1 = rgb_to_spectrum({0.9, 0.1, 0.1});
    auto spec2 = rgb_to_spectrum({0.1, 0.1, 0.9});
    double de_max = max_delta_e_under_standard(spec1, spec2);
    REQUIRE(de_max > 10.0);
}

TEST_CASE("max_delta_e_under: custom illuminant set", "[delta_e][minimax]") {
    auto spec1 = rgb_to_spectrum({0.6, 0.4, 0.3});
    auto spec2 = rgb_to_spectrum({0.62, 0.42, 0.32});
    std::vector<Spectrum> illums = {Illuminant::D50(), Illuminant::D65()};
    double de = max_delta_e_under(spec1, spec2, illums);
    REQUIRE(de >= 0.0);
    REQUIRE(de < 20.0);
}
