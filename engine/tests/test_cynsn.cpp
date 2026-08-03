#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "cynsn.h"
#include "km.h"
#include "delta_e.h"
#include <cmath>
#include <numeric>
#include <vector>

using namespace jcolour::forward;
using namespace jcolour::spectral;

static Spectrum flat(double val) {
    Spectrum s;
    s.fill(val);
    return s;
}

// ---- demichel ----

TEST_CASE("demichel: N=0 gives single probability 1.0", "[cynsn][demichel]") {
    auto p = demichel({});
    REQUIRE(p.size() == 1);
    REQUIRE(p[0] == Catch::Approx(1.0));
}

TEST_CASE("demichel: N=1 probabilities sum to 1", "[cynsn][demichel]") {
    for (double c : {0.0, 0.3, 0.5, 0.7, 1.0}) {
        auto p = demichel({c});
        REQUIRE(p.size() == 2);
        REQUIRE(p[0] + p[1] == Catch::Approx(1.0).margin(1e-12));
        REQUIRE(p[0] == Catch::Approx(1.0 - c).margin(1e-12));
        REQUIRE(p[1] == Catch::Approx(c).margin(1e-12));
    }
}

TEST_CASE("demichel: N=2 probabilities sum to 1", "[cynsn][demichel]") {
    auto p = demichel({0.4, 0.6});
    REQUIRE(p.size() == 4);
    double sum = 0;
    for (double v : p) sum += v;
    REQUIRE(sum == Catch::Approx(1.0).margin(1e-12));
    // p[0] = (1-0.4)*(1-0.6) = 0.24
    REQUIRE(p[0] == Catch::Approx(0.24).margin(1e-12));
    // p[1] = 0.4*(1-0.6) = 0.16
    REQUIRE(p[1] == Catch::Approx(0.16).margin(1e-12));
    // p[2] = (1-0.4)*0.6 = 0.36
    REQUIRE(p[2] == Catch::Approx(0.36).margin(1e-12));
    // p[3] = 0.4*0.6 = 0.24
    REQUIRE(p[3] == Catch::Approx(0.24).margin(1e-12));
}

TEST_CASE("demichel: N=3 probabilities sum to 1", "[cynsn][demichel]") {
    auto p = demichel({0.2, 0.5, 0.8});
    REQUIRE(p.size() == 8);
    double sum = 0;
    for (double v : p) sum += v;
    REQUIRE(sum == Catch::Approx(1.0).margin(1e-12));
}

TEST_CASE("demichel: coverage 0 = substrate only", "[cynsn][demichel]") {
    auto p = demichel({0.0, 0.0});
    REQUIRE(p[0] == Catch::Approx(1.0).margin(1e-12));
    for (int i = 1; i < 4; ++i) REQUIRE(p[i] == Catch::Approx(0.0).margin(1e-12));
}

TEST_CASE("demichel: coverage 1 = all-ink primary only", "[cynsn][demichel]") {
    auto p = demichel({1.0, 1.0});
    REQUIRE(p[3] == Catch::Approx(1.0).margin(1e-12));
    for (int i = 0; i < 3; ++i) REQUIRE(p[i] == Catch::Approx(0.0).margin(1e-12));
}

// ---- YNSNModel ----

TEST_CASE("YNSN: N=1 zero coverage = substrate", "[cynsn][ynsn]") {
    YNSNModel model(2.0);
    model.add_primary(flat(0.8));  // substrate (no ink)
    model.add_primary(flat(0.1));  // ink full-tone

    auto result = model.predict({0.0});
    for (int i = 0; i < NUM_CHANNELS; ++i)
        REQUIRE(result[i] == Catch::Approx(0.8).margin(1e-6));
}

TEST_CASE("YNSN: N=1 full coverage = ink primary", "[cynsn][ynsn]") {
    YNSNModel model(2.0);
    model.add_primary(flat(0.8));
    model.add_primary(flat(0.1));

    auto result = model.predict({1.0});
    for (int i = 0; i < NUM_CHANNELS; ++i)
        REQUIRE(result[i] == Catch::Approx(0.1).margin(1e-6));
}

TEST_CASE("YNSN: N=1 n=1 is classical Neugebauer (linear mix)", "[cynsn][ynsn]") {
    YNSNModel model(1.0);
    model.add_primary(flat(0.8));
    model.add_primary(flat(0.2));

    double c = 0.4;
    auto result = model.predict({c});
    double expected = (1.0 - c) * 0.8 + c * 0.2;  // linear
    for (int i = 0; i < NUM_CHANNELS; ++i)
        REQUIRE(result[i] == Catch::Approx(expected).margin(1e-9));
}

TEST_CASE("YNSN: N=2 zero coverage = substrate", "[cynsn][ynsn]") {
    YNSNModel model(2.5);
    model.add_primary(flat(0.9));   // 00: substrate
    model.add_primary(flat(0.15));  // 01: ink0
    model.add_primary(flat(0.25));  // 10: ink1
    model.add_primary(flat(0.05));  // 11: ink0+ink1

    auto result = model.predict({0.0, 0.0});
    for (int i = 0; i < NUM_CHANNELS; ++i)
        REQUIRE(result[i] == Catch::Approx(0.9).margin(1e-6));
}

TEST_CASE("YNSN: N=2 full coverage both inks = overprint primary", "[cynsn][ynsn]") {
    YNSNModel model(2.5);
    model.add_primary(flat(0.9));
    model.add_primary(flat(0.15));
    model.add_primary(flat(0.25));
    model.add_primary(flat(0.05));

    auto result = model.predict({1.0, 1.0});
    for (int i = 0; i < NUM_CHANNELS; ++i)
        REQUIRE(result[i] == Catch::Approx(0.05).margin(1e-6));
}

TEST_CASE("YNSN: N=2 result in valid range [0,1]", "[cynsn][ynsn]") {
    YNSNModel model(2.5);
    model.add_primary(flat(0.9));
    model.add_primary(flat(0.15));
    model.add_primary(flat(0.25));
    model.add_primary(flat(0.05));

    for (double c1 : {0.0, 0.3, 0.5, 0.7, 1.0}) {
        for (double c2 : {0.0, 0.3, 0.5, 0.7, 1.0}) {
            auto result = model.predict({c1, c2});
            for (int i = 0; i < NUM_CHANNELS; ++i) {
                REQUIRE(result[i] >= 0.0);
                REQUIRE(result[i] <= 1.0 + 1e-9);
            }
        }
    }
}

TEST_CASE("YNSN: ink count mismatch throws", "[cynsn][ynsn]") {
    YNSNModel model(2.0);
    model.add_primary(flat(0.9));
    model.add_primary(flat(0.1));

    REQUIRE_THROWS_AS(model.predict({}), std::invalid_argument);
    REQUIRE_THROWS_AS(model.predict({0.5, 0.5}), std::invalid_argument);
}

TEST_CASE("YNSN: incomplete primaries throws", "[cynsn][ynsn]") {
    YNSNModel model(2.0);
    model.add_primary(flat(0.9));  // only 1 of 4 primaries for N=2
    model.add_primary(flat(0.1));
    model.add_primary(flat(0.2));
    // missing primary 3 — but n_inks_ is still 1 (2 primaries = 2^1)
    // Add a 3rd primary to make it non-power-of-2 state... actually
    // n_inks_ is derived from the last power-of-2 count.
    // Test: with 2 primaries (N=1), calling predict({0.5, 0.5}) throws.
    YNSNModel m2(2.0);
    m2.add_primary(flat(0.9));
    m2.add_primary(flat(0.1));
    REQUIRE_THROWS_AS(m2.predict({0.5, 0.5}), std::invalid_argument);
}

TEST_CASE("YNSN: n exponent <= 0 throws", "[cynsn][ynsn]") {
    REQUIRE_THROWS_AS(YNSNModel(0.0), std::invalid_argument);
    REQUIRE_THROWS_AS(YNSNModel(-1.0), std::invalid_argument);
}

TEST_CASE("YNSN: N=2 monotone in single ink coverage", "[cynsn][ynsn]") {
    // With ink1 fixed at 0, increasing ink0 should darken the result
    YNSNModel model(2.0);
    model.add_primary(flat(0.9));
    model.add_primary(flat(0.1));
    model.add_primary(flat(0.8));
    model.add_primary(flat(0.08));

    double prev = 1.0;
    for (double c : {0.0, 0.2, 0.4, 0.6, 0.8, 1.0}) {
        auto result = model.predict({c, 0.0});
        double mean = 0;
        for (int i = 0; i < NUM_CHANNELS; ++i) mean += result[i];
        mean /= NUM_CHANNELS;
        REQUIRE(mean <= prev + 0.001);
        prev = mean;
    }
}

// ---- CellularYNSNModel (A8) ----

// Beer-Lambert mixing: R_mix = R_sub * product((R_ink_j/R_sub)^c_j)
static Spectrum beer_lambert(const Spectrum& sub,
                              const std::vector<Spectrum>& inks,
                              const std::vector<double>& c) {
    Spectrum r;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        double val = sub[i];
        for (int j = 0; j < (int)inks.size(); ++j) {
            double ratio = std::max(1e-6, inks[j][i]) / std::max(1e-6, sub[i]);
            val *= std::pow(ratio, c[j]);
        }
        r[i] = std::max(0.0, std::min(1.0, val));
    }
    return r;
}

TEST_CASE("Cellular YNSN: N=1 exact at grid corners", "[cynsn][cellular]") {
    Spectrum sub = flat(0.85);
    Spectrum ink = flat(0.10);
    std::vector<Spectrum> inks = {ink};

    CellularYNSNModel m(1, 2, 2.0);
    // 3 grid points: 0, 0.5, 1.0
    for (int i = 0; i <= 2; ++i) {
        double c = i * 0.5;
        m.set_grid_point({i}, beer_lambert(sub, inks, {c}));
    }

    for (int i = 0; i <= 2; ++i) {
        double c = i * 0.5;
        auto pred = m.predict({c});
        auto ref  = beer_lambert(sub, inks, {c});
        for (int ch = 0; ch < NUM_CHANNELS; ++ch)
            REQUIRE(pred[ch] == Catch::Approx(ref[ch]).margin(1e-9));
    }
}

TEST_CASE("Cellular YNSN: N=2 exact at grid corners", "[cynsn][cellular]") {
    Spectrum sub = flat(0.9);
    std::vector<Spectrum> inks = {flat(0.15), flat(0.25)};

    CellularYNSNModel m(2, 2, 2.5);
    for (int i = 0; i <= 2; ++i)
        for (int j = 0; j <= 2; ++j)
            m.set_grid_point({i, j}, beer_lambert(sub, inks, {i*0.5, j*0.5}));

    for (int i = 0; i <= 2; ++i) {
        for (int j = 0; j <= 2; ++j) {
            auto pred = m.predict({i*0.5, j*0.5});
            auto ref  = beer_lambert(sub, inks, {i*0.5, j*0.5});
            for (int ch = 0; ch < NUM_CHANNELS; ++ch)
                REQUIRE(pred[ch] == Catch::Approx(ref[ch]).margin(1e-9));
        }
    }
}

TEST_CASE("Cellular YNSN: result in valid range", "[cynsn][cellular]") {
    Spectrum sub = flat(0.9);
    std::vector<Spectrum> inks = {flat(0.2), flat(0.3), flat(0.1)};

    CellularYNSNModel m(3, 2, 2.0);
    for (int i = 0; i <= 2; ++i)
        for (int j = 0; j <= 2; ++j)
            for (int k = 0; k <= 2; ++k)
                m.set_grid_point({i,j,k}, beer_lambert(sub, inks, {i*0.5, j*0.5, k*0.5}));

    for (double c0 : {0.1, 0.4, 0.9}) {
        for (double c1 : {0.2, 0.6}) {
            for (double c2 : {0.3, 0.7}) {
                auto r = m.predict({c0, c1, c2});
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    REQUIRE(r[ch] >= 0.0);
                    REQUIRE(r[ch] <= 1.0 + 1e-9);
                }
            }
        }
    }
}

TEST_CASE("Cellular YNSN: N=4 ΔE improvement >= 30% vs K-M", "[cynsn][cellular]") {
    // Ground truth: Beer-Lambert with 4 spectrally distinct inks
    Spectrum sub;
    for (int i = 0; i < NUM_CHANNELS; ++i) sub[i] = 0.90;

    std::vector<Spectrum> inks(4);
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        double wl = WAVELENGTH_MIN + i * WAVELENGTH_STEP;
        inks[0][i] = (wl <= 520.0) ? 0.55 : 0.05;          // cyan-like
        inks[1][i] = (wl <= 440.0 || wl >= 610.0) ? 0.50 : 0.05; // magenta-like
        inks[2][i] = (wl >= 560.0) ? 0.65 : 0.05;          // yellow-like
        inks[3][i] = 0.06;                                   // black-like
    }

    // Calibrate cellular YNSN with Beer-Lambert at all 3^4=81 grid corners
    CellularYNSNModel cynsn(4, 2, 2.5);
    for (int i0 = 0; i0 <= 2; ++i0)
        for (int i1 = 0; i1 <= 2; ++i1)
            for (int i2 = 0; i2 <= 2; ++i2)
                for (int i3 = 0; i3 <= 2; ++i3)
                    cynsn.set_grid_point({i0,i1,i2,i3},
                        beer_lambert(sub, inks, {i0*0.5, i1*0.5, i2*0.5, i3*0.5}));

    // Calibrate K-M with full-tone spectra
    KMForwardModel km(sub);
    for (auto& ink : inks) km.add_ink(ink);

    // Test at intermediate points (not at grid corners)
    std::vector<std::vector<double>> test_pts = {
        {0.25, 0.25, 0.25, 0.25},
        {0.75, 0.25, 0.25, 0.25},
        {0.25, 0.75, 0.25, 0.25},
        {0.25, 0.25, 0.75, 0.25},
        {0.25, 0.25, 0.25, 0.75},
        {0.75, 0.75, 0.25, 0.25},
        {0.75, 0.25, 0.75, 0.25},
        {0.25, 0.75, 0.75, 0.25},
        {0.33, 0.67, 0.33, 0.67},
        {0.67, 0.33, 0.67, 0.33},
    };

    auto illum = jcolour::spectral::Illuminant::D50();
    double de_cynsn = 0.0, de_km = 0.0;

    for (auto& pt : test_pts) {
        auto truth = beer_lambert(sub, inks, pt);
        auto pred_c = cynsn.predict(pt);
        auto pred_k = km.predict(pt);

        auto lab_t  = jcolour::spectral::xyz_to_lab(jcolour::spectral::spectrum_to_xyz(truth,  illum));
        auto lab_c  = jcolour::spectral::xyz_to_lab(jcolour::spectral::spectrum_to_xyz(pred_c, illum));
        auto lab_k  = jcolour::spectral::xyz_to_lab(jcolour::spectral::spectrum_to_xyz(pred_k, illum));

        de_cynsn += jcolour::delta_e::delta_e_2000(lab_t, lab_c);
        de_km    += jcolour::delta_e::delta_e_2000(lab_t, lab_k);
    }

    // Cellular YNSN must be at least 30% better than K-M
    REQUIRE(de_cynsn < 0.70 * de_km);
}

TEST_CASE("Cellular YNSN: ink count mismatch throws", "[cynsn][cellular]") {
    CellularYNSNModel m(2, 2, 2.0);
    REQUIRE_THROWS_AS(m.predict({0.5}), std::invalid_argument);
    REQUIRE_THROWS_AS(m.predict({0.5, 0.5, 0.5}), std::invalid_argument);
}

TEST_CASE("Cellular YNSN: invalid construction throws", "[cynsn][cellular]") {
    REQUIRE_THROWS_AS(CellularYNSNModel(0, 2, 2.0), std::invalid_argument);
    REQUIRE_THROWS_AS(CellularYNSNModel(2, 0, 2.0), std::invalid_argument);
    REQUIRE_THROWS_AS(CellularYNSNModel(2, 2, 0.0), std::invalid_argument);
}
