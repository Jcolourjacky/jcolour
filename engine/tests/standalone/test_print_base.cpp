// 建档前置（墨限 + 线性化 + TIL + 建档闸门）—— 独立测试，不依赖 Catch2
#include "check.h"

#include "print_base.h"
#include "printer_sim.h"

#include <algorithm>

using namespace jcolour::printbase;

namespace {

PrintBase full_base(const SimulatedPrinter& sim) {
    PrintBase b;
    for (const auto& ch : epson_p_active_channels())
        b.channels.push_back(make_channel_base(sim.measureRamp(ch, default_ramp_steps())));
    b.totalInkLimit = 280;
    b.tilMeasuredOnLinearized = true;
    return b;
}

bool has(const GateResult& g, const std::string& code, const std::string& ch = "*") {
    for (const auto& i : g.issues)
        if (i.code == code && (ch == "*" || i.channel == ch)) return true;
    return false;
}

}  // namespace

TEST("SCTV endpoints and midpoint (ISO 20654)") {
    const Lab paper{95, 0, -2}, solid{50, -30, -50};
    CHECK_NEAR(sctv(paper, paper, solid), 0.0, 1e-9);
    CHECK_NEAR(sctv(solid, paper, solid), 100.0, 1e-9);
    const Lab mid{72.5, -15, -26};
    CHECK_NEAR(sctv(mid, paper, solid), 50.0, 1e-6);   // V 值对 Lab 线性
}

TEST("channel limit found below 100% on a saturating ink") {
    const auto sim = SimulatedPrinter::epson_p_nominal();
    const auto lim = detect_channel_limit(sim.measureRamp("C", default_ramp_steps()));
    CHECK(lim.limit >= 0.4 && lim.limit <= 1.0);
    CHECK(lim.limit <= lim.peakInput + 1e-9);
    CHECK(!lim.reversal);
}

TEST("channel limit flags reversal (bronzing) after the peak") {
    ChannelRamp r;
    r.channel = "K";
    const double L[] = {95, 70, 45, 25, 14, 10, 18};
    for (int i = 0; i < 7; ++i) r.patches.push_back({i / 6.0, {L[i], 0, 0}});
    const auto lim = detect_channel_limit(r);
    CHECK(lim.reversal);
    CHECK(lim.limit <= 5.0 / 6.0 + 1e-9);
}

TEST("linearization makes SCTV linear in the request (all 11 Epson P channels)") {
    const auto sim = SimulatedPrinter::epson_p_nominal();
    // 用密集渐变检验（13 阶建曲线，101 阶验证）
    std::vector<double> dense;
    for (int i = 0; i <= 100; ++i) dense.push_back(i / 100.0);
    for (const auto& ch : epson_p_active_channels()) {
        const auto cb = make_channel_base(sim.measureRamp(ch, default_ramp_steps()));
        CHECK(cb.curve.monotonic);
        CHECK_NEAR(cb.curve.nodes.front(), 0.0, 1e-12);
        CHECK_NEAR(cb.curve.nodes.back(), cb.effectiveLimit(), 1e-12);
        CHECK(cb.curve.nonlinearityBefore > 5.0);   // 原始响应明显非线性
        const Lab paper = sim.printSingle(ch, 0.0);
        const Lab solid = sim.printSingle(ch, cb.effectiveLimit());
        double worst = 0;
        for (int k = 0; k <= 20; ++k) {
            const double t = k / 20.0;
            const double s = sctv(sim.printSingle(ch, cb.curve.apply(t)), paper, solid);
            worst = std::max(worst, std::fabs(s - 100.0 * t));
        }
        CHECK(worst < 3.0);
    }
}

TEST("operator override of the channel limit rebuilds the curve to that limit") {
    const auto sim = SimulatedPrinter::epson_p_nominal();
    const auto cb = make_channel_base(sim.measureRamp("VM", default_ramp_steps()), 0.6);
    CHECK_NEAR(cb.effectiveLimit(), 0.6, 1e-12);
    CHECK_NEAR(cb.curve.apply(1.0), 0.6, 1e-12);
}

TEST("total ink limit scales combinations proportionally") {
    const auto out = apply_total_ink_limit({1.0, 1.0, 1.0, 0.6}, 300);
    double s = 0;
    for (double v : out) s += v;
    CHECK_NEAR(s, 3.0, 1e-9);
    CHECK_NEAR(out[0] / out[3], 1.0 / 0.6, 1e-9);
    const auto same = apply_total_ink_limit({0.5, 0.5}, 300);
    CHECK_NEAR(same[0], 0.5, 1e-12);
}

TEST("profiling gate: cannot profile without linearization and TIL") {
    const auto& req = epson_p_active_channels();
    PrintBase empty;
    auto g = profiling_gate(empty, req);
    CHECK(!g.ready);
    CHECK(has(g, "NO_RAMP", "C"));
    CHECK(has(g, "NO_TIL", ""));

    const auto sim = SimulatedPrinter::epson_p_nominal();
    PrintBase b = full_base(sim);
    CHECK(profiling_gate(b, req).ready);

    PrintBase noTil = b;
    noTil.totalInkLimit = 0;
    CHECK(has(profiling_gate(noTil, req), "NO_TIL"));

    PrintBase tilFirst = b;
    tilFirst.tilMeasuredOnLinearized = false;
    CHECK(has(profiling_gate(tilFirst, req), "TIL_BEFORE_LIN"));

    PrintBase stale = b;
    stale.find("Y")->limitOverride = 0.55;   // 改了墨限没重做线性化
    CHECK(has(profiling_gate(stale, req), "CURVE_STALE", "Y"));

    PrintBase noCurve = b;
    noCurve.find("Gr")->curve.nodes.clear();
    CHECK(has(profiling_gate(noCurve, req), "NO_CURVE", "Gr"));

    PrintBase lowTil = b;
    lowTil.totalInkLimit = 40;   // 低于单通道墨限
    CHECK(has(profiling_gate(lowTil, req), "TIL_OUT_OF_RANGE"));

    PrintBase missing = b;
    missing.channels.erase(missing.channels.begin() + 3);   // 少一个通道
    CHECK(has(profiling_gate(missing, req), "NO_RAMP", req[3]));
}

int main() { return RUN_ALL(); }
