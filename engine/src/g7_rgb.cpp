#include "g7_rgb.h"

#include <algorithm>
#include <cmath>

namespace jcolour {
namespace g7rgb {

namespace {

// L* ↔ Y（相对亮度）
double LtoY(double L) {
    const double f = (L + 16.0) / 116.0;
    return f * f * f > 0.008856 ? f * f * f : (f - 16.0 / 116.0) / 7.787;
}
double YtoL(double Y) {
    return Y > 0.008856 ? 116.0 * std::cbrt(Y) - 16.0 : 903.3 * Y;
}

// NPDC 形状：ND 域幂律缩放（tone 先做 gammaNpdc 预修正）
double toneToL(const NpdcModel& m, double tone) {
    const double t = std::pow(std::clamp(tone, 0.0, 100.0) / 100.0, m.gammaNpdc);
    const double ndP = -std::log10(std::max(1e-4, LtoY(m.paper.L)));
    const double ndB = -std::log10(std::max(1e-4, LtoY(m.black.L)));
    const double nd = ndP + (ndB - ndP) * std::pow(t, m.exponent);
    return YtoL(std::pow(10.0, -nd));
}

}  // namespace

const std::array<double, 25>& verifierTones() {
    static const std::array<double, 25> kTones = {
        0, 2, 4, 6, 8, 10, 15, 20, 25, 30, 35, 40, 45, 50,
        55, 60, 65, 70, 75, 80, 85, 90, 95, 98, 100};
    return kTones;
}

NpdcModel solveNpdc(const Lab& paper, const Lab& black,
                    double gammaNpdc, double anchorL50) {
    NpdcModel m;
    m.paper = paper;
    m.black = black;
    m.gammaNpdc = gammaNpdc;
    m.anchorL50 = anchorL50;
    // 二分求 exponent 使 toneToL(50) = anchorL50（L 随 exponent 单调递增：
    // 指数越大，中段 ND 越小 → 越亮）
    double lo = 0.5, hi = 4.0;
    for (int i = 0; i < 60; ++i) {
        m.exponent = (lo + hi) / 2;
        if (toneToL(m, 50.0) < anchorL50) lo = m.exponent;
        else hi = m.exponent;
    }
    return m;
}

Lab targetLab(const NpdcModel& m, double tone) {
    Lab out;
    out.L = toneToL(m, tone);
    // G7 灰平衡：a/b 自纸白按版调渐变归零（黑端中性）
    const double w = 1.0 - std::clamp(tone, 0.0, 100.0) / 100.0;
    out.a = m.paper.a * w;
    out.b = m.paper.b * w;
    return out;
}

int neutralValue(const NpdcModel& m, double tone, double gammaDisp) {
    // 色块按目标亮度做显示 Gamma 编码：RGB = 255·Y^(1/γ)。
    // 自检锚点：L*57.5 → Y≈0.2506 → 255×0.2506^(1/2.2) ≈ 135 ✓（实证值）
    const double Y = LtoY(toneToL(m, tone));
    const int v = int(std::lround(255.0 * std::pow(std::clamp(Y, 0.0, 1.0), 1.0 / gammaDisp)));
    return std::clamp(v, 0, 255);
}

std::vector<Patch> buildChart(const NpdcModel& m, bool light, int probeDelta) {
    // 区段中心：5 区段（36 格）或 3 区段（23 格）
    const std::vector<double> zones = light
        ? std::vector<double>{25, 50, 75}
        : std::vector<double>{15, 30, 50, 70, 85};
    std::vector<Patch> chart;
    // 锚点：纸白 + RGB(0,0,0) 黑点
    Patch pw; pw.r = pw.g = pw.b = 255; chart.push_back(pw);
    Patch bk; bk.r = bk.g = bk.b = 0; chart.push_back(bk);
    for (int z = 0; z < int(zones.size()); ++z) {
        const double tone = zones[size_t(z)];
        const int v = neutralValue(m, tone);
        const int d = probeDelta;
        auto add = [&](int dr, int dg, int db, int kind) {
            Patch p;
            p.r = std::clamp(v + dr, 0, 255);
            p.g = std::clamp(v + dg, 0, 255);
            p.b = std::clamp(v + db, 0, 255);
            p.tone = tone;
            p.zone = z;
            p.kind = kind;
            chart.push_back(p);
        };
        add(0, 0, 0, 0);        // 中性基准
        add(+d, 0, 0, 1);       // R+ → 偏红（a+）
        add(-d, 0, 0, 2);       // R−
        add(0, 0, +d, 3);       // B+ → 偏蓝（b−）
        add(0, 0, -d, 4);       // B−
        add(0, +d, 0, 5);       // G+ →（a−，兼动 L）
        // 36 格约束（i1Pro3 单条上限）：5 区段时最后一区省略 G−
        if (light || z != int(zones.size()) - 1)
            add(0, -d, 0, 6);   // G−
    }
    return chart;   // 36 格（5 区段）/ 23 格（3 区段），含纸白+黑点锚
}

Curves predict(const NpdcModel& m, const std::vector<Patch>& chart,
               const std::vector<Lab>& measured, double gammaDisp) {
    // 每区段：以中性基准的实测 ab 与目标 ab 之差，用探针的 dA/dR、dB/dB
    // 敏感度线性解出 ΔR、ΔB；ΔL 差用 G 通道敏感度补（近似三通道解耦）。
    struct ZoneFix { double tone; double dr, dg, db; };
    std::vector<ZoneFix> fixes;

    const int n = int(std::min(chart.size(), measured.size()));
    auto find = [&](int zone, int kind) -> const Lab* {
        for (int i = 0; i < n; ++i)
            if (chart[size_t(i)].zone == zone && chart[size_t(i)].kind == kind)
                return &measured[size_t(i)];
        return nullptr;
    };
    int maxZone = -1;
    for (const auto& p : chart) maxZone = std::max(maxZone, p.zone);

    for (int z = 0; z <= maxZone; ++z) {
        const Lab* base = find(z, 0);
        if (!base) continue;
        double tone = 0;
        int delta = 12;
        for (const auto& p : chart)
            if (p.zone == z && p.kind == 0) { tone = p.tone; break; }
        const Lab tgt = targetLab(m, tone);

        // 敏感度（每 +delta 通道值的 Lab 变化）；缺探针则用经验值
        auto sens = [&](int kindPlus, int kindMinus, double Lab::*ch,
                        double fallback) -> double {
            const Lab* plus = find(z, kindPlus);
            const Lab* minus = find(z, kindMinus);
            if (plus && minus) return ((*plus).*ch - (*minus).*ch) / (2.0 * delta);
            if (plus) return ((*plus).*ch - (*base).*ch) / delta;
            return fallback;
        };
        const double dA_dR = sens(1, 2, &Lab::a, +0.20);
        const double dB_dB = sens(3, 4, &Lab::b, -0.20);
        const double dL_dG = sens(5, 6, &Lab::L, +0.10);
        // 版调（NPDC）修正 = 三通道同动，不动灰平衡；G 约占亮度 72%，
        // 全通道同动的 L 敏感度按 1/0.72 放大
        const double dL_dAll = dL_dG / 0.72;

        ZoneFix f;
        f.tone = tone;
        const double errA = tgt.a - base->a;
        const double errB = tgt.b - base->b;
        const double errL = tgt.L - base->L;
        // dg 承载全通道版调修正；dr/db 仅承载灰平衡差分（叠加在 dg 之上）
        f.dg = std::abs(dL_dAll) > 1e-4 ? errL / dL_dAll : 0;
        f.dr = std::abs(dA_dR) > 1e-4 ? errA / dA_dR : 0;
        f.db = std::abs(dB_dB) > 1e-4 ? errB / dB_dB : 0;
        // 安全夹持：单区段修正不超过 ±30
        f.dr = std::clamp(f.dr, -30.0, 30.0);
        f.dg = std::clamp(f.dg, -30.0, 30.0);
        f.db = std::clamp(f.db, -30.0, 30.0);
        fixes.push_back(f);
    }

    // 25 点曲线 = 中性基线 + 区段修正的线性插值（两端锚 0 修正）
    Curves c;
    const auto& tones = verifierTones();
    auto lerpFix = [&](double tone, double ZoneFix::*ch) -> double {
        if (fixes.empty()) return 0;
        // 区段外平推（首/末区段修正水平延伸；端点由 0..255 夹持保护）
        if (tone <= fixes.front().tone) return fixes.front().*ch;
        if (tone >= fixes.back().tone) return fixes.back().*ch;
        for (size_t i = 0; i + 1 < fixes.size(); ++i) {
            if (tone >= fixes[i].tone && tone <= fixes[i + 1].tone) {
                const double u = (tone - fixes[i].tone) /
                                 (fixes[i + 1].tone - fixes[i].tone);
                return fixes[i].*ch * (1 - u) + fixes[i + 1].*ch * u;
            }
        }
        return 0;
    };
    for (size_t i = 0; i < tones.size(); ++i) {
        const double v = neutralValue(m, tones[i], gammaDisp);
        const double all = lerpFix(tones[i], &ZoneFix::dg);   // 版调（三通道同动）
        c.r[i] = std::clamp(v + all + lerpFix(tones[i], &ZoneFix::dr), 0.0, 255.0);
        c.g[i] = std::clamp(v + all, 0.0, 255.0);
        c.b[i] = std::clamp(v + all + lerpFix(tones[i], &ZoneFix::db), 0.0, 255.0);
    }
    return c;
}

Verdict verify(const NpdcModel& m, const std::vector<double>& tones,
               const std::vector<Lab>& measured) {
    Verdict v;
    const size_t n = std::min(tones.size(), measured.size());
    double sumL = 0, sumC = 0;
    for (size_t i = 0; i < n; ++i) {
        VerifyPoint p;
        p.tone = tones[i];
        p.target = targetLab(m, tones[i]);
        p.measured = measured[i];
        p.dL = p.measured.L - p.target.L;
        p.dCh = std::hypot(p.measured.a - p.target.a, p.measured.b - p.target.b);
        sumL += std::abs(p.dL);
        sumC += p.dCh;
        v.maxAbsDL = std::max(v.maxAbsDL, std::abs(p.dL));
        v.maxCh = std::max(v.maxCh, p.dCh);
        v.points.push_back(p);
    }
    if (n > 0) { v.avgAbsDL = sumL / double(n); v.avgCh = sumC / double(n); }
    v.pass = v.avgAbsDL <= 1.5 && v.maxAbsDL <= 3.0 &&
             v.avgCh <= 1.5 && v.maxCh <= 3.0;
    return v;
}

}  // namespace g7rgb
}  // namespace jcolour
