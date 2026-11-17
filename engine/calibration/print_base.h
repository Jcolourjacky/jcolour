#pragma once

// PrintBase — 建档前的「设备地基」：每通道墨限 → 每通道线性化 → 总墨量 TIL。
//
// 对标 GMG ColorServer 5.7 SmartProfiler：特性化（建 ICC）之前先做线性化与限墨，
// 稳定性好得多。jcolour 的建档向导以 profiling_gate() 为准——地基不完整
// （缺通道、曲线过期、TIL 未在线性化之后测定……）就不能进入 G7 / ICC 步骤。
//
// 顺序（ADR-31）：单通道墨限 → 线性化（在墨限内）→ 总墨量 TIL（在已线性化
// 设备上测）→ G7 → ICC。线性化度量按 ADR-30 一律用 SCTV（ISO 20654），
// 密度/色度只用来找单通道墨限。
//
// 纯 C++，无 Qt / LCMS 依赖；界面层负责持久化（paper_state.cpp）。

#include "spectral.h"

#include <string>
#include <vector>

namespace jcolour {
namespace printbase {

using spectral::Lab;

// ISO 20654 SCTV（0..100）。V 值直接由 CIELAB 导出：
//   V_X = L* + 116·a*/500,  V_Y = L*,  V_Z = L* − 116·b*/200
// SCTV = 100 · |V(tint) − V(substrate)| / |V(solid) − V(substrate)|
double sctv(const Lab& tint, const Lab& substrate, const Lab& solid);

// 单通道渐变靶的一个色块：input = 请求墨量 0..1（未线性化的原始设备值）。
struct RampPatch {
    double input = 0.0;
    Lab    lab{0, 0, 0};
};

struct ChannelRamp {
    std::string            channel;
    std::vector<RampPatch> patches;   // 任意顺序；需含 0（纸白）

    // 按 input 排序后的副本
    std::vector<RampPatch> sorted() const;
    bool valid() const;               // ≥4 个点，含 0 与 ≥0.5 的点
};

// 原始响应插值（按 input 线性插值 Lab）
Lab interpolate_ramp_lab(const ChannelRamp& ramp, double input);

// ── 单通道墨限 ──────────────────────────────────────────────────────────
// 响应 = ΔE00(色块, 纸白)。墨限 = 首次达到峰值响应 fraction（默认 98%）的
// 输入值（Fiery「Ink Limit per Channel」/ TIL 向导第 1 步：最大色度/密度点，
// 很多介质 50–80% 即到顶）。峰值之后响应回落（青铜化 / 过饱和）记 reversal。
struct ChannelLimit {
    double limit     = 1.0;
    double peakInput = 1.0;
    double peakResponse = 0.0;   // 峰值 ΔE00
    bool   reversal  = false;
};
ChannelLimit detect_channel_limit(const ChannelRamp& ramp, double fraction = 0.98);

// ── 线性化 ──────────────────────────────────────────────────────────────
// 请求 t (0..1) → 设备墨量 u (0..limit)，使 SCTV(u; 纸白, 墨限处实地) ≈ 100·t。
struct LinearizationCurve {
    std::string         channel;
    double              limit = 1.0;          // 构建时用的通道墨限
    std::vector<double> nodes;                // nodes[i] = u(t = i/(N−1))
    double nonlinearityBefore = 0.0;          // 原始响应与理想直线的最大偏差（SCTV）
    bool   monotonic = true;

    bool   empty() const { return nodes.size() < 2; }
    double apply(double t) const;             // 线性插值，t 夹在 0..1
    static LinearizationCurve identity(const std::string& ch, double limit, int n = 33);
};

// 原始响应在墨限内的 SCTV（纸白 = input 0，实地 = 墨限处插值 Lab）
double ramp_sctv_at(const ChannelRamp& ramp, double input, double limit);

LinearizationCurve build_linearization(const ChannelRamp& ramp, double limit,
                                       int nodeCount = 33);

// ── 地基 ────────────────────────────────────────────────────────────────
struct ChannelBase {
    std::string        channel;
    ChannelRamp        ramp;                  // 建档时实测（再校准的参考来源）
    ChannelLimit       limit;
    double             limitOverride = -1.0;  // 操作员手改（<0 = 用自动）
    LinearizationCurve curve;                 // 当前曲线（再校准会更新）
    LinearizationCurve referenceCurve;        // 建档时曲线（再校准的「原状态」，永不改）

    const LinearizationCurve& reference() const {
        return referenceCurve.empty() ? curve : referenceCurve;
    }
    double effectiveLimit() const { return limitOverride > 0 ? limitOverride : limit.limit; }
};

// 再校准基准：建档时用同一张「少量色块靶」实测的灰块（可选，见 recalibration.h）
struct ReferencePatch {
    std::string id;
    Lab         lab{0, 0, 0};
};

// 一步：渐变实测 → 自动墨限（或手改）→ 线性化曲线
ChannelBase make_channel_base(const ChannelRamp& ramp, double limitOverride = -1.0,
                              int nodeCount = 33);

struct PrintBase {
    std::vector<ChannelBase>    channels;
    double totalInkLimit = 0.0;               // %，0 = 未测
    bool   tilMeasuredOnLinearized = false;   // TIL 是否在当前线性化之后测定
    std::vector<ReferencePatch> grayReference;  // 建档时灰块实测（再校准对照）

    const ChannelBase* find(const std::string& ch) const;
    ChannelBase*       find(const std::string& ch);
    double sumOfChannelLimitsPercent() const;
};

// 总墨量：Σ墨量·100 超过 TIL 时按比例缩减（0 = 不限）
std::vector<double> apply_total_ink_limit(const std::vector<double>& inks01, double tilPercent);

// ── 建档闸门 ────────────────────────────────────────────────────────────
// code：
//   NO_RAMP          该通道没有渐变测量
//   NO_CURVE         没有线性化曲线
//   CURVE_STALE      曲线构建时的墨限 ≠ 当前墨限（改了墨限没重做线性化）
//   NOT_MONOTONIC    曲线非单调（喷头 / 墨水问题，先清洗重测）
//   NO_TIL           总墨量未测
//   TIL_BEFORE_LIN   TIL 不是在当前线性化之后测的（须重测）
//   TIL_OUT_OF_RANGE TIL 超出 [最大单通道墨限, Σ通道墨限]
struct GateIssue {
    std::string code;
    std::string channel;      // 空 = 全局
};
struct GateResult {
    bool                   ready = false;
    std::vector<GateIssue> issues;
};
GateResult profiling_gate(const PrintBase& base, const std::vector<std::string>& requiredChannels);

// Epson SureColor P 系列 PRO 12 墨族的 11 个活动通道（channel_layout.h 顺序；
// MAIN_K = 当前上机的 PK 或 MK）
const std::vector<std::string>& epson_p_active_channels();

}  // namespace printbase
}  // namespace jcolour
