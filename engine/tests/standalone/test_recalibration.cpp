// 打印机再校准 —— 独立测试，不依赖 Catch2
#include "check.h"

#include "print_base.h"
#include "printer_sim.h"
#include "recalibration.h"

using namespace jcolour;
using namespace jcolour::printbase;
using namespace jcolour::recal;

namespace {

PrintBase build_base(const SimulatedPrinter& sim) {
    PrintBase b;
    for (const auto& ch : epson_p_active_channels())
        b.channels.push_back(make_channel_base(sim.measureRamp(ch, default_ramp_steps())));
    b.totalInkLimit = 280;
    b.tilMeasuredOnLinearized = true;
    return b;
}

std::vector<Lab> print_chart(const SimulatedPrinter& sim, const PrintBase& base,
                             const ChartSpec& spec, const std::vector<ChartPatch>& chart) {
    std::vector<Lab> out;
    for (const auto& p : chart) out.push_back(sim.print(device_inks(p, base, spec)));
    return out;
}

ChartSpec spec_all() {
    ChartSpec s;
    s.channels = epson_p_active_channels();
    return s;
}

}  // namespace

TEST("chart is small: 4 steps per channel + paper + 3 grays") {
    const auto spec = spec_all();
    const auto chart = build_chart(spec);
    CHECK(chart.size() == 1 + 11 * 4 + 3);
    CHECK(chart.front().id == "PAPER");
    CHECK(chart[1].id == "K-25");
    CHECK(chart.back().id == "GRAY-75");
}

TEST("no drift → nothing to correct") {
    const auto sim = SimulatedPrinter::epson_p_nominal();
    PrintBase base = build_base(sim);
    const auto spec = spec_all();
    const auto chart = build_chart(spec);
    for (const auto& p : chart)
        if (p.channel == "GRAY") base.grayReference.push_back({p.id, sim.print(device_inks(p, base, spec))});
    const auto r = compute_recalibration(base, chart, print_chart(sim, base, spec, chart));
    CHECK(r.ok);
    CHECK(r.before.all.max < 0.3);
    for (const auto& cc : r.channels) CHECK(cc.maxShift < 0.01);
}

TEST("ink batch drift is pulled back by updating only the linearization") {
    const auto nominal = SimulatedPrinter::epson_p_nominal();
    PrintBase base = build_base(nominal);
    const auto spec = spec_all();
    const auto chart = build_chart(spec);
    for (const auto& p : chart)
        if (p.channel == "GRAY")
            base.grayReference.push_back({p.id, nominal.print(device_inks(p, base, spec))});

    SimulatedPrinter drifted = nominal;
    drifted.driftInk("C", 0.86, 0.0);     // 新墨批偏淡
    drifted.driftInk("VM", 1.10, 0.04);   // 偏浓 + 网点扩大
    drifted.driftInk("Y", 1.0, 0.08);
    drifted.driftInk("K", 0.93, 0.0);

    const auto before = print_chart(drifted, base, spec, chart);
    const auto r = compute_recalibration(base, chart, before);
    CHECK(r.ok);
    CHECK(r.before.channelsOnly.avg > 0.5);   // 44 块里 4 个通道漂了

    const PrintBase fixed = apply_recalibration(base, r);
    // ICC / 墨限 / TIL / 参考都没动，只有曲线变了
    CHECK(fixed.totalInkLimit == base.totalInkLimit);
    CHECK(fixed.channels.size() == base.channels.size());
    for (size_t i = 0; i < fixed.channels.size(); ++i) {
        CHECK(fixed.channels[i].effectiveLimit() == base.channels[i].effectiveLimit());
        CHECK(fixed.channels[i].ramp.patches.size() == base.channels[i].ramp.patches.size());
        CHECK(fixed.channels[i].curve.limit == base.channels[i].curve.limit);
    }
    CHECK(profiling_gate(fixed, epson_p_active_channels()).ready);

    const auto after = evaluate(chart, reference_labs(fixed, chart),
                                print_chart(drifted, fixed, spec, chart));
    std::printf("  before avg %.2f max %.2f | after avg %.2f max %.2f | gray %.2f → %.2f\n",
                r.before.channelsOnly.avg, r.before.channelsOnly.max,
                after.channelsOnly.avg, after.channelsOnly.max,
                r.before.grays.avg, after.grays.avg);
    CHECK(after.channelsOnly.avg < r.before.channelsOnly.avg * 0.4);
    CHECK(after.channelsOnly.avg < 1.0);
    CHECK(after.grays.avg < r.before.grays.avg);

    // 淡掉的墨（C）要加墨，变浓的（VM）要减墨
    for (const auto& cc : r.channels) {
        if (cc.channel == "C") CHECK(cc.shiftAtMid > 0.0);
        if (cc.channel == "VM") CHECK(cc.shiftAtMid < 0.0);
        if (cc.channel == "LC") CHECK(cc.maxShift < 0.01);   // 没漂的不动
    }
}

TEST("a much weaker ink cannot exceed its channel limit → flagged") {
    const auto nominal = SimulatedPrinter::epson_p_nominal();
    PrintBase base = build_base(nominal);
    ChartSpec spec = spec_all();
    const auto chart = build_chart(spec);
    SimulatedPrinter drifted = nominal;
    drifted.driftInk("Gr", 0.6, 0.0);
    const auto r = compute_recalibration(base, chart, print_chart(drifted, base, spec, chart));
    bool flagged = false;
    for (const auto& w : r.warnings) flagged = flagged || w == "HIT_LIMIT:Gr";
    CHECK(flagged);
    bool noGrayRef = false;
    for (const auto& w : r.warnings) noGrayRef = noGrayRef || w == "NO_REFERENCE_GRAY";
    CHECK(noGrayRef);   // 这个 base 没记录灰块基准
    for (const auto& cc : r.channels)
        if (cc.channel == "Gr") CHECK(cc.newCurve.apply(1.0) <= base.find("Gr")->effectiveLimit() + 1e-12);
}

TEST("paper batch change is reported") {
    const auto nominal = SimulatedPrinter::epson_p_nominal();
    PrintBase base = build_base(nominal);
    ChartSpec spec = spec_all();
    const auto chart = build_chart(spec);
    SimulatedPrinter newPaper = nominal;
    newPaper.setPaper({96.0, 1.5, -6.0});   // 增白剂更多
    const auto r = compute_recalibration(base, chart, print_chart(newPaper, base, spec, chart));
    bool flagged = false;
    for (const auto& w : r.warnings) flagged = flagged || w.rfind("PAPER_SHIFT:", 0) == 0;
    CHECK(flagged);
}

int main() { return RUN_ALL(); }
