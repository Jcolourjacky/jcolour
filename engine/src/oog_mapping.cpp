#include "oog_mapping.h"

#include "delta_e.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace jcolour {
namespace gamut {

using spectral::Lab;

namespace {

constexpr double kPi = 3.14159265358979323846;

double hueDeg(const Lab& c) {
    double h = std::atan2(c.b, c.a) * 180.0 / kPi;
    if (h < 0) h += 360.0;
    return h;
}

double chroma(const Lab& c) { return std::hypot(c.a, c.b); }

Lab fromLCh(double L, double C, double h) {
    const double r = h * kPi / 180.0;
    return {L, C * std::cos(r), C * std::sin(r)};
}

double wrapHue(double h) {
    h = std::fmod(h, 360.0);
    return h < 0 ? h + 360.0 : h;
}

}  // namespace

const char* oog_strategy_name_zh(OogStrategy s) {
    switch (s) {
        case OogStrategy::VisualFidelity:      return "视觉保真（保外观）";
        case OogStrategy::ClosestColorimetric: return "色度最近（最小 ΔE00）";
        case OogStrategy::IccTable:            return "ICC 表（可感知 / 饱和度）";
    }
    return "";
}

const char* oog_strategy_key(OogStrategy s) {
    switch (s) {
        case OogStrategy::VisualFidelity:      return "visual";
        case OogStrategy::ClosestColorimetric: return "closest";
        case OogStrategy::IccTable:            return "icc";
    }
    return "visual";
}

OogStrategy oog_strategy_from_key(const std::string& k) {
    if (k == "closest") return OogStrategy::ClosestColorimetric;
    if (k == "icc") return OogStrategy::IccTable;
    return OogStrategy::VisualFidelity;
}

// ── GamutBoundary ───────────────────────────────────────────────────────

void GamutBoundary::buildFromSamples(const std::vector<Lab>& pts) {
    std::fill(cmax_.begin(), cmax_.end(), -1.0);
    valid_ = false;
    if (pts.empty()) return;
    lMin_ = 100.0;
    lMax_ = 0.0;
    for (const auto& p : pts) {
        const double L = std::clamp(p.L, 0.0, 100.0);
        lMin_ = std::min(lMin_, L);
        lMax_ = std::max(lMax_, L);
        const int li = std::clamp(int(L / 100.0 * kLBins), 0, kLBins - 1);
        const int hi = std::clamp(int(hueDeg(p) / 360.0 * kHBins), 0, kHBins - 1);
        double& c = cmax_[size_t(li) * kHBins + size_t(hi)];
        c = std::max(c, chroma(p));
    }
    // 每个色相列内：空格在已填格之间线性插值，两端外为 0
    std::vector<bool> colFilled(kHBins, false);
    for (int h = 0; h < kHBins; ++h) {
        int prev = -1;
        for (int l = 0; l < kLBins; ++l) {
            double& c = cmax_[size_t(l) * kHBins + size_t(h)];
            if (c < 0) continue;
            colFilled[size_t(h)] = true;
            if (prev >= 0 && l - prev > 1) {
                const double c0 = cell(prev, h);
                for (int k = prev + 1; k < l; ++k)
                    cmax_[size_t(k) * kHBins + size_t(h)] =
                        c0 + (c - c0) * double(k - prev) / double(l - prev);
            }
            prev = l;
        }
        for (int l = 0; l < kLBins; ++l) {
            double& c = cmax_[size_t(l) * kHBins + size_t(h)];
            if (c < 0) c = 0.0;
        }
    }
    // 整列空：取最近的已填列
    for (int h = 0; h < kHBins; ++h) {
        if (colFilled[size_t(h)]) continue;
        for (int d = 1; d < kHBins; ++d) {
            const int a = (h + d) % kHBins, b = (h - d + kHBins) % kHBins;
            const int src = colFilled[size_t(a)] ? a : (colFilled[size_t(b)] ? b : -1);
            if (src < 0) continue;
            for (int l = 0; l < kLBins; ++l)
                cmax_[size_t(l) * kHBins + size_t(h)] = cell(l, src);
            break;
        }
    }
    // 3×3 膨胀（取邻格最大）后再做双线性插值：段最大值在原色尖角处是尖峰，
    // 直接插值会低估（把真实原色判成色域外）；取「相邻最大」又会在格边跳变、
    // 映射结果出台阶。膨胀 + 双线性两头兼顾：平滑且略偏保守地包住样本。
    {
        std::vector<double> d(cmax_.size(), 0.0);
        for (int l = 0; l < kLBins; ++l)
            for (int h = 0; h < kHBins; ++h) {
                double m = 0.0;
                for (int dl = -1; dl <= 1; ++dl) {
                    const int ll = l + dl;
                    if (ll < 0 || ll >= kLBins) continue;
                    for (int dh = -1; dh <= 1; ++dh)
                        m = std::max(m, cell(ll, (h + dh + kHBins) % kHBins));
                }
                d[size_t(l) * kHBins + size_t(h)] = m;
            }
        cmax_.swap(d);
    }
    valid_ = true;
    // 每个色相格中心的 cusp（视觉保真的焦点明度按色相线性插值，避免焦点跳变）
    for (int h = 0; h < kHBins; ++h) {
        const double hue = (h + 0.5) * 360.0 / kHBins;
        double bl = 50.0, bc = 0.0;
        for (double l = lMin_; l <= lMax_; l += 0.5) {
            const double c = maxChroma(l, hue);
            if (c > bc) { bc = c; bl = l; }
        }
        cuspL_[size_t(h)] = bl;
        cuspC_[size_t(h)] = bc;
    }
}

bool GamutBoundary::buildFromProfile(cmsHPROFILE device, int faceGrid) {
    valid_ = false;
    if (!device) return false;
    cmsHPROFILE lab = cmsCreateLab4Profile(nullptr);
    const cmsColorSpaceSignature cs = cmsGetColorSpace(device);
    std::vector<double> in;
    cmsUInt32Number fmt = 0;
    int nch = 0;
    if (cs == cmsSigRgbData) {
        fmt = TYPE_RGB_DBL; nch = 3;
        const int g = std::max(5, faceGrid);
        for (int face = 0; face < 6; ++face) {
            const int fixed = face / 2;
            const double v = (face % 2) ? 1.0 : 0.0;
            for (int i = 0; i < g; ++i)
                for (int j = 0; j < g; ++j) {
                    double px[3];
                    const double s = double(i) / (g - 1), t = double(j) / (g - 1);
                    int k = 0;
                    for (int c = 0; c < 3; ++c) px[c] = (c == fixed) ? v : (k++ == 0 ? s : t);
                    in.insert(in.end(), px, px + 3);
                }
        }
    } else if (cs == cmsSigCmykData) {
        fmt = TYPE_CMYK_DBL; nch = 4;
        const int g = 11;
        for (int c = 0; c < g; ++c) for (int m = 0; m < g; ++m)
            for (int y = 0; y < g; ++y) for (int k = 0; k < g; ++k) {
                in.push_back(100.0 * c / (g - 1)); in.push_back(100.0 * m / (g - 1));
                in.push_back(100.0 * y / (g - 1)); in.push_back(100.0 * k / (g - 1));
            }
    } else if (cs == cmsSigGrayData) {
        fmt = TYPE_GRAY_DBL; nch = 1;
        for (int i = 0; i < 256; ++i) in.push_back(i / 255.0);
    } else {
        cmsCloseProfile(lab);
        return false;
    }
    cmsHTRANSFORM xf = cmsCreateTransform(device, fmt, lab, TYPE_Lab_DBL,
                                          INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOCACHE);
    cmsCloseProfile(lab);
    if (!xf) return false;
    const size_t n = in.size() / size_t(nch);
    std::vector<cmsCIELab> out(n);
    cmsDoTransform(xf, in.data(), out.data(), cmsUInt32Number(n));
    cmsDeleteTransform(xf);
    std::vector<Lab> pts;
    pts.reserve(n);
    for (const auto& o : out) pts.push_back({o.L, o.a, o.b});
    buildFromSamples(pts);
    return valid_;
}

double GamutBoundary::maxChroma(double L, double hue) const {
    if (!valid_ || L < lMin_ - 1e-9 || L > lMax_ + 1e-9) return 0.0;
    const double lf = std::clamp(L / 100.0 * kLBins - 0.5, 0.0, double(kLBins - 1));
    const double hf = wrapHue(hue) / 360.0 * kHBins - 0.5;
    const int l0 = std::min(int(lf), kLBins - 2);
    const double fl = lf - l0;
    int h0 = int(std::floor(hf));
    const double fh = hf - h0;
    h0 = (h0 % kHBins + kHBins) % kHBins;
    const int h1 = (h0 + 1) % kHBins;
    const double c00 = cell(l0, h0), c01 = cell(l0, h1);
    const double c10 = cell(l0 + 1, h0), c11 = cell(l0 + 1, h1);
    return (1 - fl) * ((1 - fh) * c00 + fh * c01) + fl * ((1 - fh) * c10 + fh * c11);
}

bool GamutBoundary::contains(const Lab& c, double tol) const {
    if (!valid_) return true;
    if (c.L < lMin_ - tol || c.L > lMax_ + tol) return false;
    const double L = std::clamp(c.L, lMin_, lMax_);
    return chroma(c) <= maxChroma(L, hueDeg(c)) + tol;
}

void GamutBoundary::cusp(double hue, double& L, double& C) const {
    const double hf = wrapHue(hue) / 360.0 * kHBins - 0.5;
    int h0 = int(std::floor(hf));
    const double f = hf - h0;
    h0 = (h0 % kHBins + kHBins) % kHBins;
    const int h1 = (h0 + 1) % kHBins;
    L = (1 - f) * cuspL_[size_t(h0)] + f * cuspL_[size_t(h1)];
    C = (1 - f) * cuspC_[size_t(h0)] + f * cuspC_[size_t(h1)];
}

// ── OogMapper ───────────────────────────────────────────────────────────

OogMapper::OogMapper(const GamutBoundary& dst, const GamutBoundary* src, OogStrategy s)
    : dst_(dst), src_(src && src->valid() ? src : nullptr), strategy_(s) {}

Lab OogMapper::map(const Lab& in) const {
    if (!dst_.valid()) return in;
    switch (strategy_) {
        case OogStrategy::ClosestColorimetric: return mapClosest(in);
        case OogStrategy::VisualFidelity:      return mapVisual(in);
        case OogStrategy::IccTable:            return in;
    }
    return in;
}

Lab OogMapper::mapClosest(const Lab& in) const {
    if (dst_.contains(in, 0.0)) return in;
    const double h0 = hueDeg(in);
    // 起点：明度夹到范围内、彩度夹到边界
    const double Lc = std::clamp(in.L, dst_.lMin(), dst_.lMax());
    Lab best = fromLCh(Lc, std::min(chroma(in), dst_.maxChroma(Lc, h0) * 0.999), h0);
    double bestDe = delta_e::delta_e_2000(in, best);
    double bestL = Lc, bestH = h0;
    auto tryAt = [&](double L, double h) {
        L = std::clamp(L, dst_.lMin(), dst_.lMax());
        const double cmax = dst_.maxChroma(L, h) * 0.999;
        const Lab c = fromLCh(L, std::min(cmax, chroma(in)), h);
        const double de = delta_e::delta_e_2000(in, c);
        if (de < bestDe) { bestDe = de; best = c; bestL = L; bestH = h; }
    };
    for (double dh = -6.0; dh <= 6.0 + 1e-9; dh += 2.0)
        for (double L = dst_.lMin(); L <= dst_.lMax() + 1e-9; L += 2.0) tryAt(L, h0 + dh);
    const double cL = bestL, cH = bestH;
    for (double dh = -2.0; dh <= 2.0 + 1e-9; dh += 0.5)
        for (double dl = -2.0; dl <= 2.0 + 1e-9; dl += 0.25) tryAt(cL + dl, cH + dh);
    return best;
}

double OogMapper::rayToBoundary(const GamutBoundary& b, double Lf, double dL, double dC,
                                double hue, double) const {
    auto inside = [&](double s) {
        const double L = Lf + s * dL, C = s * dC;
        if (L < b.lMin() || L > b.lMax()) return false;
        return C <= b.maxChroma(L, hue);
    };
    double lo = 0.0, hi = 250.0;
    if (inside(hi)) return hi;
    for (int i = 0; i < 40; ++i) {
        const double mid = 0.5 * (lo + hi);
        (inside(mid) ? lo : hi) = mid;
    }
    return lo;
}

Lab OogMapper::mapVisual(const Lab& in) const {
    // 1) 明度范围映射（源黑白 → 目标黑白）
    const double sLo = src_ ? src_->lMin() : 0.0, sHi = src_ ? src_->lMax() : 100.0;
    const double dLo = dst_.lMin(), dHi = dst_.lMax();
    auto toDst = [&](double L) {
        return sHi - sLo > 1e-6 ? dLo + (L - sLo) * (dHi - dLo) / (sHi - sLo) : L;
    };
    auto toSrc = [&](double L) {
        return dHi - dLo > 1e-6 ? sLo + (L - dLo) * (sHi - sLo) / (dHi - dLo) : L;
    };
    const double L1 = std::clamp(toDst(in.L), dLo, dHi);
    const double C = chroma(in);
    if (C < 1e-6) return {L1, 0.0, 0.0};
    const double h = hueDeg(in);

    // 2) 彩度：沿指向目标 cusp 明度中性点的射线做膝点压缩（色相不变）
    double cuspL = 50.0, cuspC = 0.0;
    dst_.cusp(h, cuspL, cuspC);
    const double Lf = std::clamp(cuspL, dLo + 1.0, dHi - 1.0);
    const double vL = L1 - Lf, vC = C;
    const double d = std::hypot(vL, vC);
    if (d < 1e-9) return fromLCh(L1, C, h);
    const double uL = vL / d, uC = vC / d;

    const double dDst = rayToBoundary(dst_, Lf, uL, uC, h, 1.0);
    double dSrc;
    if (src_) {
        // 源边界在「已映射明度」空间里判断：把射线上的明度反映射回源刻度
        auto inside = [&](double s) {
            const double Lm = Lf + s * uL, Cs = s * uC;
            const double Ls = toSrc(Lm);
            if (Ls < src_->lMin() || Ls > src_->lMax()) return false;
            return Cs <= src_->maxChroma(Ls, h);
        };
        double lo = 0.0, hi = 250.0;
        if (inside(hi)) lo = hi;
        else for (int i = 0; i < 40; ++i) {
            const double mid = 0.5 * (lo + hi);
            (inside(mid) ? lo : hi) = mid;
        }
        dSrc = lo;
    } else {
        dSrc = dDst * 1.6;
    }

    double dOut = d;
    const double kneeD = knee * dDst;
    if (d > kneeD) {
        if (dSrc > dDst + 1e-6) {
            const double x = std::clamp((d - kneeD) / (dSrc - kneeD), 0.0, 1.0);
            dOut = kneeD + x * (dDst - kneeD);
        } else {
            dOut = std::min(d, dDst);
        }
    }
    const double Lo = Lf + dOut * uL, Co = std::max(0.0, dOut * uC);
    return fromLCh(Lo, Co, h);
}

OogStats measure_mapping(const OogMapper& m, const GamutBoundary& dst,
                         const std::vector<Lab>& samples) {
    OogStats s;
    for (const auto& c : samples) {
        ++s.total;
        if (dst.contains(c, 0.5)) continue;
        ++s.outOfGamut;
        const double de = delta_e::delta_e_2000(c, m.map(c));
        s.meanDe00 += de;
        s.maxDe00 = std::max(s.maxDe00, de);
    }
    if (s.outOfGamut) s.meanDe00 /= s.outOfGamut;
    return s;
}

// ── 图像变换：设备链接（源设备 → 映射 → 目标设备） ──────────────────────
//
// 为什么不用 Lab 抽象配置文件：ICC v4 的 16 位 Lab 编码把 a*/b* 限在 −128..127，
// ProPhoto 的绿 / 蓝（a* 可达 −190）一进 PCS 就被削平，映射前已经出现台阶。
// 这里在源设备空间（RGB 33³ / CMYK 17⁴）建 CLUT，每个节点走浮点 Lab（不限范围）
// → 映射 → 浮点 Lab → 目标设备，再封装成 ICC 设备链接交给 LCMS 插值。

namespace {

double floatScale(cmsColorSpaceSignature cs) {
    return (cs == cmsSigCmykData || cs == cmsSigCmyData) ? 100.0 : 1.0;
}

struct LinkCargo {
    const OogMapper* mapper;
    cmsHTRANSFORM toLab, fromLab;
    int nIn, nOut;
    double inScale, outScale;
};

cmsInt32Number linkSampler(const cmsUInt16Number In[], cmsUInt16Number Out[], void* cargo) {
    const auto* c = static_cast<const LinkCargo*>(cargo);
    double dev[cmsMAXCHANNELS] = {0};
    for (int i = 0; i < c->nIn; ++i) dev[i] = In[i] / 65535.0 * c->inScale;
    cmsCIELab lab;
    cmsDoTransform(c->toLab, dev, &lab, 1);
    const Lab r = c->mapper->map({lab.L, lab.a, lab.b});
    cmsCIELab m{r.L, r.a, r.b};
    double out[cmsMAXCHANNELS] = {0};
    cmsDoTransform(c->fromLab, &m, out, 1);
    for (int i = 0; i < c->nOut; ++i)
        Out[i] = cmsUInt16Number(std::lround(std::clamp(out[i] / c->outScale, 0.0, 1.0) * 65535.0));
    return TRUE;
}

cmsHPROFILE createOogDeviceLink(cmsHPROFILE src, cmsHPROFILE dst, const OogMapper& m) {
    const cmsColorSpaceSignature sc = cmsGetColorSpace(src), dc = cmsGetColorSpace(dst);
    const int nIn = int(cmsChannelsOf(sc)), nOut = int(cmsChannelsOf(dc));
    if (nIn < 1 || nIn > 4 || nOut < 1 || nOut > 8) return nullptr;
    cmsHPROFILE lab = cmsCreateLab4Profile(nullptr);
    LinkCargo cargo{&m,
                    cmsCreateTransform(src, cmsFormatterForColorspaceOfProfile(src, 0, TRUE), lab, TYPE_Lab_DBL,
                                       INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOCACHE | cmsFLAGS_NOOPTIMIZE),
                    cmsCreateTransform(lab, TYPE_Lab_DBL, dst, cmsFormatterForColorspaceOfProfile(dst, 0, TRUE),
                                       INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOCACHE | cmsFLAGS_NOOPTIMIZE),
                    nIn, nOut, floatScale(sc), floatScale(dc)};
    cmsCloseProfile(lab);
    cmsHPROFILE h = nullptr;
    if (cargo.toLab && cargo.fromLab) {
        h = cmsCreateProfilePlaceholder(nullptr);
        cmsSetProfileVersion(h, 4.3);
        cmsSetDeviceClass(h, cmsSigLinkClass);
        cmsSetColorSpace(h, sc);
        cmsSetPCS(h, dc);
        cmsSetHeaderRenderingIntent(h, INTENT_RELATIVE_COLORIMETRIC);
        cmsPipeline* lut = cmsPipelineAlloc(nullptr, cmsUInt32Number(nIn), cmsUInt32Number(nOut));
        cmsStage* clut = cmsStageAllocCLut16bit(nullptr, nIn <= 3 ? 33u : 17u, cmsUInt32Number(nIn),
                                                cmsUInt32Number(nOut), nullptr);
        bool ok = lut && clut && cmsStageSampleCLut16bit(clut, linkSampler, &cargo, 0) &&
                  cmsPipelineInsertStage(lut, cmsAT_END, clut);
        if (ok) {
            clut = nullptr;   // 归 pipeline 所有
            cmsMLU* desc = cmsMLUalloc(nullptr, 1);
            cmsMLUsetASCII(desc, "en", "US", m.strategy() == OogStrategy::VisualFidelity
                                                 ? "jcolour OOG visual fidelity link"
                                                 : "jcolour OOG closest colorimetric link");
            cmsWriteTag(h, cmsSigProfileDescriptionTag, desc);
            cmsMLUfree(desc);
            ok = cmsWriteTag(h, cmsSigAToB0Tag, lut);
        }
        if (clut) cmsStageFree(clut);
        if (lut) cmsPipelineFree(lut);
        if (!ok) { cmsCloseProfile(h); h = nullptr; }
    }
    if (cargo.toLab) cmsDeleteTransform(cargo.toLab);
    if (cargo.fromLab) cmsDeleteTransform(cargo.fromLab);
    return h;
}

}  // namespace

OogTransform::~OogTransform() {
    if (xf_) cmsDeleteTransform(xf_);
}

bool OogTransform::create(cmsHPROFILE src, cmsHPROFILE dst, OogStrategy s,
                          cmsUInt32Number inFmt, cmsUInt32Number outFmt,
                          int iccIntent, bool bpc) {
    if (xf_) { cmsDeleteTransform(xf_); xf_ = nullptr; }
    error_.clear();
    if (!src || !dst) { error_ = "missing profile"; return false; }
    if (s == OogStrategy::IccTable) {
        xf_ = cmsCreateTransform(src, inFmt, dst, outFmt, cmsUInt32Number(iccIntent),
                                 bpc ? cmsFLAGS_BLACKPOINTCOMPENSATION : 0);
        if (!xf_) error_ = "cmsCreateTransform failed";
        return xf_ != nullptr;
    }
    if (!dstB_.buildFromProfile(dst)) { error_ = "destination gamut sampling failed"; return false; }
    const bool haveSrc = srcB_.buildFromProfile(src);
    OogMapper mapper(dstB_, haveSrc ? &srcB_ : nullptr, s);
    cmsHPROFILE link = createOogDeviceLink(src, dst, mapper);
    if (!link) { error_ = "device link failed"; return false; }
    xf_ = cmsCreateTransform(link, inFmt, nullptr, outFmt, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(link);
    if (!xf_) error_ = "cmsCreateTransform(device link) failed";
    return xf_ != nullptr;
}

void OogTransform::apply(const void* in, void* out, cmsUInt32Number pixels) const {
    if (xf_) cmsDoTransform(xf_, in, out, pixels);
}

cmsHPROFILE create_prophoto_profile() {
    cmsCIExyYTRIPLE prim = {{0.7347, 0.2653, 1.0}, {0.1596, 0.8404, 1.0}, {0.0366, 0.0001, 1.0}};
    cmsToneCurve* g = cmsBuildGamma(nullptr, 1.8);
    cmsToneCurve* curves[3] = {g, g, g};
    cmsHPROFILE p = cmsCreateRGBProfile(cmsD50_xyY(), &prim, curves);
    cmsFreeToneCurve(g);
    return p;
}

}  // namespace gamut
}  // namespace jcolour
