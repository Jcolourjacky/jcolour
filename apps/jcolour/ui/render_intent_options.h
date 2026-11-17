#pragma once

// 渲染意图下拉（摄影师版）：把「色域外处理」放进渲染意图里（GMG OpenColor 3.4
// 的两种色域外映射），摄影默认「视觉保真」。下拉序号 ⇄ {LCMS 意图, 色域外策略}：
//
//   0 视觉保真（保外观）· 摄影默认   相对比色 + 引擎视觉保真映射（Lab 抽象配置文件）
//   1 色度最近（最小 ΔE00）          相对比色 + 引擎最小 ΔE00 裁切
//   2 可感知（ICC 表）               ICC 自带可感知表，引擎不插手
//   3 饱和度（ICC 表）               ICC 自带饱和度表
//   4 绝对色度（模拟纸白）           绝对比色，色域外按色度最近
//
// GamutAdvisor 推荐「可感知」时映射到「视觉保真」（同属保外观），推荐「相对比色」
// 时映射到「色度最近」。

#include "oog_mapping.h"

#include <QStringList>
#include <lcms2.h>

namespace jcolour::ui {

struct RenderChoice {
    int lcmsIntent;
    jcolour::gamut::OogStrategy oog;
};

inline QStringList renderIntentLabels() {
    return {QStringLiteral("视觉保真（保外观）· 摄影默认"),
            QStringLiteral("色度最近（最小 ΔE00）"),
            QStringLiteral("可感知（ICC 表）"),
            QStringLiteral("饱和度（ICC 表）"),
            QStringLiteral("绝对色度（模拟纸白）")};
}

inline QString renderIntentTooltip() {
    return QStringLiteral(
        "色域外颜色怎么处理：\n"
        "· 视觉保真：保色相，把超出打印机色域的颜色按比例压进来，饱和区的层次与渐变保留（摄影默认）\n"
        "· 色度最近：色域内不动，色域外取 ΔE00 最小的可打印色——单色最准，但饱和渐变可能并成一片\n"
        "· ICC 表：直接用打印机 ICC 自带的可感知 / 饱和度表");
}

inline RenderChoice renderChoiceAt(int index) {
    using jcolour::gamut::OogStrategy;
    switch (index) {
        case 1:  return {INTENT_RELATIVE_COLORIMETRIC, OogStrategy::ClosestColorimetric};
        case 2:  return {INTENT_PERCEPTUAL, OogStrategy::IccTable};
        case 3:  return {INTENT_SATURATION, OogStrategy::IccTable};
        case 4:  return {INTENT_ABSOLUTE_COLORIMETRIC, OogStrategy::ClosestColorimetric};
        default: return {INTENT_RELATIVE_COLORIMETRIC, OogStrategy::VisualFidelity};
    }
}

inline int renderIndexFor(int lcmsIntent, jcolour::gamut::OogStrategy oog) {
    using jcolour::gamut::OogStrategy;
    switch (lcmsIntent) {
        case INTENT_PERCEPTUAL:
            return oog == OogStrategy::IccTable ? 2 : 0;
        case INTENT_SATURATION:
            return 3;
        case INTENT_ABSOLUTE_COLORIMETRIC:
            return 4;
        default:
            return oog == OogStrategy::ClosestColorimetric ? 1 : 0;
    }
}

// GamutAdvisor 的 LCMS 意图推荐 → 下拉序号
inline int renderIndexForAdvice(int lcmsIntent) {
    switch (lcmsIntent) {
        case INTENT_PERCEPTUAL:            return 0;
        case INTENT_RELATIVE_COLORIMETRIC: return 1;
        case INTENT_SATURATION:            return 3;
        case INTENT_ABSOLUTE_COLORIMETRIC: return 4;
        default:                           return 0;
    }
}

}  // namespace jcolour::ui
