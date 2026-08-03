#include "ogv_meter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace jcolour {
namespace gamut {

namespace {

constexpr double kPi = 3.14159265358979323846;

// One extended-ink hue band: centre & half-width (deg), the CMYK chroma ceiling
// in that band, and the colorant's own (near-)max chroma for normalisation.
struct Band {
    double hue_c, hue_hw;    // hue band (Lab degrees)
    double cmyk_ceiling;     // C* CMYK can reach here (it's weak in these hues)
    double ink_max;          // C* the dedicated O/G/V ink reaches
};
// Tuned to PRO12 reality: CMYK secondaries are dull in orange / pure-green /
// violet; the dedicated inks push markedly further. (Estimator constants —
// replaced by measured boundaries once the CMYKOGV target is read.)
constexpr Band kOr{ 55.0, 22.0, 55.0, 95.0};
constexpr Band kGr{150.0, 22.0, 58.0, 92.0};
constexpr Band kV {295.0, 22.0, 45.0, 88.0};

// Triangular hue membership (1 at centre → 0 at edge of band).
double membership(double hue, const Band& b) {
    double d = std::fabs(hue - b.hue_c);
    if (d > 180.0) d = 360.0 - d;            // wrap
    return d >= b.hue_hw ? 0.0 : (1.0 - d / b.hue_hw);
}

// Recruited coverage for one pixel in one band: only chroma ABOVE the CMYK
// ceiling needs the extra ink, scaled to the ink's headroom, weighted by how
// squarely the hue sits in the band.
double recruit(double hue, double C, const Band& b) {
    double m = membership(hue, b);
    if (m <= 0.0 || C <= b.cmyk_ceiling) return 0.0;
    double excess = (C - b.cmyk_ceiling) / (b.ink_max - b.cmyk_ceiling);
    if (excess > 1.0) excess = 1.0;
    return m * excess;
}

}  // namespace

OGVUsage estimate_ogv_usage(const float* lab, std::size_t n) {
    OGVUsage u;
    if (!lab || n == 0) { u.summary = "无像素"; return u; }

    double sOr = 0, sGr = 0, sV = 0;
    std::size_t cOr = 0, cGr = 0, cV = 0, cAny = 0;
    constexpr double kFireThresh = 0.05;     // count as "recruiting" above 5% coverage

    for (std::size_t i = 0; i < n; ++i) {
        double a = lab[i * 3 + 1], bb = lab[i * 3 + 2];
        double C = std::hypot(a, bb);
        double hue = std::atan2(bb, a) * 180.0 / kPi;
        if (hue < 0) hue += 360.0;

        double rOr = recruit(hue, C, kOr);
        double rGr = recruit(hue, C, kGr);
        double rV  = recruit(hue, C, kV);
        sOr += rOr; sGr += rGr; sV += rV;
        bool any = false;
        if (rOr > kFireThresh) { ++cOr; any = true; }
        if (rGr > kFireThresh) { ++cGr; any = true; }
        if (rV  > kFireThresh) { ++cV;  any = true; }
        if (any) ++cAny;
    }

    const double N = static_cast<double>(n);
    u.or_pct = static_cast<float>(sOr / N);
    u.gr_pct = static_cast<float>(sGr / N);
    u.v_pct  = static_cast<float>(sV  / N);
    u.or_ratio = static_cast<float>(cOr / N);
    u.gr_ratio = static_cast<float>(cGr / N);
    u.v_ratio  = static_cast<float>(cV  / N);
    u.extension_ratio = static_cast<float>(cAny / N);

    // §8.5 thresholds on the strongest channel.
    float peak = std::max({u.or_pct, u.gr_pct, u.v_pct});
    char buf[160];
    if (peak < 0.02f)
        std::snprintf(buf, sizeof(buf),
            "扩展色基本未用（峰值 %.0f%%）— 是否重选 intent / 换源文件？", peak * 100);
    else if (peak > 0.30f)
        std::snprintf(buf, sizeof(buf),
            "充分调用扩展色域（Or %.0f%% · Gr %.0f%% · V %.0f%%）",
            u.or_pct * 100, u.gr_pct * 100, u.v_pct * 100);
    else
        std::snprintf(buf, sizeof(buf),
            "适度调用（Or %.0f%% · Gr %.0f%% · V %.0f%% · %.0f%% 像素用到扩展色）",
            u.or_pct * 100, u.gr_pct * 100, u.v_pct * 100, u.extension_ratio * 100);
    u.summary = buf;
    return u;
}

}  // namespace gamut
}  // namespace jcolour
