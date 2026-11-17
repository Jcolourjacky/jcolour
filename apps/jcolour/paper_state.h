#pragma once

// PaperState — the app-side "paper standard" spine.
//
// Single source of truth for built calibration configs, backed by the engine's
// SQLite ParamDb (engine/calibration/db.h). The new two-path UI reads this:
//   - 建檔校準 config library  → listConfigs()
//   - 列印前狀態確認閘          → evaluateGate()
//   - 噴頭 / 墨批 硬件          → nozzleCheck() / readCartridges() (engine epson)
//
// Storage model (increment 1): one ParamDb row per config at workflow slot 0
// acting as the "config header"; the per-mode built flags live in the JSON
// blob. Real per-workflow LUT blobs (slots 1/2/3/4/6) are added when the
// calibration stepper wires actual measurement.

#include <QString>
#include <QVector>
#include <QDateTime>
#include <array>
#include <memory>

#include "print_base_io.h"   // 建档前置地基（墨限 + 线性化 + TIL）与再校准记录

namespace jcolour {

// The 5 print modes, valued by their engine workflow id (5/CMYK is skipped).
enum class PrintMode { SingleBlack = 1, BlackLightGray = 2, BlackLightGrayColor = 3, RGB = 4, RGB16 = 6 };
constexpr int kNumModes = 5;
const std::array<PrintMode, kNumModes>& allModes();
int modeColumn(PrintMode m);            // 0..4 stable column index
QString modeFullName(PrintMode m);      // "單黑" / "黑+淺灰" / ...

// A built paper standard (aggregate over up to 5 modes).
struct PaperConfig {
    QString   id;                       // uuid
    QString   configName;               // "Photo Rag · 標準"
    int       version = 1;              // v1 / v2 / v3 …
    QString   paperId;                  // stable key, e.g. "hahnemuhle-photo-rag"
    QString   paperName;                // display, e.g. "Hahnemühle Photo Rag"
    QString   paperMeta;                // "Matte · 308gsm"
    QString   printer;                  // "P9500"
    QString   inkBatch;                 // "#A7"
    bool      enabled = true;
    QDateTime lastCalibrated;
    double    mediaAdvanceMm = 0.0;     // 走紙 1m correction (GMG 米尺測試 → paper_feed.h)
    bool      feedMeasured = false;     // 走紙是否實際量過 (區分 0.0 = 正好對齊 vs 未測)
    bool      qcPassed = false;         // GMG 品質目視檢查（色條/均勻度/線條）通過
    double    linBaselineDe = 0.0;      // 建檔時線性化(SCTV)殘差 ΔE — 自動校驗基準
    double    g7BaselineDe  = 0.0;      // 建檔時 G7 平均 ΔE — 自動校驗基準（彩色，校驗只看不調）
    std::array<bool, kNumModes> modeBuilt{false, false, false, false, false};

    // 建档前置地基（SmartProfiler 式：先线性化与限墨，再建档）。旧版环境没有
    // 记录时 hasBase = false——再校准需要它作为「建档时的状态」。
    bool                 hasBase = false;
    printbase::PrintBase base;
    // 再校准：只更新 base 里的线性化曲线；ICC / G7 / 版本号都不变
    QDateTime            lastRecalibrated;
    QVector<RecalRecord> recalHistory;
};

// Pre-print status gate, graded per the project decision:
//   HardBlock: UNCALIBRATED / NO_MATCH / DEGRADED / ON_HOLD  → print disabled
//   SoftWarn : STALE / NEEDS_RECAL                           → warn, may proceed
//   Green    : fresh exact match                             → go
enum class GateLevel { Green, SoftWarn, HardBlock };

struct GateResult {
    GateLevel level = GateLevel::HardBlock;
    QString   stateName;                // "READY" / "STALE" / "未校準" …
    QString   summary;                  // one-line banner text
    // sub-checks shown as the three lamps in the inspector
    bool      nozzleFresh = false;  QString nozzleInfo;   // 4h TTL, per printer
    bool      feedStored  = false;  QString feedInfo;     // per paper, once (GMG 米尺)
    bool      calOk       = false;  QString calInfo;      // freshness vs standard
    bool      qcOk        = false;  QString qcInfo;       // GMG 品質目視檢查 (色條/均勻度/線條)
};

// Engine epson hardware results surfaced to the UI.
struct NozzleReport { bool reachable = false; int channels = 0; int clogged = 0; QString text; };
struct InkLevels    { bool reachable = false; QString text; QVector<int> levels; };

class PaperState {
public:
    static PaperState& instance();

    // --- config library (建檔校準) ---
    QVector<PaperConfig> listConfigs() const;       // newest first
    void  saveConfig(const PaperConfig& c);         // upsert by id
    void  setEnabled(const QString& id, bool on);
    void  removeConfig(const QString& id);
    // 重新校正: clone with next version number (v2/v3…), new id, disabled-old left intact.
    PaperConfig nextVersionOf(const PaperConfig& base) const;

    // --- pre-print status gate (列印前狀態確認) ---
    GateResult evaluateGate(const QString& printer, const QString& paperId,
                            const QString& inkBatch, PrintMode mode) const;

    // --- hardware (engine epson, graceful when no printer) ---
    NozzleReport nozzleCheck(const QString& host);  // also stamps 4h nozzle freshness
    InkLevels    readCartridges(const QString& host, const QString& batchId);
    QDateTime    lastNozzleCheck(const QString& printer) const;

private:
    PaperState();
    ~PaperState();
    PaperState(const PaperState&) = delete;
    PaperState& operator=(const PaperState&) = delete;
    struct Impl;
    std::unique_ptr<Impl> d_;
};

}  // namespace jcolour
