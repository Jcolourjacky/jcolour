#include "ink_lot_tracker.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QDate>

namespace jcolour {

InkLotTracker& InkLotTracker::instance() {
    static InkLotTracker instance;
    return instance;
}

InkLotTracker::InkLotTracker(QObject* parent)
    : QObject(parent)
{
    loadFromStorage();
    checkExpirationDates();
}

QString InkLotTracker::addLot(const QString& inkType, const QString& vendor,
                               const QString& batchNumber, const QDate& receivedDate,
                               const QDate& expirationDate) {
    QString lotId = QString("%1-%2-%3")
        .arg(inkType)
        .arg(receivedDate.toString("yyyy-MM"))
        .arg(lots_.size() + 1);

    InkLot lot;
    lot.id = lotId;
    lot.inkType = inkType;
    lot.vendor = vendor;
    lot.batchNumber = batchNumber;
    lot.receivedDate = receivedDate;
    lot.expirationDate = expirationDate;
    lot.remainingPercent = 100.0;
    lot.printsRemaining = 1000; // Estimated
    lot.isOpened = false;
    lot.targetL = 18; // Default for black
    lot.targetA = 0;
    lot.targetB = 0;

    lots_[lotId] = lot;
    saveToStorage();

    emit lotAdded(lotId);
    return lotId;
}

bool InkLotTracker::removeLot(const QString& lotId) {
    if (!lots_.contains(lotId)) return false;
    lots_.remove(lotId);
    saveToStorage();
    emit lotRemoved(lotId);
    return true;
}

bool InkLotTracker::updateLot(const QString& lotId, const InkLot& lot) {
    if (!lots_.contains(lotId)) return false;
    lots_[lotId] = lot;
    saveToStorage();
    emit lotUpdated(lotId);
    return true;
}

InkLot InkLotTracker::getLot(const QString& lotId) const {
    return lots_.value(lotId);
}

QVector<InkLot> InkLotTracker::getLotsByType(const QString& inkType) const {
    QVector<InkLot> result;
    for (const auto& lot : lots_) {
        if (lot.inkType == inkType) {
            result.append(lot);
        }
    }
    // Sort by expiration date
    std::sort(result.begin(), result.end(), [](const InkLot& a, const InkLot& b) {
        return a.expirationDate < b.expirationDate;
    });
    return result;
}

QVector<InkLot> InkLotTracker::getAllLots() const {
    QVector<InkLot> result;
    for (const auto& lot : lots_) {
        result.append(lot);
    }
    return result;
}

bool InkLotTracker::recordUsage(const QString& lotId, double coveragePercent, int count) {
    if (!lots_.contains(lotId)) return false;

    InkLot& lot = lots_[lotId];

    // Update remaining ink
    double usagePerPrint = coveragePercent * 0.01; // Simplified
    lot.remainingPercent = qMax(0.0, lot.remainingPercent - usagePerPrint * count);
    lot.printsRemaining = qMax(0, lot.printsRemaining - count);

    // If opened, update
    if (!lot.isOpened) {
        lot.isOpened = true;
        lot.openedDate = QDate::currentDate();
    }

    // Record usage
    InkUsageRecord record;
    record.date = QDate::currentDate();
    record.lotId = lotId;
    record.coveragePercent = coveragePercent;
    record.printCount = count;
    usageHistory_.append(record);

    updateEstimates();   // refine printsRemaining from accumulated history
    saveToStorage();
    emit lotUpdated(lotId);

    // Check if reorder needed
    if (needsReorder(lot.inkType)) {
        emit reorderRecommended(lot.inkType);
    }

    return true;
}

QVector<InkUsageRecord> InkLotTracker::getUsageHistory(const QString& lotId) const {
    QVector<InkUsageRecord> result;
    for (const auto& record : usageHistory_) {
        if (record.lotId == lotId) {
            result.append(record);
        }
    }
    return result;
}

QVector<InkUsageRecord> InkLotTracker::getAllUsageHistory() const {
    return usageHistory_;
}

QVector<InkLot> InkLotTracker::getExpiringLots(int daysAhead) const {
    QVector<InkLot> result;
    QDate threshold = QDate::currentDate().addDays(daysAhead);

    for (const auto& lot : lots_) {
        if (lot.expirationDate <= threshold && lot.expirationDate >= QDate::currentDate()) {
            result.append(lot);
        }
    }

    std::sort(result.begin(), result.end(), [](const InkLot& a, const InkLot& b) {
        return a.expirationDate < b.expirationDate;
    });

    return result;
}

QVector<InkLot> InkLotTracker::getExpiredLots() const {
    QVector<InkLot> result;
    QDate today = QDate::currentDate();

    for (const auto& lot : lots_) {
        if (lot.expirationDate < today) {
            result.append(lot);
        }
    }

    return result;
}

bool InkLotTracker::isLotExpired(const QString& lotId) const {
    if (!lots_.contains(lotId)) return true;
    return lots_[lotId].expirationDate < QDate::currentDate();
}

InkLotTracker::Statistics InkLotTracker::getStatistics() const {
    Statistics stats;
    stats.totalLots = lots_.size();
    stats.activeLots = 0;
    stats.expiredLots = 0;
    stats.lotsNeedingReorder = 0;
    stats.averageInkUsagePerPrint = 0;

    int totalUsage = 0;
    QDate today = QDate::currentDate();

    for (const auto& lot : lots_) {
        if (lot.expirationDate >= today) {
            stats.activeLots++;
        } else {
            stats.expiredLots++;
        }

        if (lot.remainingPercent < 20) {
            stats.lotsNeedingReorder++;
        }

        totalUsage += lot.printsRemaining;
    }

    if (!usageHistory_.isEmpty()) {
        double avg = 0;
        for (const auto& record : usageHistory_) {
            avg += record.coveragePercent;
        }
        stats.averageInkUsagePerPrint = avg / usageHistory_.size();
    }

    return stats;
}

bool InkLotTracker::needsReorder(const QString& inkType) const {
    // Reorder when ALL usable (active, non-expired) stock of this ink is running
    // low — i.e. there is no reasonably-full lot left. A near-empty lot sitting
    // next to a fresh one should not trigger a reorder.
    constexpr double kLowThreshold = 20.0;
    const QDate today = QDate::currentDate();
    bool haveActive = false;
    double maxRemaining = 0.0;
    for (const auto& lot : lots_) {
        if (lot.inkType == inkType && lot.expirationDate >= today) {
            haveActive = true;
            maxRemaining = qMax(maxRemaining, lot.remainingPercent);
        }
    }
    return haveActive && maxRemaining < kLowThreshold;
}

QStringList InkLotTracker::getReorderAlerts() const {
    QStringList alerts;

    // Check expiring lots
    auto expiring = getExpiringLots(7);
    for (const auto& lot : expiring) {
        int days = QDate::currentDate().daysTo(lot.expirationDate);
        alerts.append(QString("%1 expires in %2 days").arg(lot.id).arg(days));
    }

    // Check low ink
    for (const auto& lot : lots_) {
        if (lot.remainingPercent < 15) {
            alerts.append(QString("%1 low ink (%2% remaining)").arg(lot.id).arg(lot.remainingPercent, 0, 'f', 0));
        }
    }

    // Check reorder
    QStringList inkTypes = {"PK", "MK", "LK", "LLK", "C", "M", "Y"};
    for (const QString& type : inkTypes) {
        if (needsReorder(type)) {
            alerts.append(QString("Reorder %1: all active lots below 20%").arg(type));
        }
    }

    return alerts;
}

void InkLotTracker::checkExpirationDates() {
    QDate today = QDate::currentDate();

    for (const auto& lot : lots_) {
        if (lot.expirationDate < today) {
            emit lotExpired(lot.id);
        } else if (lot.expirationDate <= today.addDays(7)) {
            int days = today.daysTo(lot.expirationDate);
            emit lotExpiringSoon(lot.id, days);
        }
    }
}

void InkLotTracker::updateEstimates() {
    // Estimate remaining prints based on historical usage
    double avgCoverage = 0;
    if (!usageHistory_.isEmpty()) {
        for (const auto& record : usageHistory_) {
            avgCoverage += record.coveragePercent;
        }
        avgCoverage /= usageHistory_.size();
    }

    for (auto& lot : lots_) {
        if (avgCoverage > 0) {
            lot.printsRemaining = static_cast<int>(lot.remainingPercent / (avgCoverage * 0.01));
        }
    }
}

void InkLotTracker::saveToStorage() {
    QSettings settings("jcolour", "ink_lots");

    QJsonArray array;
    for (const auto& lot : lots_) {
        QJsonObject obj;
        obj["id"] = lot.id;
        obj["inkType"] = lot.inkType;
        obj["vendor"] = lot.vendor;
        obj["batchNumber"] = lot.batchNumber;
        obj["receivedDate"] = lot.receivedDate.toString(Qt::ISODate);
        obj["expirationDate"] = lot.expirationDate.toString(Qt::ISODate);
        obj["remainingPercent"] = lot.remainingPercent;
        obj["printsRemaining"] = lot.printsRemaining;
        obj["isOpened"] = lot.isOpened;
        obj["openedDate"] = lot.openedDate.toString(Qt::ISODate);
        obj["targetL"] = lot.targetL;
        obj["targetA"] = lot.targetA;
        obj["targetB"] = lot.targetB;
        obj["measuredL"] = lot.measuredL;
        obj["measuredA"] = lot.measuredA;
        obj["measuredB"] = lot.measuredB;
        obj["deltaE"] = lot.deltaE;
        obj["notes"] = lot.notes;
        array.append(obj);
    }
    settings.setValue("lots", QString(QJsonDocument(array).toJson()));

    // Save usage history
    QJsonArray historyArray;
    for (const auto& record : usageHistory_) {
        QJsonObject obj;
        obj["date"] = record.date.toString(Qt::ISODate);
        obj["lotId"] = record.lotId;
        obj["coveragePercent"] = record.coveragePercent;
        obj["printCount"] = record.printCount;
        historyArray.append(obj);
    }
    settings.setValue("history", QString(QJsonDocument(historyArray).toJson()));
}

void InkLotTracker::loadFromStorage() {
    QSettings settings("jcolour", "ink_lots");

    // Load lots
    QString json = settings.value("lots").toString();
    if (!json.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        if (doc.isArray()) {
            for (const auto& val : doc.array()) {
                QJsonObject obj = val.toObject();
                InkLot lot;
                lot.id = obj["id"].toString();
                lot.inkType = obj["inkType"].toString();
                lot.vendor = obj["vendor"].toString();
                lot.batchNumber = obj["batchNumber"].toString();
                lot.receivedDate = QDate::fromString(obj["receivedDate"].toString(), Qt::ISODate);
                lot.expirationDate = QDate::fromString(obj["expirationDate"].toString(), Qt::ISODate);
                lot.remainingPercent = obj["remainingPercent"].toDouble();
                lot.printsRemaining = obj["printsRemaining"].toInt();
                lot.isOpened = obj["isOpened"].toBool();
                lot.openedDate = QDate::fromString(obj["openedDate"].toString(), Qt::ISODate);
                lot.targetL = obj["targetL"].toDouble();
                lot.targetA = obj["targetA"].toDouble();
                lot.targetB = obj["targetB"].toDouble();
                lot.measuredL = obj["measuredL"].toDouble();
                lot.measuredA = obj["measuredA"].toDouble();
                lot.measuredB = obj["measuredB"].toDouble();
                lot.deltaE = obj["deltaE"].toDouble();
                lot.notes = obj["notes"].toString();
                lots_[lot.id] = lot;
            }
        }
    }

    // Load history
    QString historyJson = settings.value("history").toString();
    if (!historyJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(historyJson.toUtf8());
        if (doc.isArray()) {
            for (const auto& val : doc.array()) {
                QJsonObject obj = val.toObject();
                InkUsageRecord record;
                record.date = QDate::fromString(obj["date"].toString(), Qt::ISODate);
                record.lotId = obj["lotId"].toString();
                record.coveragePercent = obj["coveragePercent"].toDouble();
                record.printCount = obj["printCount"].toInt();
                usageHistory_.append(record);
            }
        }
    }
}

} // namespace jcolour