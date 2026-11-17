// 色域外映射：视觉保真 / 色度最近 —— 独立测试，不依赖 Catch2（用 LCMS 内置配置文件）
#include "check.h"

#include "delta_e.h"
#include "oog_mapping.h"

#include <algorithm>
#include <cmath>

using namespace jcolour;
using namespace jcolour::gamut;
using spectral::Lab;

namespace {

double hue(const Lab& c) {
    double h = std::atan2(c.b, c.a) * 180.0 / 3.14159265358979323846;
    return h < 0 ? h + 360 : h;
}
double chroma(const Lab& c) { return std::hypot(c.a, c.b); }
double hueDiff(double a, double b) {
    double d = std::fabs(a - b);
    return d > 180 ? 360 - d : d;
}

struct Fixture {
    cmsHPROFILE pro = create_prophoto_profile();
    cmsHPROFILE srgb = cmsCreate_sRGBProfile();
    GamutBoundary src, dst;
    Fixture() {
        src.buildFromProfile(pro);
        dst.buildFromProfile(srgb);
    }
    ~Fixture() { cmsCloseProfile(pro); cmsCloseProfile(srgb); }
    Lab labOf(cmsHPROFILE p, double r, double g, double b) const {
        cmsHPROFILE lab = cmsCreateLab4Profile(nullptr);
        cmsHTRANSFORM x = cmsCreateTransform(p, TYPE_RGB_DBL, lab, TYPE_Lab_DBL,
                                             INTENT_RELATIVE_COLORIMETRIC, 0);
        double in[3] = {r, g, b};
        cmsCIELab o;
        cmsDoTransform(x, in, &o, 1);
        cmsDeleteTransform(x);
        cmsCloseProfile(lab);
        return {o.L, o.a, o.b};
    }
};

Fixture& fx() { static Fixture f; return f; }

}  // namespace

TEST("boundary: sRGB primaries inside, ProPhoto green outside") {
    auto& f = fx();
    CHECK(f.dst.valid());
    CHECK(f.src.valid());
    CHECK(f.dst.contains(f.labOf(f.srgb, 1, 0, 0), 1.5));
    CHECK(f.dst.contains(f.labOf(f.srgb, 0.5, 0.5, 0.5), 0.5));
    CHECK(!f.dst.contains(f.labOf(f.pro, 0, 1, 0), 0.5));
    // 源边界在原色尖角处允许 ~3% 的段最大值分辨率误差
    const Lab pg = f.labOf(f.pro, 0, 1, 0);
    CHECK(f.src.maxChroma(pg.L, hue(pg)) > 0.97 * chroma(pg));
    CHECK(f.dst.lMax() > 99.0);
}

TEST("closest: in-gamut colours untouched, OOG lands on the boundary") {
    auto& f = fx();
    OogMapper m(f.dst, &f.src, OogStrategy::ClosestColorimetric);
    const Lab in = f.labOf(f.srgb, 0.4, 0.6, 0.3);
    const Lab o = m.map(in);
    CHECK(delta_e::delta_e_2000(in, o) < 1e-9);
    const Lab g = f.labOf(f.pro, 0, 1, 0);
    const Lab og = m.map(g);
    CHECK(f.dst.contains(og, 0.5));
}

TEST("closest gives lower ΔE00 than visual fidelity on OOG colours") {
    auto& f = fx();
    OogMapper closest(f.dst, &f.src, OogStrategy::ClosestColorimetric);
    OogMapper visual(f.dst, &f.src, OogStrategy::VisualFidelity);
    std::vector<Lab> samples;
    for (int i = 0; i <= 6; ++i)
        for (int j = 0; j <= 6; ++j)
            for (int k = 0; k <= 6; ++k)
                samples.push_back(f.labOf(f.pro, i / 6.0, j / 6.0, k / 6.0));
    const auto sc = measure_mapping(closest, f.dst, samples);
    const auto sv = measure_mapping(visual, f.dst, samples);
    std::printf("  OOG %d/%d  closest ΔE00 %.2f  visual ΔE00 %.2f\n",
                sc.outOfGamut, sc.total, sc.meanDe00, sv.meanDe00);
    CHECK(sc.outOfGamut > 20);
    CHECK(sc.meanDe00 < sv.meanDe00);
}

TEST("visual fidelity keeps hue and keeps OOG gradations distinguishable") {
    auto& f = fx();
    OogMapper closest(f.dst, &f.src, OogStrategy::ClosestColorimetric);
    OogMapper visual(f.dst, &f.src, OogStrategy::VisualFidelity);
    // 同一色相、彩度递增、全部在 sRGB 外（但在 ProPhoto 内）的一串绿
    const Lab g = f.labOf(f.pro, 0.1, 0.8, 0.1);
    const double h = hue(g), L = g.L;
    std::vector<Lab> ramp;
    const double cDst = f.dst.maxChroma(L, h), cSrc = f.src.maxChroma(L, h);
    CHECK(cSrc > cDst * 1.3);
    for (int i = 0; i < 5; ++i) {
        const double C = cDst * 1.08 + (cSrc * 0.95 - cDst * 1.08) * i / 4.0;
        ramp.push_back({L, C * std::cos(h * 3.14159265358979323846 / 180),
                        C * std::sin(h * 3.14159265358979323846 / 180)});
    }
    // 视觉保真：色相不动、相邻台阶大小均匀（不出平台）；
    // 色度最近：为了 ΔE00 最小会挪色相，并在最外几阶挤成平台。
    std::vector<double> stepV, stepC;
    double hueDriftV = 0, hueDriftC = 0;
    for (size_t i = 0; i < ramp.size(); ++i) {
        const Lab v = visual.map(ramp[i]);
        const Lab c = closest.map(ramp[i]);
        hueDriftV = std::max(hueDriftV, hueDiff(hue(v), h));
        hueDriftC = std::max(hueDriftC, hueDiff(hue(c), h));
        CHECK(f.dst.contains(v, 1.0));
        if (i > 0) {
            const Lab v0 = visual.map(ramp[i - 1]);
            CHECK(chroma(v) > chroma(v0));   // 严格递增
            stepV.push_back(delta_e::delta_e_2000(v, v0));
            stepC.push_back(delta_e::delta_e_2000(c, closest.map(ramp[i - 1])));
        }
    }
    auto evenness = [](const std::vector<double>& s) {
        return *std::min_element(s.begin(), s.end()) / *std::max_element(s.begin(), s.end());
    };
    std::printf("  hue drift: visual %.2f°  closest %.2f° | step evenness: visual %.2f  closest %.2f\n",
                hueDriftV, hueDriftC, evenness(stepV), evenness(stepC));
    CHECK(hueDriftV < 1.0);
    CHECK(hueDriftC > hueDriftV);
    CHECK(evenness(stepV) > 0.4);
    CHECK(evenness(stepV) > evenness(stepC));
}

TEST("both strategies keep the neutral axis neutral") {
    auto& f = fx();
    for (auto s : {OogStrategy::VisualFidelity, OogStrategy::ClosestColorimetric}) {
        OogMapper m(f.dst, &f.src, s);
        for (double L : {5.0, 30.0, 50.0, 80.0, 99.0}) {
            const Lab o = m.map({L, 0, 0});
            CHECK(chroma(o) < 0.5);
        }
    }
}

TEST("visual fidelity leaves deep in-gamut colours nearly unchanged") {
    auto& f = fx();
    OogMapper visual(f.dst, &f.src, OogStrategy::VisualFidelity);
    const Lab skin{65, 15, 18};
    CHECK(delta_e::delta_e_2000(skin, visual.map(skin)) < 2.5);
}

TEST("image transform through the OOG device link (ProPhoto → sRGB)") {
    auto& f = fx();
    OogTransform tv, tc, ti;
    CHECK(tv.create(f.pro, f.srgb, OogStrategy::VisualFidelity, TYPE_RGB_16, TYPE_RGB_16));
    CHECK(tc.create(f.pro, f.srgb, OogStrategy::ClosestColorimetric, TYPE_RGB_16, TYPE_RGB_16));
    CHECK(ti.create(f.pro, f.srgb, OogStrategy::IccTable, TYPE_RGB_16, TYPE_RGB_16,
                    INTENT_PERCEPTUAL, true));
    // 两个不同的饱和绿 + 中灰
    const uint16_t in[9] = {0, 65535, 0, 6000, 58000, 6000, 30000, 30000, 30000};
    uint16_t ov[9], oc[9];
    tv.apply(in, ov, 3);
    tc.apply(in, oc, 3);
    auto dist = [](const uint16_t* a, const uint16_t* b) {
        double s = 0;
        for (int i = 0; i < 3; ++i) s += std::fabs(double(a[i]) - double(b[i]));
        return s;
    };
    std::printf("  green A/B distance: visual %.0f  closest %.0f (16-bit units)\n",
                dist(ov, ov + 3), dist(oc, oc + 3));
    CHECK(dist(ov, ov + 3) > dist(oc, oc + 3));
    // 中灰仍是灰
    CHECK(std::abs(int(ov[6]) - int(ov[7])) < 700 && std::abs(int(ov[7]) - int(ov[8])) < 700);
    CHECK(std::abs(int(oc[6]) - int(oc[7])) < 700 && std::abs(int(oc[7]) - int(oc[8])) < 700);
}

TEST("strategy keys round-trip; unknown → visual (photography default)") {
    for (auto s : {OogStrategy::VisualFidelity, OogStrategy::ClosestColorimetric, OogStrategy::IccTable})
        CHECK(oog_strategy_from_key(oog_strategy_key(s)) == s);
    CHECK(oog_strategy_from_key("bogus") == OogStrategy::VisualFidelity);
}

int main() { return RUN_ALL(); }
