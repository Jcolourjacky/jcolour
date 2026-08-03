#pragma once

#include <QObject>
#include <QString>
#include <QDate>
#include <QMap>
#include <QVector>

namespace jcolour {

struct InkLot {
    QString id;                 // e.g., "PK-2024-001"
    QString inkType;           // "PK", "MK", "LK", "LLK", "C", "M", "Y", "Or", "Gr", "V"
    QString vendor;
    QString batchNumber;
    QDate receivedDate;
    QDate expirationDate;
    double remainingPercent;    // 0-100
    int printsRemaining;       // Estimated based on coverage
    bool isOpened;
    QDate openedDate;

    // Color data (from spectral measurement)
    double targetL;
    double targetA;
    double targetB;
    double measuredL;
    double measuredA;
    double measuredB;
    double deltaE;

    QString notes;
};

struct InkUsageRecord {
    QDate date;
    QString lotId;
    QString imagePath;
    double coveragePercent;     // Average ink coverage
    int printCount;
};

class InkLotTracker : public QObject {
    Q_OBJECT
public:
    static InkLotTracker& instance();

    // Lot management
    QString addLot(const QString& inkType, const QString& vendor,
                   const QString& batchNumber, const QDate& receivedDate,
                   const QDate& expirationDate);
    bool removeLot(const QString& lotId);
    bool updateLot(const QString& lotId, const InkLot& lot);
    InkLot getLot(const QString& lotId) const;
    QVector<InkLot> getLotsByType(const QString& inkType) const;
    QVector<InkLot> getAllLots() const;

    // Usage tracking
    bool recordUsage(const QString& lotId, double coveragePercent, int count = 1);
    QVector<InkUsageRecord> getUsageHistory(const QString& lotId) const;
    QVector<InkUsageRecord> getAllUsageHistory() const;

    // Expiration alerts
    QVector<InkLot> getExpiringLots(int daysAhead = 30) const;
    QVector<InkLot> getExpiredLots() const;
    bool isLotExpired(const QString& lotId) const;

    // Statistics
    struct Statistics {
        int totalLots;
        int activeLots;
        int expiredLots;
        int lotsNeedingReorder;
        double averageInkUsagePerPrint;
    };
    Statistics getStatistics() const;

    // Warnings
    bool needsReorder(const QString& inkType) const;
    QStringList getReorderAlerts() const;

signals:
    void lotAdded(const QString& lotId);
    void lotUpdated(const QString& lotId);
    void lotRemoved(const QString& lotId);
    void lotExpired(const QString& lotId);
    void lotExpiringSoon(const QString& lotId, int daysRemaining);
    void reorderRecommended(const QString& inkType);

private:
    InkLotTracker(QObject* parent = nullptr);
    ~InkLotTracker() = default;

    void saveToStorage();
    void loadFromStorage();
    void checkExpirationDates();
    void updateEstimates();

    QMap<QString, InkLot> lots_;
    QVector<InkUsageRecord> usageHistory_;
};

} // namespace jcolour