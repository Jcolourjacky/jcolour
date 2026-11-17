#pragma once

// 色域外映射两种策略（对标 GMG OpenColor 3.4「色度最近 / 视觉保真」）。
//
//   VisualFidelity  视觉保真（保外观）—— 摄影默认。
//     1) 明度：源黑/白 → 打印机黑/白线性映射（类 BPC，保暗部层次）；
//     2) 彩度：沿「同色相、指向打印机该色相 cusp 明度的中性点」射线做膝点压缩：
//        离目标边界 80% 以内不动，80%..源边界 线性压进 80%..目标边界。
//        色域外的颜色彼此仍可区分（渐变不出平台），代价是整体 ΔE 偏大。
//   ClosestColorimetric  色度最近（最小 ΔE00）——
//     色域内不动；色域外取目标边界上 ΔE00 最小的点（色相 ±6° / 全明度范围
//     粗搜 + 细搜）。单色准确度最高，但同色相不同彩度的色域外颜色会挤到同一点。
//   IccTable  直接用 ICC 自带表（可感知 / 饱和度意图），引擎不插手。
//
// 落地方式（ADR-29 原则 1：语义/色域映射在 ICC 之前、PCS 内完成）：
//   源 ICC →（浮点 Lab，不受 ICC 16 位 Lab 编码 ±128 限制）→ 上述映射 → 打印机 ICC
//   （相对比色），在源设备空间采样成 33³ CLUT 的 ICC 设备链接交给 LCMS。
//   打印机 ICC 本身保持纯净。

#include "spectral.h"

#include <lcms2.h>

#include <cstdint>
#include <string>
#include <vector>

namespace jcolour {
namespace gamut {

enum class OogStrategy : int {
    VisualFidelity      = 0,
    ClosestColorimetric = 1,
    IccTable            = 2,
};
const char* oog_strategy_name_zh(OogStrategy s);
const char* oog_strategy_key(OogStrategy s);              // "visual" / "closest" / "icc"
OogStrategy oog_strategy_from_key(const std::string& k);  // 未知 → VisualFidelity

// 段最大值（segment maxima）色域边界：L* 50 段 × 色相 90 段，每格最大彩度。
class GamutBoundary {
public:
    static constexpr int kLBins = 50;
    static constexpr int kHBins = 90;

    void buildFromSamples(const std::vector<spectral::Lab>& pts);
    // 设备 → Lab（相对比色）采样：RGB 取立方体六个面，CMYK 取 11⁴ 网格。
    bool buildFromProfile(cmsHPROFILE device, int faceGrid = 65);

    bool   valid() const { return valid_; }
    double lMin() const { return lMin_; }
    double lMax() const { return lMax_; }
    double maxChroma(double L, double hueDeg) const;
    bool   contains(const spectral::Lab& c, double tol = 0.0) const;
    // 该色相最大彩度处的明度 / 彩度
    void   cusp(double hueDeg, double& L, double& C) const;

private:
    double cell(int li, int hi) const { return cmax_[size_t(li) * kHBins + size_t(hi)]; }
    std::vector<double> cmax_ = std::vector<double>(size_t(kLBins) * kHBins, 0.0);
    std::vector<double> cuspL_ = std::vector<double>(size_t(kHBins), 50.0);   // 每个色相格的 cusp 明度
    std::vector<double> cuspC_ = std::vector<double>(size_t(kHBins), 0.0);
    double lMin_ = 0.0, lMax_ = 100.0;
    bool   valid_ = false;
};

class OogMapper {
public:
    // src 可为 nullptr（未知源色域：视觉保真退化为「源边界 = 目标边界 × 1.6」）
    OogMapper(const GamutBoundary& dst, const GamutBoundary* src, OogStrategy s);

    spectral::Lab map(const spectral::Lab& in) const;
    OogStrategy strategy() const { return strategy_; }

    double knee = 0.80;   // 视觉保真：目标边界此比例以内不动

private:
    spectral::Lab mapClosest(const spectral::Lab& in) const;
    spectral::Lab mapVisual(const spectral::Lab& in) const;
    // 从中性点 (Lf, 0) 沿 (dL, dC) 方向到边界的距离
    double rayToBoundary(const GamutBoundary& b, double Lf, double dL, double dC,
                         double hue, double scaleIfMissing) const;

    const GamutBoundary& dst_;
    const GamutBoundary* src_;
    OogStrategy strategy_;
};

// 统计（A/B 对比用）
struct OogStats {
    int    total = 0;
    int    outOfGamut = 0;
    double meanDe00 = 0;       // 色域外样本：映射前后 ΔE00 平均
    double maxDe00 = 0;
};
OogStats measure_mapping(const OogMapper& m, const GamutBoundary& dst,
                         const std::vector<spectral::Lab>& samples);

// 完整图像变换：src → (映射) → dst（设备链接）。IccTable 时退化为普通 ICC 变换（iccIntent）。
class OogTransform {
public:
    OogTransform() = default;
    ~OogTransform();
    OogTransform(const OogTransform&) = delete;
    OogTransform& operator=(const OogTransform&) = delete;

    // inFmt / outFmt 为 LCMS TYPE_*；iccIntent 只在 IccTable 时生效
    bool create(cmsHPROFILE src, cmsHPROFILE dst, OogStrategy s,
                cmsUInt32Number inFmt, cmsUInt32Number outFmt,
                int iccIntent = INTENT_PERCEPTUAL, bool bpc = true);
    void apply(const void* in, void* out, cmsUInt32Number pixels) const;
    bool valid() const { return xf_ != nullptr; }
    const std::string& error() const { return error_; }
    const GamutBoundary& destinationBoundary() const { return dstB_; }

private:
    cmsHTRANSFORM xf_ = nullptr;
    GamutBoundary srcB_, dstB_;
    std::string error_;
};

// ProPhoto RGB（ROMM，D50，γ1.8）——内置源配置文件，测试与无嵌入配置文件时用
cmsHPROFILE create_prophoto_profile();

}  // namespace gamut
}  // namespace jcolour
