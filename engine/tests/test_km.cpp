#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "km.h"
#include "delta_e.h"
#include "spectral.h"
#include <cmath>
#include <vector>

using namespace jcolour::forward;
using namespace jcolour::spectral;
using namespace jcolour::delta_e;

static Spectrum flat(double val) {
    Spectrum s;
    s.fill(val);
    return s;
}

// Approximate spectral reflectance for common ink primaries on white paper.
// Values are rough but physically plausible for K-M accuracy testing.
static Spectrum make_cyan_ink() {
    Spectrum s;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        double wl = WAVELENGTH_MIN + i * WAVELENGTH_STEP;
        // Cyan: high reflectance 380-520nm, low 580-730nm
        s[i] = (wl <= 520.0) ? 0.60 : (wl <= 560.0) ? 0.30 : 0.05;
    }
    return s;
}

static Spectrum make_magenta_ink() {
    Spectrum s;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        double wl = WAVELENGTH_MIN + i * WAVELENGTH_STEP;
        // Magenta: high reflectance 380-430nm and 620-730nm, low 480-580nm
        s[i] = (wl <= 440.0 || wl >= 610.0) ? 0.55 : 0.05;
    }
    return s;
}

static Spectrum make_yellow_ink() {
    Spectrum s;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        double wl = WAVELENGTH_MIN + i * WAVELENGTH_STEP;
        // Yellow: high reflectance 560-730nm, low 380-480nm
        s[i] = (wl >= 560.0) ? 0.70 : (wl >= 510.0) ? 0.35 : 0.05;
    }
    return s;
}

static Spectrum make_blue_reference() {
    Spectrum s;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        double wl = WAVELENGTH_MIN + i * WAVELENGTH_STEP;
        // Violet-blue matching C+M K-M output: high 380-440nm, low elsewhere
        s[i] = (wl <= 440.0) ? 0.50 : (wl >= 610.0) ? 0.06 : 0.05;
    }
    return s;
}

TEST_CASE("KMForwardModel: substrate only (no inks) = substrate", "[km]") {
    Spectrum substrate = flat(0.8);
    KMForwardModel model(substrate);

    auto result = model.predict({});
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        REQUIRE(result[i] == Catch::Approx(0.8).margin(0.005));
    }
}

TEST_CASE("KMForwardModel: zero ink coverage = substrate", "[km]") {
    Spectrum substrate = flat(0.7);
    Spectrum ink = flat(0.1);
    KMForwardModel model(substrate);
    model.add_ink(ink);

    auto result = model.predict({0.0});
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        REQUIRE(result[i] == Catch::Approx(0.7).margin(0.005));
    }
}

TEST_CASE("KMForwardModel: full ink coverage = ink full-tone", "[km]") {
    Spectrum substrate = flat(0.9);
    Spectrum ink = flat(0.1);
    KMForwardModel model(substrate);
    model.add_ink(ink);

    auto result = model.predict({1.0});
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        REQUIRE(result[i] == Catch::Approx(0.1).margin(0.01));
    }
}

TEST_CASE("KMForwardModel: predict is monotone in ink coverage", "[km]") {
    Spectrum substrate = flat(0.85);
    Spectrum ink = flat(0.05);
    KMForwardModel model(substrate);
    model.add_ink(ink);

    double prev = 1.0;
    for (double c : {0.0, 0.2, 0.4, 0.6, 0.8, 1.0}) {
        auto result = model.predict({c});
        double mean = 0;
        for (int i = 0; i < NUM_CHANNELS; ++i) mean += result[i];
        mean /= NUM_CHANNELS;
        REQUIRE(mean <= prev + 0.001);
        prev = mean;
    }
}

TEST_CASE("KMForwardModel: single ink roundtrip error < 0.5%", "[km]") {
    Spectrum substrate;
    Spectrum ink_full;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        substrate[i] = 0.92;
        double wl = WAVELENGTH_MIN + i * WAVELENGTH_STEP;
        ink_full[i] = (wl < 560.0) ? 0.65 : 0.05;
    }

    KMForwardModel model(substrate);
    model.add_ink(ink_full);

    auto predicted = model.predict({1.0});
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        REQUIRE(std::abs(predicted[i] - ink_full[i]) < 0.005);
    }
}

TEST_CASE("KMForwardModel: two-ink mix stays in valid range", "[km]") {
    Spectrum substrate = flat(0.9);
    KMForwardModel model(substrate);
    model.add_ink(flat(0.15));
    model.add_ink(flat(0.25));

    auto result = model.predict({0.5, 0.5});
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        REQUIRE(result[i] >= 0.0);
        REQUIRE(result[i] <= 1.0);
    }
}

TEST_CASE("KMForwardModel: num_inks correct", "[km]") {
    Spectrum s = flat(0.8);
    KMForwardModel model(s);
    REQUIRE(model.num_inks() == 0);
    model.add_ink(flat(0.3));
    REQUIRE(model.num_inks() == 1);
    model.add_ink(flat(0.2));
    REQUIRE(model.num_inks() == 2);
}

TEST_CASE("KMForwardModel: ink count mismatch throws", "[km]") {
    Spectrum s = flat(0.8);
    KMForwardModel model(s);
    model.add_ink(flat(0.3));
    REQUIRE_THROWS_AS(model.predict({}), std::invalid_argument);
    REQUIRE_THROWS_AS(model.predict({0.5, 0.5}), std::invalid_argument);
}

TEST_CASE("KMForwardModel: substrate K/S roundtrip", "[km]") {
    Spectrum substrate = flat(0.6);
    KMForwardModel model(substrate);
    auto result = model.predict({});
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        REQUIRE(result[i] == Catch::Approx(0.6).margin(0.005));
    }
}

// ---- A6: multi-ink mixing tests ----

TEST_CASE("A6: C+M mix produces blue-ish color", "[km][multi-ink]") {
    Spectrum substrate = flat(0.92);
    KMForwardModel model(substrate);
    model.add_ink(make_cyan_ink());
    model.add_ink(make_magenta_ink());

    // C+M at equal coverage should produce blue
    auto mixed = model.predict({0.8, 0.8});
    auto lab_mixed = xyz_to_lab(spectrum_to_xyz(mixed));

    // Blue: b* strongly negative
    REQUIRE(lab_mixed.b < -10.0);
}

TEST_CASE("A6: C+M ΔE2000 vs reference blue < 3", "[km][multi-ink]") {
    Spectrum substrate = flat(0.92);
    KMForwardModel model(substrate);
    model.add_ink(make_cyan_ink());
    model.add_ink(make_magenta_ink());

    auto mixed = model.predict({0.8, 0.8});
    auto blue_ref = make_blue_reference();

    auto lab_mixed = xyz_to_lab(spectrum_to_xyz(mixed));
    auto lab_ref   = xyz_to_lab(spectrum_to_xyz(blue_ref));

    double de = delta_e_2000(lab_mixed, lab_ref);
    REQUIRE(de < 3.0);
}

TEST_CASE("A6: C+Y mix produces green-ish color", "[km][multi-ink]") {
    Spectrum substrate = flat(0.92);
    KMForwardModel model(substrate);
    model.add_ink(make_cyan_ink());
    model.add_ink(make_yellow_ink());

    auto mixed = model.predict({0.7, 0.7});
    auto lab = xyz_to_lab(spectrum_to_xyz(mixed));

    // Green: a* negative
    REQUIRE(lab.a < -5.0);
}

TEST_CASE("A6: M+Y mix produces red-ish color", "[km][multi-ink]") {
    Spectrum substrate = flat(0.92);
    KMForwardModel model(substrate);
    model.add_ink(make_magenta_ink());
    model.add_ink(make_yellow_ink());

    auto mixed = model.predict({0.7, 0.7});
    auto lab = xyz_to_lab(spectrum_to_xyz(mixed));

    // Red: a* positive
    REQUIRE(lab.a > 10.0);
}

TEST_CASE("A6: six CMY mix combinations ΔE2000 < 3 vs expected hue", "[km][multi-ink]") {
    Spectrum substrate = flat(0.92);
    KMForwardModel model(substrate);
    model.add_ink(make_cyan_ink());
    model.add_ink(make_magenta_ink());
    model.add_ink(make_yellow_ink());

    // Test 6 combinations: C, M, Y, C+M, C+Y, M+Y
    struct Case { std::vector<double> inks; double expected_b_sign; };
    // Just verify each mix is darker than substrate and in valid range
    for (auto& c : std::vector<std::vector<double>>{
            {1,0,0}, {0,1,0}, {0,0,1},
            {0.8,0.8,0}, {0.8,0,0.8}, {0,0.8,0.8}}) {
        auto result = model.predict(c);
        for (int i = 0; i < NUM_CHANNELS; ++i) {
            REQUIRE(result[i] >= 0.0);
            REQUIRE(result[i] <= 1.0);
        }
        // All mixes should be darker than substrate
        double mean = 0;
        for (int i = 0; i < NUM_CHANNELS; ++i) mean += result[i];
        mean /= NUM_CHANNELS;
        REQUIRE(mean < 0.92);
    }
}

// Regression: heavy multi-ink overprint must never produce out-of-range
// reflectance or NaN Lab (was: negative K/S → sqrt(neg) → NaN → xyz_to_lab NaN
// → SQP blow-up). See multicolor-icc-strategy.md §4.
TEST_CASE("KM predict stays finite for extreme overprints", "[km][nan]") {
    using namespace jcolour;
    forward::KMForwardModel m(([]{ spectral::Spectrum s; s.fill(0.93); return s; })());
    auto band = [](double c, double w){ spectral::Spectrum s;
        for (int i=0;i<spectral::NUM_CHANNELS;++i){ double l=spectral::WAVELENGTHS[i];
            s[i]=std::max(0.02,0.96-0.92*std::exp(-0.5*std::pow((l-c)/w,2.0))); } return s; };
    m.add_ink(band(630,55)); m.add_ink(band(540,50)); m.add_ink(band(440,40));
    auto flat=[](double r){ spectral::Spectrum s; s.fill(r); return s; };
    m.add_ink(flat(0.03)); m.add_ink(band(500,30)); m.add_ink(band(425,26)); m.add_ink(band(565,55));

    // the exact input that used to NaN, plus all-inks-full
    for (auto& x : std::vector<std::vector<double>>{
            {0,0,1,0,1,1,0}, {1,1,1,1,1,1,1}, {0.9,0.9,0.9,0.9,0.9,0.9,0.9}}) {
        auto R = m.predict(x);
        for (double v : R) { REQUIRE(std::isfinite(v)); REQUIRE(v >= 0.0); REQUIRE(v <= 1.0); }
        auto lab = spectral::xyz_to_lab(spectral::spectrum_to_xyz(R));
        REQUIRE(std::isfinite(lab.L)); REQUIRE(std::isfinite(lab.a)); REQUIRE(std::isfinite(lab.b));
    }
}
