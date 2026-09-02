#pragma once

// G7(+) for RGB Printer —— Print By Number 方法论的引擎实现。
//
// 背景：原厂驱动是 RGB 入口（内部 LUT 不可知），换纸/换墨后灰平衡与
// 版调偏离，且没有 RIP 可改 CMYK。本模块把 G7 的版调(NPDC)+灰平衡
// 逻辑移植到 RGB 域：
//
//   · Verifier 25 阶调点（0,2,4,6,8,10,15,20..95,98,100%）。
//   · 色块 RGB 组合 = 按该阶调的目标亮度 Y 做显示 Gamma(2.2) 编码
//     （锚点自洽：tone50 目标 L*≈57.5 → RGB≈135，与实证一致）。
//   · L* 目标：纸白/黑点(RGB 0,0,0 实测)幂律缩放的 NPDC，tone 域先做
//     Gamma 0.9 预修正（实证结论：RGB 黑点比 CMY300 深，未修正则整体
//     过暗）；幂指数由「tone50 → CRPC6 C50MY40 L*≈57.5」锚点对给定
//     纸白/黑点自动求解 —— TR015 精确查表未复刻，此为其 Scaled 工程化。
//   · a*/b* 目标沿 G7 原规则：随版调自纸白渐变归零。
//   · Gray Finder：36 格导具（i1Pro3 单条上限；纸白+黑点+34 探针，
//     5 区段）或 23 格轻量版（3 区段）；每区段中性基准 ± R/B 偏移探针，
//     线性解出该区段的灰平衡修正，插值成 R/G/B 三条校正曲线。
//   · Verifier：25 点 ΔL* 与 ΔCh(√(Δa²+Δb²))，按 G7+ Grayscale 惯用
//     容差判定（avg≤1.5 / max≤3.0，近似标注）。
//
// 纯算法、无 Qt 依赖；UI 见 apps/jcolour/ui/g7_rgb_dialog。

#include <array>
#include <vector>

namespace jcolour {
namespace g7rgb {

struct Lab { double L = 0, a = 0, b = 0; };

// Verifier 的 25 个阶调点（%）
const std::array<double, 25>& verifierTones();

// 给定纸白与黑点（RGB 0,0,0 实测），解 NPDC 幂指数使 tone50 命中锚点
// L*（默认 57.5，CRPC6 C50MY40）。gammaNpdc 为 tone 域预修正（默认 0.9）。
struct NpdcModel {
    Lab paper, black;
    double gammaNpdc = 0.9;
    double exponent = 1.55;      // solve() 后为锚定值
    double anchorL50 = 57.5;
};
NpdcModel solveNpdc(const Lab& paper, const Lab& black,
                    double gammaNpdc = 0.9, double anchorL50 = 57.5);

// tone% → 目标 Lab（L 走 NPDC；a/b 沿 G7 规则自纸白渐变归零）
Lab targetLab(const NpdcModel& m, double tone);

// tone% → 导具/输出的中性 RGB 值（0..255；显示 Gamma 编码，默认 2.2）
int neutralValue(const NpdcModel& m, double tone, double gammaDisp = 2.2);

// ── 导具（Gray Finder 探针） ────────────────────────────────────────
struct Patch {
    int r = 0, g = 0, b = 0;
    double tone = -1;       // 所属区段中心 tone；-1 = 纸白/黑点锚点
    int zone = -1;          // 区段号；-1 = 锚点
    int kind = 0;           // 0 中性基准 · 1 R+ · 2 R− · 3 B+ · 4 B− · 5 G+ · 6 G−
};
// light=false → 36 格（5 区段×7 探针 + 纸白 + 黑点）
// light=true  → 23 格（3 区段×7 探针 + 纸白 + 黑点）
std::vector<Patch> buildChart(const NpdcModel& m, bool light, int probeDelta = 12);

// ── Gray Finder 预测：探针测量 → R/G/B 校正曲线 ────────────────────
// 曲线以 25 阶调点表示：out[i] = 该 tone 校正后的通道输出值（0..255）。
struct Curves {
    std::array<double, 25> r{}, g{}, b{};
};
Curves predict(const NpdcModel& m, const std::vector<Patch>& chart,
               const std::vector<Lab>& measured, double gammaDisp = 2.2);

// ── Verifier 判定 ───────────────────────────────────────────────────
struct VerifyPoint {
    double tone = 0;
    Lab target, measured;
    double dL = 0, dCh = 0;
};
struct Verdict {
    std::vector<VerifyPoint> points;
    double avgAbsDL = 0, maxAbsDL = 0, avgCh = 0, maxCh = 0;
    bool pass = false;       // avg≤1.5 且 max≤3.0（ΔL* 与 ΔCh 双指标）
};
Verdict verify(const NpdcModel& m, const std::vector<double>& tones,
               const std::vector<Lab>& measured);

}  // namespace g7rgb
}  // namespace jcolour
