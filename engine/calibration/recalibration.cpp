#include "recalibration.h"

#include "delta_e.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace jcolour {
namespace recal {

using printbase::ChannelBase;
using printbase::LinearizationCurve;
using printbase::PrintBase;

namespace {

std::string pctId(const std::string& prefix, double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s-%d", prefix.c_str(), int(std::lround(v * 100.0)));
    return buf;
}

void accumulate(Summary& s, double de) {
    s.avg = (s.avg * s.count + de) / double(s.count + 1);
    s.max = std::max(s.max, de);
    ++s.count;
}

}  // namespace

std::vector<ChartPatch> build_chart(const ChartSpec& spec) {
    std::vector<ChartPatch> out;
    out.push_back({"PAPER", "", 0.0});
    for (const auto& ch : spec.channels)
        for (double s : spec.steps) out.push_back({pctId(ch, s), ch, s});
    for (double g : spec.grays) out.push_back({pctId("GRAY", g), "GRAY", g});
    return out;
}

std::vector<std::pair<std::string, double>>
device_inks(const ChartPatch& p, const PrintBase& base, const ChartSpec& spec) {
    std::vector<std::pair<std::string, double>> inks;
    auto through = [&](const std::string& ch, double t) {
        const ChannelBase* c = base.find(ch);
        const double u = c && !c->curve.empty() ? c->curve.apply(t)
                                                : std::clamp(t, 0.0, 1.0);
        inks.push_back({ch, u});
    };
    if (p.channel.empty()) return inks;
    if (p.channel == "GRAY") {
        for (const auto& [ch, w] : spec.grayRecipe) through(ch, w * p.request);
        return inks;
    }
    through(p.channel, p.request);
    return inks;
}

Reference reference_labs(const PrintBase& base, const std::vector<ChartPatch>& chart) {
    Reference r;
    // 纸白参考：取第一个有渐变的通道的 input 0
    Lab paper{95, 0, 0};
    bool havePaper = false;
    for (const auto& c : base.channels)
        if (c.ramp.valid()) { paper = printbase::interpolate_ramp_lab(c.ramp, 0.0); havePaper = true; break; }

    for (const auto& p : chart) {
        Lab lab{0, 0, 0};
        bool ok = false;
        if (p.channel.empty()) {
            lab = paper; ok = havePaper;
        } else if (p.channel == "GRAY") {
            for (const auto& g : base.grayReference)
                if (g.id == p.id) { lab = g.lab; ok = true; break; }
        } else if (const ChannelBase* c = base.find(p.channel); c && c->ramp.valid()) {
            // 建档时这一请求值打出来的颜色：建档曲线 → 建档渐变实测
            const auto& rc = c->reference();
            const double u = rc.empty() ? p.request * c->effectiveLimit() : rc.apply(p.request);
            lab = printbase::interpolate_ramp_lab(c->ramp, u);
            ok = true;
        }
        r.labs.push_back(lab);
        r.valid.push_back(ok);
    }
    return r;
}

Evaluation evaluate(const std::vector<ChartPatch>& chart, const Reference& ref,
                    const std::vector<Lab>& measured) {
    Evaluation e;
    const size_t n = std::min({chart.size(), ref.labs.size(), measured.size()});
    for (size_t i = 0; i < n; ++i) {
        PatchDelta d;
        d.id = chart[i].id;
        d.channel = chart[i].channel;
        d.request = chart[i].request;
        d.reference = ref.labs[i];
        d.measured = measured[i];
        d.hasReference = ref.valid[i];
        if (d.hasReference) {
            d.de00 = delta_e::delta_e_2000(d.reference, d.measured);
            accumulate(e.all, d.de00);
            if (d.channel == "GRAY") accumulate(e.grays, d.de00);
            else if (!d.channel.empty()) accumulate(e.channelsOnly, d.de00);
        }
        e.patches.push_back(d);
    }
    return e;
}

RecalResult compute_recalibration(const PrintBase& base, const std::vector<ChartPatch>& chart,
                                  const std::vector<Lab>& measured) {
    RecalResult res;
    const Reference ref = reference_labs(base, chart);
    res.before = evaluate(chart, ref, measured);
    if (measured.size() < chart.size()) return res;

    // 纸白变化提示（换纸批次）
    for (size_t i = 0; i < chart.size(); ++i) {
        if (chart[i].channel.empty() && ref.valid[i]) {
            const double de = delta_e::delta_e_2000(ref.labs[i], measured[i]);
            if (de > 2.0) {
                char buf[64];
                std::snprintf(buf, sizeof buf, "PAPER_SHIFT:%.1f", de);
                res.warnings.push_back(buf);
            }
        }
    }
    bool anyGray = false, grayRef = false;
    for (size_t i = 0; i < chart.size(); ++i)
        if (chart[i].channel == "GRAY") { anyGray = true; grayRef = grayRef || ref.valid[i]; }
    if (anyGray && !grayRef) res.warnings.push_back("NO_REFERENCE_GRAY");

    for (const auto& cb : base.channels) {
        // 该通道在靶上的色块
        std::vector<size_t> idx;
        for (size_t i = 0; i < chart.size(); ++i)
            if (chart[i].channel == cb.channel) idx.push_back(i);
        if (idx.empty() || !cb.ramp.valid()) continue;

        ChannelCorrection cc;
        cc.channel = cb.channel;
        const double L = cb.effectiveLimit();
        cc.oldCurve = cb.curve.empty() ? LinearizationCurve::identity(cb.channel, L)
                                       : cb.curve;
        const LinearizationCurve refCurve = cb.reference().empty() ? cc.oldCurve : cb.reference();
        for (size_t i : idx)
            for (const auto& d : res.before.patches)
                if (d.id == chart[i].id && d.hasReference) accumulate(cc.before, d.de00);

        const Lab refPaper = printbase::interpolate_ramp_lab(cb.ramp, 0.0);
        const Lab refSolid = printbase::interpolate_ramp_lab(cb.ramp, L);
        auto href = [&](double u) {
            return printbase::sctv(printbase::interpolate_ramp_lab(cb.ramp, u), refPaper, refSolid);
        };

        // 稀疏实测比例 k(u) = 实测 SCTV / 建档响应 SCTV（同一设备值处）
        std::vector<std::pair<double, double>> ku;
        for (size_t i : idx) {
            const double u = cc.oldCurve.apply(chart[i].request);
            const double hr = href(u);
            if (hr < 1.0) continue;
            const double m = printbase::sctv(measured[i], refPaper, refSolid);
            ku.push_back({u, m / hr});
        }
        if (ku.empty()) continue;
        std::sort(ku.begin(), ku.end());
        auto k = [&](double u) {
            if (u <= ku.front().first) return ku.front().second;
            if (u >= ku.back().first) return ku.back().second;
            for (size_t j = 1; j < ku.size(); ++j)
                if (u <= ku[j].first) {
                    const double f = (u - ku[j - 1].first) / (ku[j].first - ku[j - 1].first);
                    return ku[j - 1].second + f * (ku[j].second - ku[j - 1].second);
                }
            return ku.back().second;
        };

        // 当前响应模型 h(u) = h_ref(u)·k(u)，细网格 + 单调化
        constexpr int G = 401;
        std::vector<double> gu(G), gh(G);
        for (int j = 0; j < G; ++j) {
            gu[j] = L * double(j) / double(G - 1);
            gh[j] = href(gu[j]) * k(gu[j]);
            if (j > 0) gh[j] = std::max(gh[j], gh[j - 1] + 1e-9);
        }

        cc.newCurve = cc.oldCurve;
        const size_t N = cc.oldCurve.nodes.size();
        for (size_t n = 0; n < N; ++n) {
            const double t = double(n) / double(N - 1);
            const double target = href(refCurve.apply(t));   // 建档时这一请求值的响应
            double u = L;
            if (target <= gh.front()) {
                u = 0.0;
            } else if (target > gh.back()) {
                if (target - gh.back() > 0.5) cc.hitLimit = true;
                u = L;
            } else {
                for (int j = 1; j < G; ++j)
                    if (target <= gh[j]) {
                        const double f = (target - gh[j - 1]) / (gh[j] - gh[j - 1]);
                        u = gu[j - 1] + f * (gu[j] - gu[j - 1]);
                        break;
                    }
            }
            cc.newCurve.nodes[n] = std::clamp(u, 0.0, L);
        }
        cc.newCurve.nodes.front() = 0.0;
        for (size_t n = 1; n < N; ++n)
            cc.newCurve.nodes[n] = std::max(cc.newCurve.nodes[n], cc.newCurve.nodes[n - 1]);
        cc.newCurve.limit = L;
        cc.shiftAtMid = cc.newCurve.apply(0.5) - cc.oldCurve.apply(0.5);
        for (size_t n = 0; n < N; ++n)
            cc.maxShift = std::max(cc.maxShift, std::fabs(cc.newCurve.nodes[n] - cc.oldCurve.nodes[n]));
        if (cc.hitLimit) res.warnings.push_back("HIT_LIMIT:" + cb.channel);
        res.channels.push_back(cc);
    }
    res.ok = !res.channels.empty();
    return res;
}

PrintBase apply_recalibration(const PrintBase& base, const RecalResult& r) {
    PrintBase out = base;
    for (const auto& cc : r.channels)
        if (ChannelBase* c = out.find(cc.channel)) c->curve = cc.newCurve;
    return out;
}

}  // namespace recal
}  // namespace jcolour
