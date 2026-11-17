#include "print_base_io.h"

#include <QJsonArray>

namespace jcolour {

using namespace printbase;

namespace {

QJsonArray labToJson(const Lab& l) { return {l.L, l.a, l.b}; }
Lab labFromJson(const QJsonValue& v) {
    const QJsonArray a = v.toArray();
    return {a.size() > 0 ? a[0].toDouble() : 0, a.size() > 1 ? a[1].toDouble() : 0,
            a.size() > 2 ? a[2].toDouble() : 0};
}

QJsonObject curveToJson(const LinearizationCurve& c) {
    QJsonObject o;
    o["limit"] = c.limit;
    o["nonlinearity"] = c.nonlinearityBefore;
    o["monotonic"] = c.monotonic;
    QJsonArray n;
    for (double v : c.nodes) n.append(v);
    o["nodes"] = n;
    return o;
}

LinearizationCurve curveFromJson(const QJsonObject& o, const std::string& ch) {
    LinearizationCurve c;
    c.channel = ch;
    c.limit = o["limit"].toDouble(1.0);
    c.nonlinearityBefore = o["nonlinearity"].toDouble();
    c.monotonic = o["monotonic"].toBool(true);
    for (const auto& v : o["nodes"].toArray()) c.nodes.push_back(v.toDouble());
    return c;
}

}  // namespace

QJsonObject printBaseToJson(const PrintBase& b) {
    QJsonObject o;
    o["til"] = b.totalInkLimit;
    o["tilOnLinearized"] = b.tilMeasuredOnLinearized;
    QJsonArray chs;
    for (const auto& c : b.channels) {
        QJsonObject co;
        co["channel"] = QString::fromStdString(c.channel);
        QJsonArray ramp;
        for (const auto& p : c.ramp.patches) {
            QJsonArray e = labToJson(p.lab);
            e.prepend(p.input);
            ramp.append(e);
        }
        co["ramp"] = ramp;
        co["limit"] = c.limit.limit;
        co["peakInput"] = c.limit.peakInput;
        co["peakResponse"] = c.limit.peakResponse;
        co["reversal"] = c.limit.reversal;
        co["limitOverride"] = c.limitOverride;
        co["curve"] = curveToJson(c.curve);
        if (!c.referenceCurve.empty()) co["referenceCurve"] = curveToJson(c.referenceCurve);
        chs.append(co);
    }
    o["channels"] = chs;
    QJsonArray gray;
    for (const auto& g : b.grayReference) {
        QJsonObject go;
        go["id"] = QString::fromStdString(g.id);
        go["lab"] = labToJson(g.lab);
        gray.append(go);
    }
    o["grayReference"] = gray;
    return o;
}

PrintBase printBaseFromJson(const QJsonObject& o) {
    PrintBase b;
    b.totalInkLimit = o["til"].toDouble();
    b.tilMeasuredOnLinearized = o["tilOnLinearized"].toBool();
    for (const auto& v : o["channels"].toArray()) {
        const QJsonObject co = v.toObject();
        ChannelBase c;
        c.channel = co["channel"].toString().toStdString();
        c.ramp.channel = c.channel;
        for (const auto& e : co["ramp"].toArray()) {
            const QJsonArray a = e.toArray();
            if (a.size() < 4) continue;
            c.ramp.patches.push_back({a[0].toDouble(), {a[1].toDouble(), a[2].toDouble(), a[3].toDouble()}});
        }
        c.limit.limit = co["limit"].toDouble(1.0);
        c.limit.peakInput = co["peakInput"].toDouble(1.0);
        c.limit.peakResponse = co["peakResponse"].toDouble();
        c.limit.reversal = co["reversal"].toBool();
        c.limitOverride = co["limitOverride"].toDouble(-1.0);
        c.curve = curveFromJson(co["curve"].toObject(), c.channel);
        if (co.contains("referenceCurve"))
            c.referenceCurve = curveFromJson(co["referenceCurve"].toObject(), c.channel);
        b.channels.push_back(c);
    }
    for (const auto& v : o["grayReference"].toArray()) {
        const QJsonObject go = v.toObject();
        b.grayReference.push_back({go["id"].toString().toStdString(), labFromJson(go["lab"])});
    }
    return b;
}

QJsonObject recalRecordToJson(const RecalRecord& r) {
    QJsonObject o;
    o["at"] = r.at.toString(Qt::ISODate);
    o["reason"] = r.reason;
    o["newBatch"] = r.newBatch;
    o["beforeAvg"] = r.beforeAvg;
    o["beforeMax"] = r.beforeMax;
    o["afterAvg"] = r.afterAvg;
    o["afterMax"] = r.afterMax;
    o["simulated"] = r.simulated;
    return o;
}

RecalRecord recalRecordFromJson(const QJsonObject& o) {
    RecalRecord r;
    r.at = QDateTime::fromString(o["at"].toString(), Qt::ISODate);
    r.reason = o["reason"].toString();
    r.newBatch = o["newBatch"].toString();
    r.beforeAvg = o["beforeAvg"].toDouble();
    r.beforeMax = o["beforeMax"].toDouble();
    r.afterAvg = o["afterAvg"].toDouble(-1);
    r.afterMax = o["afterMax"].toDouble(-1);
    r.simulated = o["simulated"].toBool();
    return r;
}

QString gateIssueText(const GateIssue& i) {
    const QString ch = QString::fromStdString(i.channel);
    if (i.code == "NO_RAMP")          return QStringLiteral("%1 通道没有渐变测量").arg(ch);
    if (i.code == "NO_CURVE")         return QStringLiteral("%1 通道没有线性化曲线").arg(ch);
    if (i.code == "CURVE_STALE")      return QStringLiteral("%1 通道改过墨限，需重做线性化").arg(ch);
    if (i.code == "NOT_MONOTONIC")    return QStringLiteral("%1 通道响应非单调（先清洗喷头再重测）").arg(ch);
    if (i.code == "NO_TIL")           return QStringLiteral("总墨量 TIL 未测定");
    if (i.code == "TIL_BEFORE_LIN")   return QStringLiteral("TIL 不是在当前线性化之后测定的，需重测");
    if (i.code == "TIL_OUT_OF_RANGE") return QStringLiteral("TIL 超出合理范围（低于单通道墨限或高于通道墨限之和）");
    return QString::fromStdString(i.code);
}

}  // namespace jcolour
