#include "print_base.h"

#include "delta_e.h"

#include <algorithm>
#include <cmath>

namespace jcolour {
namespace printbase {

namespace {

struct V3 { double x, y, z; };

V3 vvals(const Lab& c) {
    return {c.L + 116.0 * c.a / 500.0, c.L, c.L - 116.0 * c.b / 200.0};
}

double dist(const V3& p, const V3& q) {
    return std::sqrt((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) +
                     (p.z - q.z) * (p.z - q.z));
}

Lab lerp(const Lab& p, const Lab& q, double f) {
    return {p.L + (q.L - p.L) * f, p.a + (q.a - p.a) * f, p.b + (q.b - p.b) * f};
}

// 单调化（只允许不减）：池相邻违例（PAVA）在等权下的简化——累计最大 +
// 平台内平均。对 12–21 个点足够。
std::vector<double> isotonic_increasing(const std::vector<double>& y) {
    struct Block { double sum; int n; };
    std::vector<Block> blocks;
    for (double v : y) {
        blocks.push_back({v, 1});
        while (blocks.size() > 1) {
            auto& b = blocks[blocks.size() - 1];
            auto& a = blocks[blocks.size() - 2];
            if (a.sum / a.n <= b.sum / b.n) break;
            a.sum += b.sum; a.n += b.n;
            blocks.pop_back();
        }
    }
    std::vector<double> out;
    for (const auto& b : blocks)
        for (int i = 0; i < b.n; ++i) out.push_back(b.sum / b.n);
    return out;
}

}  // namespace

double sctv(const Lab& tint, const Lab& substrate, const Lab& solid) {
    const V3 vt = vvals(tint), vp = vvals(substrate), vs = vvals(solid);
    const double den = dist(vs, vp);
    if (den < 1e-9) return 0.0;
    return 100.0 * dist(vt, vp) / den;
}

std::vector<RampPatch> ChannelRamp::sorted() const {
    auto s = patches;
    std::sort(s.begin(), s.end(), [](const RampPatch& a, const RampPatch& b) {
        return a.input < b.input;
    });
    return s;
}

bool ChannelRamp::valid() const {
    if (patches.size() < 4) return false;
    bool hasZero = false, hasHigh = false;
    for (const auto& p : patches) {
        if (p.input <= 1e-6) hasZero = true;
        if (p.input >= 0.5) hasHigh = true;
    }
    return hasZero && hasHigh;
}

// 单调保形三次插值（PCHIP / Fritsch–Carlson），逐 Lab 分量。
// 渐变阶距 10% 时线性插值误差可达 ~1 ΔE00，PCHIP 降到 0.2 左右，且不过冲。
namespace {
double pchip(const std::vector<double>& x, const std::vector<double>& y, double xi) {
    const size_t n = x.size();
    if (n == 1) return y[0];
    std::vector<double> h(n - 1), d(n - 1), m(n);
    for (size_t i = 0; i + 1 < n; ++i) {
        h[i] = std::max(1e-12, x[i + 1] - x[i]);
        d[i] = (y[i + 1] - y[i]) / h[i];
    }
    m[0] = d[0];
    m[n - 1] = d[n - 2];
    for (size_t i = 1; i + 1 < n; ++i) {
        if (d[i - 1] * d[i] <= 0) { m[i] = 0; continue; }
        const double w1 = 2 * h[i] + h[i - 1], w2 = h[i] + 2 * h[i - 1];
        m[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i]);
    }
    size_t k = 0;
    while (k + 2 < n && xi > x[k + 1]) ++k;
    const double t = (xi - x[k]) / h[k];
    const double t2 = t * t, t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * y[k] + (t3 - 2 * t2 + t) * h[k] * m[k] +
           (-2 * t3 + 3 * t2) * y[k + 1] + (t3 - t2) * h[k] * m[k + 1];
}
}  // namespace

Lab interpolate_ramp_lab(const ChannelRamp& ramp, double input) {
    const auto s = ramp.sorted();
    if (s.empty()) return {100, 0, 0};
    if (input <= s.front().input) return s.front().lab;
    if (input >= s.back().input) return s.back().lab;
    std::vector<double> x, L, a, b;
    for (const auto& p : s) {
        if (!x.empty() && p.input - x.back() < 1e-9) continue;   // 去重
        x.push_back(p.input); L.push_back(p.lab.L); a.push_back(p.lab.a); b.push_back(p.lab.b);
    }
    if (x.size() < 3) {
        for (size_t i = 1; i < s.size(); ++i)
            if (input <= s[i].input) {
                const double span = s[i].input - s[i - 1].input;
                return lerp(s[i - 1].lab, s[i].lab, span > 1e-12 ? (input - s[i - 1].input) / span : 0.0);
            }
        return s.back().lab;
    }
    return {pchip(x, L, input), pchip(x, a, input), pchip(x, b, input)};
}

ChannelLimit detect_channel_limit(const ChannelRamp& ramp, double fraction) {
    ChannelLimit out;
    const auto s = ramp.sorted();
    if (s.size() < 2) return out;
    const Lab paper = s.front().lab;
    std::vector<double> resp;
    for (const auto& p : s) resp.push_back(delta_e::delta_e_2000(p.lab, paper));

    size_t peak = 0;
    for (size_t i = 1; i < resp.size(); ++i)
        if (resp[i] > resp[peak]) peak = i;
    out.peakInput = s[peak].input;
    out.peakResponse = resp[peak];
    // 峰值后明显回落（>1 ΔE00）= 青铜化 / 过饱和
    for (size_t i = peak + 1; i < resp.size(); ++i)
        if (resp[peak] - resp[i] > 1.0) { out.reversal = true; break; }

    const double target = resp[peak] * fraction;
    out.limit = s[peak].input;
    for (size_t i = 1; i <= peak; ++i) {
        if (resp[i] >= target) {
            // 在 [i−1, i] 间线性插到目标响应
            const double r0 = resp[i - 1], r1 = resp[i];
            const double f = r1 - r0 > 1e-9 ? (target - r0) / (r1 - r0) : 1.0;
            out.limit = s[i - 1].input + f * (s[i].input - s[i - 1].input);
            break;
        }
    }
    // 墨限取 5% 网格，便于操作员读写（与 TIL 向导一致）
    out.limit = std::clamp(std::round(out.limit * 20.0) / 20.0, 0.05, 1.0);
    if (out.limit > out.peakInput + 1e-9) out.limit = out.peakInput;
    return out;
}

double LinearizationCurve::apply(double t) const {
    if (nodes.size() < 2) return std::clamp(t, 0.0, 1.0) * limit;
    t = std::clamp(t, 0.0, 1.0);
    const double x = t * double(nodes.size() - 1);
    const size_t i = std::min(size_t(x), nodes.size() - 2);
    const double f = x - double(i);
    return nodes[i] + (nodes[i + 1] - nodes[i]) * f;
}

LinearizationCurve LinearizationCurve::identity(const std::string& ch, double limit, int n) {
    LinearizationCurve c;
    c.channel = ch;
    c.limit = limit;
    n = std::max(2, n);
    for (int i = 0; i < n; ++i) c.nodes.push_back(limit * double(i) / double(n - 1));
    return c;
}

double ramp_sctv_at(const ChannelRamp& ramp, double input, double limit) {
    const Lab paper = interpolate_ramp_lab(ramp, 0.0);
    const Lab solid = interpolate_ramp_lab(ramp, limit);
    return sctv(interpolate_ramp_lab(ramp, input), paper, solid);
}

LinearizationCurve build_linearization(const ChannelRamp& ramp, double limit, int nodeCount) {
    LinearizationCurve curve;
    curve.channel = ramp.channel;
    curve.limit = std::clamp(limit, 0.05, 1.0);
    nodeCount = std::max(2, nodeCount);

    // 墨限内的采样点（含墨限处插值点）
    std::vector<double> u, r;
    for (const auto& p : ramp.sorted()) {
        if (p.input < curve.limit - 1e-9) {
            u.push_back(p.input);
            r.push_back(ramp_sctv_at(ramp, p.input, curve.limit));
        }
    }
    u.push_back(curve.limit);
    r.push_back(100.0);
    if (u.size() < 2 || u.front() > 1e-9) {
        return LinearizationCurve::identity(ramp.channel, curve.limit, nodeCount);
    }
    r.front() = 0.0;

    // 线性化前的非线性度（实测，未单调化）
    for (size_t i = 0; i < u.size(); ++i)
        curve.nonlinearityBefore =
            std::max(curve.nonlinearityBefore, std::fabs(r[i] - 100.0 * u[i] / curve.limit));

    // 原始响应是否单调（容差 0.5 SCTV）
    for (size_t i = 1; i < r.size(); ++i)
        if (r[i] < r[i - 1] - 0.5) curve.monotonic = false;

    // 单调化后再反解：每个目标 SCTV = 100·t，找 u
    std::vector<double> rm = isotonic_increasing(r);
    // 去平台：严格递增（极小斜率），避免反解时除零
    for (size_t i = 1; i < rm.size(); ++i)
        if (rm[i] <= rm[i - 1]) rm[i] = rm[i - 1] + 1e-6;
    // 端点归一：0 → 0，墨限 → 100
    const double r0 = rm.front(), r1 = rm.back();
    for (double& v : rm) v = (v - r0) / (r1 - r0) * 100.0;

    for (int k = 0; k < nodeCount; ++k) {
        const double target = 100.0 * double(k) / double(nodeCount - 1);
        double out = curve.limit;
        for (size_t i = 1; i < rm.size(); ++i) {
            if (target <= rm[i]) {
                const double f = (target - rm[i - 1]) / (rm[i] - rm[i - 1]);
                out = u[i - 1] + f * (u[i] - u[i - 1]);
                break;
            }
        }
        curve.nodes.push_back(std::clamp(out, 0.0, curve.limit));
    }
    curve.nodes.front() = 0.0;
    curve.nodes.back() = curve.limit;
    for (size_t i = 1; i < curve.nodes.size(); ++i)
        curve.nodes[i] = std::max(curve.nodes[i], curve.nodes[i - 1]);
    return curve;
}

ChannelBase make_channel_base(const ChannelRamp& ramp, double limitOverride, int nodeCount) {
    ChannelBase c;
    c.channel = ramp.channel;
    c.ramp = ramp;
    c.limit = detect_channel_limit(ramp);
    c.limitOverride = limitOverride > 0 ? std::clamp(limitOverride, 0.05, 1.0) : -1.0;
    c.curve = build_linearization(ramp, c.effectiveLimit(), nodeCount);
    c.referenceCurve = c.curve;
    return c;
}

const ChannelBase* PrintBase::find(const std::string& ch) const {
    for (const auto& c : channels)
        if (c.channel == ch) return &c;
    return nullptr;
}

ChannelBase* PrintBase::find(const std::string& ch) {
    for (auto& c : channels)
        if (c.channel == ch) return &c;
    return nullptr;
}

double PrintBase::sumOfChannelLimitsPercent() const {
    double s = 0.0;
    for (const auto& c : channels) s += c.effectiveLimit() * 100.0;
    return s;
}

std::vector<double> apply_total_ink_limit(const std::vector<double>& inks01, double tilPercent) {
    std::vector<double> out = inks01;
    if (tilPercent <= 0) return out;
    double sum = 0.0;
    for (double v : out) sum += std::max(0.0, v);
    const double cap = tilPercent / 100.0;
    if (sum <= cap || sum <= 0) return out;
    const double k = cap / sum;
    for (double& v : out) v = std::max(0.0, v) * k;
    return out;
}

GateResult profiling_gate(const PrintBase& base, const std::vector<std::string>& required) {
    GateResult g;
    double maxLimit = 0.0;
    for (const auto& ch : required) {
        const ChannelBase* c = base.find(ch);
        if (!c || !c->ramp.valid()) { g.issues.push_back({"NO_RAMP", ch}); continue; }
        maxLimit = std::max(maxLimit, c->effectiveLimit() * 100.0);
        if (c->curve.empty()) { g.issues.push_back({"NO_CURVE", ch}); continue; }
        if (std::fabs(c->curve.limit - c->effectiveLimit()) > 1e-6)
            g.issues.push_back({"CURVE_STALE", ch});
        if (!c->curve.monotonic) g.issues.push_back({"NOT_MONOTONIC", ch});
    }
    if (base.totalInkLimit <= 0) {
        g.issues.push_back({"NO_TIL", ""});
    } else {
        if (!base.tilMeasuredOnLinearized) g.issues.push_back({"TIL_BEFORE_LIN", ""});
        double sum = 0.0;
        for (const auto& ch : required)
            if (const ChannelBase* c = base.find(ch)) sum += c->effectiveLimit() * 100.0;
        if (base.totalInkLimit < maxLimit - 1e-6 || (sum > 0 && base.totalInkLimit > sum + 1e-6))
            g.issues.push_back({"TIL_OUT_OF_RANGE", ""});
    }
    g.ready = g.issues.empty();
    return g;
}

const std::vector<std::string>& epson_p_active_channels() {
    static const std::vector<std::string> k = {"K", "LC", "C", "VLM", "VM", "Y",
                                               "Or", "GY", "LGY", "V", "Gr"};
    return k;
}

}  // namespace printbase
}  // namespace jcolour
