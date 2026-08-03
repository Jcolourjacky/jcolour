#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QDateTime>

namespace jcolour {
namespace monitor {

enum class CalibrationStatus {
    Unknown,
    NotCalibrated,
    RecentlyCalibrated,
    CalibrationExpired,
    OutOfGamut
};

struct CalibrationInfo {
    CalibrationStatus status;
    QDateTime lastCalibration;
    int daysSinceCalibration;
    double maxLuminance;
    double whitePointTemp; // Kelvin
    double deltaEToTarget;
    QString profileName;
    QString deviceInfo;
};

class MonitorCalibrationMonitor : public QObject {
    Q_OBJECT
public:
    static MonitorCalibrationMonitor& instance();

    // Check current monitor calibration state
    CalibrationInfo checkCalibration();

    // Check if display profile exists
    bool hasDisplayProfile() const;

    // Get display profile info
    QString displayProfileName() const;

    // Check if monitor supports calibration
    bool supportsCalibration() const;

    // Trigger manual re-check
    void refresh();

    // User preference: target calibration interval (days)
    void setTargetInterval(int days);
    int targetInterval() const { return targetIntervalDays_; }

    // Emit signal when status changes
    CalibrationStatus currentStatus() const { return lastCheck_.status; }

signals:
    void calibrationStatusChanged(CalibrationStatus status);
    void calibrationExpired();
    void needsRecalibration();
    void checkCompleted(const CalibrationInfo& info);

private:
    MonitorCalibrationMonitor(QObject* parent = nullptr);
    ~MonitorCalibrationMonitor() = default;

    CalibrationInfo performSystemCheck();
    CalibrationInfo checkViaColorSync();
    bool isColorimetricallyAccurate();

    CalibrationInfo lastCheck_;
    CalibrationStatus previousStatus_{CalibrationStatus::Unknown};
    int targetIntervalDays_{30}; // Default: recalibrate every 30 days
    QTimer* refreshTimer_;
};

} // namespace monitor
} // namespace jcolour