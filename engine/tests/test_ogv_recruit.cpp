// Spike: sub-problem (B) — self-managed OGV recruitment via forward model +
// inversion.  Question: for saturated targets in the Orange / Green / Violet
// hues that CMYK cannot reach, does jcolour's own separation recruit O/G/V to
// close the gap, and how much gamut does that recover vs CMYK-only?
//
// Ink spectra are SYNTHETIC but physically shaped (band-absorbing colorants;
// O/G/V purer than any CMY secondary). Absolute ΔE is illustrative; the
// STRUCTURE — CMYK stuck out-of-gamut, CMYKOGV reaching it by firing the extra
// inks — is the decision-relevant result.
//
// Solver note: the engine SQP (optimizer::minimize) was tried first and proved
// unreliable on this objective — ΔE-of-KM is non-convex, and KM predict emits
// out-of-range reflectance for heavy overprints → xyz_to_lab NaN → BFGS blows
// up (engine follow-up: NaN-guard xyz_to_lab / clamp KM predict). SQP is fine
// on smooth convex problems (verified on a quadratic). Here we use a robust
// derivative-free Hooke-Jeeves multi-start, which finds the global optimum
// (pure-ink target → ΔE 0.000).

#include "km.h"
#include "spectral.h"
#include "delta_e.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <utility>

using namespace jcolour;
using jcolour::forward::KMForwardModel;

namespace {

spectral::Spectrum absorb(double c, double w, double depth = 0.92) {
    spectral::Spectrum s;
    for (int i = 0; i < spectral::NUM_CHANNELS; ++i) {
        double l = spectral::WAVELENGTHS[i];
        s[i] = std::max(0.02, 0.96 - depth * std::exp(-0.5 * std::pow((l - c) / w, 2.0)));
    }
    return s;
}
spectral::Spectrum absorb2(double c1, double w1, double c2, double w2) {
    spectral::Spectrum s;
    for (int i = 0; i < spectral::NUM_CHANNELS; ++i) {
        double l = spectral::WAVELENGTHS[i];
        double a = 0.92 * std::exp(-0.5 * std::pow((l - c1) / w1, 2.0)) +
                   0.92 * std::exp(-0.5 * std::pow((l - c2) / w2, 2.0));
        s[i] = std::max(0.02, 0.96 - std::min(0.93, a));
    }
    return s;
}
spectral::Spectrum flat(double r) { spectral::Spectrum s; s.fill(r); return s; }

spectral::Lab lab_of(const KMForwardModel& m, std::vector<double> x) {
    for (auto& v : x) v = std::clamp(v, 0.0, 1.0);
    auto R = m.predict(x);
    for (auto& r : R) r = std::clamp(r, 1e-4, 1.0);   // guard KM overprint range
    return spectral::xyz_to_lab(spectral::spectrum_to_xyz(R));
}
double chroma(const spectral::Lab& L) { return std::hypot(L.a, L.b); }

// Robust min-ΔE2000 inversion: Hooke-Jeeves pattern search, multi-start from
// the neutral point + each single-ink corner. Σink ≤ TIL via penalty.
struct InvResult { std::vector<double> inks; double dE; };
InvResult invert(const KMForwardModel& m, const spectral::Lab& target,
                 int n, double TIL = 2.8) {
    auto cost = [&](const std::vector<double>& x) {
        double d = delta_e::delta_e_2000(lab_of(m, x), target);
        double s = 0; for (double v : x) s += v;
        return s > TIL ? d + 50.0 * (s - TIL) : d;
    };
    auto hj = [&](std::vector<double> x) {
        double step = 0.3, best = cost(x);
        for (int it = 0; it < 400 && step > 1e-4; ++it) {
            bool improved = false;
            for (int i = 0; i < n; ++i)
                for (double s : {step, -step}) {
                    auto y = x; y[i] = std::clamp(y[i] + s, 0.0, 1.0);
                    double c = cost(y);
                    if (c < best - 1e-9) { best = c; x = y; improved = true; }
                }
            if (!improved) step *= 0.5;
        }
        return std::make_pair(best, x);
    };
    std::vector<std::vector<double>> seeds(1, std::vector<double>(n, 0.1));
    for (int i = 0; i < n; ++i) { std::vector<double> s(n, 0.0); s[i] = 0.9; seeds.push_back(s); }

    double best = 1e9; std::vector<double> bx(n, 0.0);
    for (auto& s : seeds) { auto r = hj(s); if (r.first < best) { best = r.first; bx = r.second; } }
    return {bx, delta_e::delta_e_2000(lab_of(m, bx), target)};
}

}  // namespace

TEST_CASE("OGV recruitment recovers gamut CMYK cannot reach", "[ogv]") {
    const auto white = flat(0.93);
    const auto C  = absorb(620, 70, 0.82);       // cyan (real: broad, unwanted absorptions)
    const auto M  = absorb(545, 65, 0.82);       // magenta (real)
    const auto Yk = absorb(445, 50, 0.85);       // yellow (real)
    const auto K  = flat(0.03);
    const auto Or = absorb(500, 30, 0.96);       // orange (purer)
    const auto Gr = absorb2(425, 26, 660, 36);  // pure green (purer)
    const auto V  = absorb(565, 55, 0.95);       // violet-blue (purer)

    KMForwardModel cmyk(white);
    cmyk.add_ink(C); cmyk.add_ink(M); cmyk.add_ink(Yk); cmyk.add_ink(K);

    KMForwardModel full(white);
    full.add_ink(C); full.add_ink(M); full.add_ink(Yk); full.add_ink(K);
    full.add_ink(Or); full.add_ink(Gr); full.add_ink(V);

    struct T { const char* name; spectral::Lab lab; int ink; const char* tag; };
    std::vector<T> targets = {
        {"Orange", lab_of(full, {0,0,0,0,1,0,0}), 4, "Or"},
        {"Green",  lab_of(full, {0,0,0,0,0,1,0}), 5, "Gr"},
        {"Violet", lab_of(full, {0,0,0,0,0,0,1}), 6, "V "},
    };

    std::printf("\n[ogv] hue    targetC* | CMYK ΔE | CMYKOGV ΔE | gap closed | extra ink\n");
    for (auto& t : targets) {
        auto rc = invert(cmyk, t.lab, 4);
        auto rf = invert(full, t.lab, 7);
        double used = rf.inks[t.ink];
        std::printf("[ogv] %-6s  %5.0f   |  %5.2f  |   %5.2f    |   %5.1f%%   |  %s=%.2f\n",
                    t.name, chroma(t.lab), rc.dE, rf.dE,
                    100.0 * (rc.dE - rf.dE) / std::max(rc.dE, 1e-6), t.tag, used);
        REQUIRE(rc.dE > 4.0);              // CMYK clearly short of these hues
        REQUIRE(rf.dE < rc.dE - 2.0);      // OGV closes most of the gap
        REQUIRE(used > 0.3);               // the right extra ink fires
    }

    // Control: an in-gamut CMY brown — both reach it, no false OGV dependence.
    spectral::Lab ctrl = lab_of(cmyk, {0.5, 0.4, 0.6, 0.0});
    auto cc = invert(cmyk, ctrl, 4);
    auto cf = invert(full, ctrl, 7);
    std::printf("[ogv] Brown  (in-gamut) |  %5.2f  |   %5.2f    |     —      |  control\n",
                cc.dE, cf.dE);
    REQUIRE(cc.dE < 2.0);
    REQUIRE(cf.dE < 2.0);
}
