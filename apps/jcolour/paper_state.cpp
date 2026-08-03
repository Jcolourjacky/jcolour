#include "paper_state.h"

#include "db.h"             // engine/calibration — ParamDb
#include "state_machine.h"  // engine/calibration — compute_state / CalibState
#include "epson.h"          // engine/epson — Printer (nozzle / cartridges)

#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QUuid>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

namespace jcolour {

// ---- mode helpers ----------------------------------------------------------

const std::array<PrintMode, kNumModes>& allModes() {
    static const std::array<PrintMode, kNumModes> kModes{
        PrintMode::SingleBlack, PrintMode::BlackLightGray,
        PrintMode::BlackLightGrayColor, PrintMode::RGB, PrintMode::RGB16};
    return kModes;
}

int modeColumn(PrintMode m) {
    switch (m) {
        case PrintMode::SingleBlack:         return 0;
        case PrintMode::BlackLightGray:      return 1;
        case PrintMode::BlackLightGrayColor: return 2;
        case PrintMode::RGB:                 return 3;
        case PrintMode::RGB16:               return 4;
    }
    return 0;
}

QString modeFullName(PrintMode m) {
    switch (m) {
        case PrintMode::SingleBlack:         return QStringLiteral("單黑");
        case PrintMode::BlackLightGray:      return QStringLiteral("黑+淺灰");
        case PrintMode::BlackLightGrayColor: return QStringLiteral("黑+淺灰+四色");
        case PrintMode::RGB:                 return QStringLiteral("RGB");
        case PrintMode::RGB16:               return QStringLiteral("16-bit RGB");
    }
    return {};
}

// ---- impl ------------------------------------------------------------------

struct PaperState::Impl {
    std::unique_ptr<calibration::ParamDb> db;
    QSettings settings{"jcolour", "jcolour"};  // nozzle-check timestamps

    static QString dbPath() {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(dir);
        return dir + "/paper_state.db";
    }

    // (de)serialise a PaperConfig <-> JSON blob stored in ParamDb.
    static QString toBlob(const PaperConfig& c) {
        QJsonObject o;
        o["configName"]   = c.configName;
        o["version"]      = c.version;
        o["paperId"]      = c.paperId;
        o["paperName"]    = c.paperName;
        o["paperMeta"]    = c.paperMeta;
        o["printer"]      = c.printer;
        o["inkBatch"]     = c.inkBatch;
        o["enabled"]      = c.enabled;
        o["lastCal"]      = c.lastCalibrated.toString(Qt::ISODate);
        o["mediaAdvance"] = c.mediaAdvanceMm;
        o["feedMeasured"] = c.feedMeasured;
        o["qcPassed"]     = c.qcPassed;
        o["linBaselineDe"]= c.linBaselineDe;
        o["g7BaselineDe"] = c.g7BaselineDe;
        QJsonArray modes;
        for (bool b : c.modeBuilt) modes.append(b);
        o["modeBuilt"] = modes;
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }

    static PaperConfig fromStored(const calibration::StoredParamSet& s) {
        PaperConfig c;
        c.id = QString::fromStdString(s.key.paper_id);  // uuid lives in the key
        auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(s.blob));
        QJsonObject o = doc.object();
        c.configName     = o["configName"].toString();
        c.version        = o["version"].toInt(1);
        c.paperId        = o["paperId"].toString();
        c.paperName      = o["paperName"].toString();
        c.paperMeta      = o["paperMeta"].toString();
        c.printer        = o["printer"].toString();
        c.inkBatch       = o["inkBatch"].toString();
        c.enabled        = o["enabled"].toBool(true);
        c.lastCalibrated = QDateTime::fromString(o["lastCal"].toString(), Qt::ISODate);
        c.mediaAdvanceMm = o["mediaAdvance"].toDouble(0.0);
        // back-compat: pre-feedMeasured configs infer it from a non-zero advance.
        c.feedMeasured   = o.contains("feedMeasured") ? o["feedMeasured"].toBool()
                                                      : (c.mediaAdvanceMm != 0.0);
        c.qcPassed       = o["qcPassed"].toBool(false);
        c.linBaselineDe  = o["linBaselineDe"].toDouble(0.0);
        c.g7BaselineDe   = o["g7BaselineDe"].toDouble(0.0);
        QJsonArray modes = o["modeBuilt"].toArray();
        for (int i = 0; i < kNumModes && i < modes.size(); ++i)
            c.modeBuilt[i] = modes[i].toBool();
        return c;
    }

    calibration::ParamKey keyFor(const PaperConfig& c) const {
        return {c.inkBatch.toStdString(), c.id.toStdString(), /*workflow=header*/ 0};
    }

    void seedIfEmpty();
};

// Demo configs mirroring the design mockup, so the library is populated on
// first run (replaced by real calibration output once the stepper is wired).
void PaperState::Impl::seedIfEmpty() {
    if (db->count() > 0) return;
    auto now = QDateTime::currentDateTime();

    auto mk = [](const QString& name, int ver, const QString& pid, const QString& pname,
                 const QString& meta, const QString& batch, bool enabled,
                 QDateTime cal, double feed, std::array<bool, kNumModes> modes) {
        PaperConfig c;
        c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        c.configName = name; c.version = ver; c.paperId = pid; c.paperName = pname;
        c.paperMeta = meta; c.printer = "P9500"; c.inkBatch = batch; c.enabled = enabled;
        c.lastCalibrated = cal; c.mediaAdvanceMm = feed; c.modeBuilt = modes;
        c.feedMeasured = true; c.qcPassed = true;
        c.linBaselineDe = 0.4; c.g7BaselineDe = 0.6;   // 自動校驗基準
        return c;
    };

    QVector<PaperConfig> seed = {
        mk("Photo Rag · 標準", 2, "hahnemuhle-photo-rag", "Hahnemühle Photo Rag",
           "Matte · 308gsm", "#A7", true, now.addDays(-9), 0.3, {true,true,true,true,true}),
        mk("Photo Rag · 標準", 1, "hahnemuhle-photo-rag", "Hahnemühle Photo Rag",
           "Matte · 308gsm", "#A7", false, now.addDays(-30), 0.3, {true,true,true,true,true}),
        mk("Canson Baryta · 標準", 1, "canson-baryta", "Canson Infinity Baryta",
           "Gloss · 310gsm", "#A7", true, now.addDays(-2), 0.2, {true,true,true,true,false}),
        mk("Legacy Fibre · 標準", 1, "epson-legacy-fibre", "Epson Legacy Fibre",
           "Baryta · 325gsm", "#A6", true, now.addDays(-21), 0.4, {true,true,true,true,true}),
    };
    for (const auto& c : seed) {
        calibration::StoredParamSet s;
        s.key = {c.inkBatch.toStdString(), c.id.toStdString(), 0};
        s.blob = Impl::toBlob(c).toStdString();
        s.calibrated_at = c.lastCalibrated.toSecsSinceEpoch();
        s.last_used_at  = c.lastCalibrated.toSecsSinceEpoch();
        db->upsert(s);
    }
}

PaperState::PaperState() : d_(std::make_unique<Impl>()) {
    d_->db = std::make_unique<calibration::ParamDb>(Impl::dbPath().toStdString());
    d_->seedIfEmpty();
}
PaperState::~PaperState() = default;

PaperState& PaperState::instance() {
    static PaperState s;
    return s;
}

// ---- config library --------------------------------------------------------

QVector<PaperConfig> PaperState::listConfigs() const {
    QVector<PaperConfig> out;
    for (const auto& s : d_->db->list_mru(200)) {
        if (s.key.workflow != 0) continue;  // headers only
        out.push_back(Impl::fromStored(s));
    }
    return out;
}

void PaperState::saveConfig(const PaperConfig& c) {
    calibration::StoredParamSet s;
    s.key = d_->keyFor(c);
    s.blob = Impl::toBlob(c).toStdString();
    s.calibrated_at = c.lastCalibrated.toSecsSinceEpoch();
    s.last_used_at  = QDateTime::currentSecsSinceEpoch();
    d_->db->upsert(s);
}

void PaperState::setEnabled(const QString& id, bool on) {
    for (auto c : listConfigs()) {
        if (c.id == id) { c.enabled = on; saveConfig(c); return; }
    }
}

void PaperState::removeConfig(const QString& id) {
    for (const auto& c : listConfigs()) {
        if (c.id == id) { d_->db->remove(d_->keyFor(c)); return; }
    }
}

PaperConfig PaperState::nextVersionOf(const PaperConfig& base) const {
    int maxVer = base.version;
    for (const auto& c : listConfigs())
        if (c.paperId == base.paperId && c.configName == base.configName)
            maxVer = std::max(maxVer, c.version);
    PaperConfig c = base;
    c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    c.version = maxVer + 1;
    c.enabled = true;
    c.lastCalibrated = QDateTime::currentDateTime();
    return c;
}

// ---- pre-print status gate -------------------------------------------------

GateResult PaperState::evaluateGate(const QString& printer, const QString& paperId,
                                    const QString& inkBatch, PrintMode mode) const {
    GateResult g;
    const int col = modeColumn(mode);

    // 1) find the best enabled config for this paper+mode (prefer printer match).
    PaperConfig match; bool found = false;
    for (const auto& c : listConfigs()) {
        if (!c.enabled || c.paperId != paperId || !c.modeBuilt[col]) continue;
        if (!found || (c.printer == printer && match.printer != printer)
                   || c.lastCalibrated > match.lastCalibrated) {
            match = c; found = true;
        }
    }

    // 2) nozzle freshness (4h TTL, per printer) — independent of paper.
    QDateTime noz = lastNozzleCheck(printer);
    double hrs = noz.isValid() ? noz.secsTo(QDateTime::currentDateTime()) / 3600.0 : 1e9;
    g.nozzleFresh = noz.isValid() && hrs < 4.0;
    g.nozzleInfo = g.nozzleFresh
        ? QStringLiteral("%1 小時前 · 4 小時內有效").arg(hrs, 0, 'f', 1)
        : (noz.isValid() ? QStringLiteral("逾 4 小時 · 需重測") : QStringLiteral("未測"));

    if (!found) {
        g.level = GateLevel::HardBlock;
        g.stateName = QStringLiteral("未校準");
        g.calOk = false; g.calInfo = QStringLiteral("此紙×此模式無標準");
        g.feedStored = false; g.feedInfo = QStringLiteral("未建檔");
        g.qcOk = false; g.qcInfo = QStringLiteral("未檢查");
        g.summary = QStringLiteral("未校準 · 此紙在「%1」模式下無標準，請先建檔")
                        .arg(modeFullName(mode));
        return g;
    }

    // 3) feed (走紙) — measured once per paper via the GMG 米尺測試.
    g.feedStored = match.feedMeasured;
    g.feedInfo = g.feedStored
        ? QStringLiteral("此紙已量 · 1m 校正 %1%2mm")
              .arg(match.mediaAdvanceMm >= 0 ? "+" : "").arg(match.mediaAdvanceMm, 0, 'f', 1)
        : QStringLiteral("未測");

    // 3b) GMG 品質目視檢查 (色條一致 / 均勻度 / 線條無干涉紋) — advisory.
    g.qcOk = match.qcPassed;
    g.qcInfo = g.qcOk ? QStringLiteral("已通過") : QStringLiteral("未檢查（建議）");

    // 4) calibration freshness — real engine compute_state().
    calibration::ParamSet ps;
    ps.key = {match.inkBatch.toStdString(), match.paperId.toStdString()};
    ps.calibrated_at = match.lastCalibrated.toSecsSinceEpoch();
    ps.state = calibration::CalibState::FRESH;
    calibration::InkBatch curInk{inkBatch.toStdString()};
    calibration::Paper    curPaper{paperId.toStdString()};
    auto cs = calibration::compute_state(ps, QDateTime::currentSecsSinceEpoch(),
                                         curInk, curPaper);

    int days = match.lastCalibrated.daysTo(QDateTime::currentDateTime());
    switch (cs) {
        case calibration::CalibState::UNCALIBRATED:
            g.level = GateLevel::HardBlock; g.stateName = QStringLiteral("未校準");
            g.calOk = false; g.calInfo = QStringLiteral("無有效校準");
            g.summary = QStringLiteral("未校準 · 請先建檔"); break;
        case calibration::CalibState::FRESH:
            g.calOk = true; g.stateName = QStringLiteral("READY");
            g.calInfo = QStringLiteral("%1 天前 · 新鮮").arg(days);
            g.level = g.nozzleFresh ? GateLevel::Green : GateLevel::SoftWarn;
            g.summary = g.nozzleFresh ? QStringLiteral("可列印 · 已對齊標準")
                                      : QStringLiteral("可列印 · 噴頭逾 4 小時，建議先測噴頭");
            break;
        case calibration::CalibState::STALE_SOFT:
        case calibration::CalibState::STALE_HARD:
            g.calOk = false; g.stateName = QStringLiteral("略過期");
            g.calInfo = QStringLiteral("%1 天前 · 略過期 (STALE)").arg(days);
            g.level = GateLevel::SoftWarn;
            g.summary = QStringLiteral("可列印 · 校準略舊，建議重校；仍可繼續");
            break;
    }
    return g;
}

// ---- hardware (engine epson) ----------------------------------------------

NozzleReport PaperState::nozzleCheck(const QString& host) {
    NozzleReport r;
    if (host.trimmed().isEmpty()) { r.text = QStringLiteral("未設定印表機位址"); return r; }
    epson::Printer p(host.toStdString());
    auto nozzles = p.nozzle_check();              // empty if unreachable
    if (nozzles.empty()) {
        r.reachable = false;
        r.text = QStringLiteral("無法連線印表機（%1）").arg(host);
        return r;
    }
    r.reachable = true;
    r.channels = static_cast<int>(nozzles.size());
    for (const auto& n : nozzles) if (n.has_clog) ++r.clogged;
    r.text = r.clogged == 0
        ? QStringLiteral("噴頭正常 · %1 通道無堵塞").arg(r.channels)
        : QStringLiteral("偵測到 %1/%2 通道堵塞，建議清噴頭").arg(r.clogged).arg(r.channels);
    // stamp 4h freshness
    d_->settings.setValue("nozzle/" + host, QDateTime::currentDateTime());
    return r;
}

InkLevels PaperState::readCartridges(const QString& host, const QString& batchId) {
    InkLevels r;
    if (host.trimmed().isEmpty()) { r.text = QStringLiteral("未設定印表機位址"); return r; }
    epson::Printer p(host.toStdString());
    auto carts = batchId.isEmpty() ? p.read_cartridges()
                                   : p.read_cartridges(batchId.toStdString());
    if (carts.empty()) {
        r.reachable = false;
        r.text = QStringLiteral("無法讀取墨匣（%1）").arg(host);
        return r;
    }
    r.reachable = true;
    int low = 0;
    for (const auto& c : carts) { r.levels.push_back(c.level_pct); if (c.level_pct >= 0 && c.level_pct < 15) ++low; }
    r.text = low == 0
        ? QStringLiteral("%1 支墨匣 · 墨量充足").arg(carts.size())
        : QStringLiteral("%1 支墨匣 · %2 支偏低 (<15%)").arg(carts.size()).arg(low);
    return r;
}

QDateTime PaperState::lastNozzleCheck(const QString& printer) const {
    // keyed by host in nozzleCheck(); printer name maps 1:1 here for the demo
    return d_->settings.value("nozzle/" + printer).toDateTime();
}

}  // namespace jcolour
