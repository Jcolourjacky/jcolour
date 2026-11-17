#pragma once

// PrintBase ⇄ JSON（打印环境里持久化「建档前置地基」与再校准记录）。
// 引擎类型见 engine/calibration/print_base.h；这里只做 Qt 侧序列化。

#include "print_base.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace jcolour {

// 一次再校准的记录（只更新线性化，特性文件不动）
struct RecalRecord {
    QDateTime at;
    QString   reason;          // 「换纸批次」/「换墨批次」/「定期」
    QString   newBatch;        // 新纸批 / 墨批（可空）
    double    beforeAvg = 0, beforeMax = 0;
    double    afterAvg = -1, afterMax = -1;   // <0 = 未复测
    bool      simulated = false;
};

QJsonObject printBaseToJson(const printbase::PrintBase& b);
printbase::PrintBase printBaseFromJson(const QJsonObject& o);

QJsonObject recalRecordToJson(const RecalRecord& r);
RecalRecord recalRecordFromJson(const QJsonObject& o);

// 建档闸门问题 → 中文说明（界面用）
QString gateIssueText(const printbase::GateIssue& i);

}  // namespace jcolour
