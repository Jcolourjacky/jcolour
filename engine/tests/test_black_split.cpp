// Spike: how good is jcolour's *self-managed* same-hue ink split (sub-problem
// A)?  QTR-style 3-black partition (K/GY/LGY) on a neutral ramp, predicted
// through the engine's KM forward model, measured for the properties that
// actually matter: monotonic L*, smooth crossovers, step uniformity, TIL.
//
// Ink spectra here are SYNTHETIC neutral greys (no instrument yet) — so
// absolute L* is illustrative, but monotonicity / smoothness / step spread
// are structural results of the split and are meaningful.

#include "black_split.h"
#include "km.h"
#include "spectral.h"
#include "delta_e.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cstdio>
#include <cmath>
#include <vector>

using namespace jcolour;
using namespace jcolour::calibration;
using jcolour::forward::KMForwardModel;
using Catch::Approx;

namespace {
// Flat (neutral) reflectance spectrum at level r.
spectral::Spectrum flat(double r) {
    spectral::Spectrum s;
    s.fill(r);
    return s;
}
// KM model with the 3 PRO12 neutral inks (synthetic full-tone reflectances).
KMForwardModel make_neutral_model() {
    KMForwardModel m(flat(0.94));   // paper white
    m.add_ink(flat(0.030));         // [0] MAIN_K  (very dark)
    m.add_ink(flat(0.330));         // [1] GY      (mid grey)
    m.add_ink(flat(0.620));         // [2] LGY     (light grey)
    return m;
}
spectral::Lab predict_lab(const KMForwardModel& m, const InkSplit& s) {
    auto R = m.predict({s.k, s.gy, s.lgy});
    return spectral::xyz_to_lab(spectral::spectrum_to_xyz(R));
}
}  // namespace

TEST_CASE("3-black partition: correct ink ownership by tone", "[blacksplit]") {
    // Peak-tone ordering is the robust ownership property (crossover centres
    // are tunable; exact level at a fixed tone is not).
    auto peak_tone = [](double InkSplit::* f) {
        double best = -1, bt = 0;
        for (int i = 0; i <= 100; ++i) {
            double t = i / 100.0, v = split_neutral(t).*f;
            if (v > best) { best = v; bt = t; }
        }
        return bt;
    };
    const double lgyP = peak_tone(&InkSplit::lgy);
    const double gyP  = peak_tone(&InkSplit::gy);
    const double kP   = peak_tone(&InkSplit::k);

    REQUIRE(lgyP < gyP);              // Light Gray peaks before Gray
    REQUIRE(gyP  < kP);              // Gray peaks before main black
    REQUIRE(lgyP < 0.45);            // LGY owns highlights
    REQUIRE(kP   > 0.95);            // main black owns deep shadow

    REQUIRE(split_neutral(0.12).k   == Approx(0.0));   // no full black in highlights
    REQUIRE(split_neutral(0.90).lgy < 0.10);           // Light Gray gone by deep shadow
}

TEST_CASE("neutral build is monotonic, smooth, and within TIL", "[blacksplit]") {
    const int N = 21;
    auto ramp = neutral_ramp(N);
    auto model = make_neutral_model();

    std::vector<double> L;
    double max_total = 0;
    for (auto& s : ramp) {
        L.push_back(predict_lab(model, s).L);
        max_total = std::max(max_total, s.total());
    }

    // 1) L* strictly decreasing white→black (no reversals / no bumps)
    for (int i = 1; i < N; ++i) REQUIRE(L[i] < L[i - 1]);

    // 2) total ink within a sane neutral TIL (3 inks, ≤ ~2.2 here)
    REQUIRE(max_total <= 2.2);

    // 3) crossover smoothness: each single-ink curve has at most one local max
    auto humps = [&](double InkSplit::* f) {
        int dirChanges = 0; double prev = ramp[0].*f; int sign = 0;
        for (int i = 1; i < N; ++i) {
            double d = ramp[i].*f - prev; prev = ramp[i].*f;
            int sg = (d > 1e-6) - (d < -1e-6);
            if (sg && sg != sign && sign) ++dirChanges;
            if (sg) sign = sg;
        }
        return dirChanges;           // 0 = monotonic, 1 = single hump
    };
    REQUIRE(humps(&InkSplit::lgy) <= 1);
    REQUIRE(humps(&InkSplit::gy)  <= 1);
    REQUIRE(humps(&InkSplit::k)   <= 1);

    // 4) step uniformity: ΔE2000 between consecutive steps — report spread
    std::vector<double> dE;
    auto lab = [&](int i){ return predict_lab(model, ramp[i]); };
    double sum = 0, mx = 0;
    for (int i = 1; i < N; ++i) {
        double d = delta_e::delta_e_2000(lab(i - 1), lab(i));
        dE.push_back(d); sum += d; mx = std::max(mx, d);
    }
    double mean = sum / dE.size();
    double var = 0; for (double d : dE) var += (d - mean) * (d - mean);
    double cv = std::sqrt(var / dE.size()) / mean;   // coefficient of variation

    std::printf("\n[blacksplit] %d-step neutral ramp via KM forward model\n", N);
    std::printf("  L* range   : %.1f → %.1f\n", L.front(), L.back());
    std::printf("  max totalink: %.2f (3 neutral inks)\n", max_total);
    std::printf("  step dE2000 : mean %.2f  max %.2f  CV %.0f%% (pre-linearization)\n",
                mean, mx, cv * 100.0);
    std::printf("  tone  | LGY   GY    K    | L*\n");
    for (int i = 0; i < N; ++i)
        std::printf("  %.2f  | %.2f  %.2f  %.2f | %.1f\n",
                    double(i)/(N-1), ramp[i].lgy, ramp[i].gy, ramp[i].k, L[i]);

    // pre-linearization the spread is expected; just assert it's bounded.
    REQUIRE(cv < 1.5);
}
