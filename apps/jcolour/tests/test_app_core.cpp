// jcolour 应用层（非界面）测试：SBOM / 第三方声明、PrintBase 持久化、
// 渲染意图下拉映射。不依赖 Catch2 / QtTest。
#include "check.h"

#include "print_base_io.h"
#include "printer_sim.h"
#include "sbom.h"
#include "ui/render_intent_options.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace jcolour;

TEST("SBOM: CycloneDX 1.5 with runtime versions and licenses") {
    const auto comps = sbom::collect(false);
    sbom::BuildInfo info;
    info.appVersion = QStringLiteral("0.1.0");
    const QByteArray json = sbom::toCycloneDx(comps, info, QDateTime::fromSecsSinceEpoch(1763337600),
                                              QStringLiteral("00000000-0000-4000-8000-000000000001"));
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    CHECK(err.error == QJsonParseError::NoError);
    const QJsonObject root = doc.object();
    CHECK(root["bomFormat"].toString() == "CycloneDX");
    CHECK(root["specVersion"].toString() == "1.5");
    CHECK(root["serialNumber"].toString().startsWith("urn:uuid:"));
    CHECK(root["metadata"].toObject()["component"].toObject()["name"].toString() == "jcolour");
    const QJsonArray list = root["components"].toArray();
    CHECK(list.size() >= 8);
    bool qt = false, lcms = false, tiff = false, sqlite = false;
    for (const auto& v : list) {
        const QJsonObject o = v.toObject();
        CHECK(!o["name"].toString().isEmpty());
        CHECK(!o["licenses"].toArray().isEmpty());
        const QString n = o["name"].toString();
        if (n == "Qt") { qt = true; CHECK(o["version"].toString() == QString::fromLatin1(qVersion())); }
        if (n == "Little CMS") { lcms = true; CHECK(o["version"].toString().startsWith("2.")); }
        if (n == "libtiff") { tiff = true; CHECK(o["version"].toString().startsWith("4.")); }
        if (n == "SQLite") { sqlite = true; CHECK(o["version"].toString().startsWith("3.")); }
    }
    CHECK(qt && lcms && tiff && sqlite);
    CHECK(root["dependencies"].toArray().size() == 1);
}

TEST("third-party notices name every component and its license") {
    const auto comps = sbom::collect(false);
    const QString t = sbom::thirdPartyNotices(comps, {});
    for (const auto& c : comps) {
        CHECK(t.contains(c.name));
        CHECK(t.contains(c.license));
    }
    CHECK(t.contains(QStringLiteral("独立进程")));   // GPL / AGPL 组件的隔离说明
}

TEST("PrintBase JSON round-trip keeps curves, limits, TIL and references") {
    const auto sim = printbase::SimulatedPrinter::epson_p_nominal();
    printbase::PrintBase b;
    for (const auto& ch : printbase::epson_p_active_channels())
        b.channels.push_back(printbase::make_channel_base(sim.measureRamp(ch, printbase::default_ramp_steps())));
    b.find("Y")->limitOverride = 0.65;
    b.find("Y")->curve = printbase::build_linearization(b.find("Y")->ramp, 0.65);
    b.find("C")->curve.nodes[5] += 0.01;   // 模拟再校准后曲线 ≠ 建档曲线
    b.totalInkLimit = 285;
    b.tilMeasuredOnLinearized = true;
    b.grayReference.push_back({"GRAY-50", {55.2, 0.4, -1.1}});

    const QByteArray bytes = QJsonDocument(printBaseToJson(b)).toJson();
    const printbase::PrintBase r = printBaseFromJson(QJsonDocument::fromJson(bytes).object());
    CHECK(r.channels.size() == b.channels.size());
    CHECK(r.totalInkLimit == 285);
    CHECK(r.tilMeasuredOnLinearized);
    CHECK(r.grayReference.size() == 1 && r.grayReference[0].id == "GRAY-50");
    CHECK_NEAR(r.find("Y")->effectiveLimit(), 0.65, 1e-12);
    CHECK_NEAR(r.find("C")->curve.nodes[5], b.find("C")->curve.nodes[5], 1e-12);
    CHECK_NEAR(r.find("C")->reference().nodes[5], b.find("C")->referenceCurve.nodes[5], 1e-12);
    CHECK(r.find("K")->ramp.patches.size() == b.find("K")->ramp.patches.size());
    CHECK(printbase::profiling_gate(r, printbase::epson_p_active_channels()).ready);
}

TEST("recal record round-trip; gate issue texts are Chinese") {
    RecalRecord rec;
    rec.at = QDateTime::fromSecsSinceEpoch(1763337600);
    rec.reason = QStringLiteral("换墨批次");
    rec.newBatch = QStringLiteral("#A8");
    rec.beforeAvg = 2.1; rec.beforeMax = 4.5; rec.afterAvg = 0.3; rec.afterMax = 1.4;
    const RecalRecord r = recalRecordFromJson(recalRecordToJson(rec));
    CHECK(r.reason == rec.reason && r.newBatch == "#A8");
    CHECK_NEAR(r.afterMax, 1.4, 1e-12);
    CHECK(gateIssueText({"NO_TIL", ""}).contains(QStringLiteral("TIL")));
    CHECK(gateIssueText({"CURVE_STALE", "Y"}).startsWith("Y"));
}

TEST("render-intent menu: photography default is visual fidelity") {
    using jcolour::gamut::OogStrategy;
    const auto d = ui::renderChoiceAt(0);
    CHECK(d.oog == OogStrategy::VisualFidelity && d.lcmsIntent == INTENT_RELATIVE_COLORIMETRIC);
    CHECK(ui::renderChoiceAt(1).oog == OogStrategy::ClosestColorimetric);
    for (int i = 0; i < ui::renderIntentLabels().size(); ++i) {
        const auto c = ui::renderChoiceAt(i);
        CHECK(ui::renderIndexFor(c.lcmsIntent, c.oog) == i);
    }
    // 旧作业（相对比色、无色域外字段 → 0）读进来是视觉保真
    CHECK(ui::renderIndexFor(INTENT_RELATIVE_COLORIMETRIC, OogStrategy(0)) == 0);
    CHECK(ui::renderIndexForAdvice(INTENT_PERCEPTUAL) == 0);
    CHECK(ui::renderIndexForAdvice(INTENT_RELATIVE_COLORIMETRIC) == 1);
}

int main() { return RUN_ALL(); }
