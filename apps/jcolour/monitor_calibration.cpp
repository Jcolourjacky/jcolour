#include "monitor_calibration.h"
#include <QSettings>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace jcolour {
namespace monitor {

MonitorCalibrationMonitor& MonitorCalibrationMonitor::instance() {
    static MonitorCalibrationMonitor instance;
    return instance;
}

MonitorCalibrationMonitor::MonitorCalibrationMonitor(QObject* parent)
    : QObject(parent)
{
    // Check calibration status every hour
    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &MonitorCalibrationMonitor::refresh);
    refreshTimer_->start(3600000); // 1 hour

    // Initial check
    lastCheck_ = performSystemCheck();
}

CalibrationInfo MonitorCalibrationMonitor::checkCalibration() {
    lastCheck_ = performSystemCheck();
    emit checkCompleted(lastCheck_);

    if (lastCheck_.status != previousStatus_) {
        previousStatus_ = lastCheck_.status;
        emit calibrationStatusChanged(lastCheck_.status);

        if (lastCheck_.status == CalibrationStatus::CalibrationExpired) {
            emit calibrationExpired();
        } else if (lastCheck_.status == CalibrationStatus::CalibrationExpired) {
            emit calibrationExpired();
        } else if (lastCheck_.status == CalibrationStatus::CalibrationExpired) {
            emit needsRecalibration();
        }
    }

    return lastCheck_;
}

CalibrationInfo MonitorCalibrationMonitor::performSystemCheck() {
    CalibrationInfo info;
    info.status = CalibrationStatus::Unknown;

    // Method 1: Try ColorSync / color utility
    info = checkViaColorSync();

    // Method 2: Check user defaults / settings
    if (info.status == CalibrationStatus::Unknown) {
        QSettings settings("jcolour", "monitor");
        QDateTime lastCal = settings.value("lastCalibration").toDateTime();
        QString profile = settings.value("profileName").toString();

        if (!lastCal.isNull()) {
            info.lastCalibration = lastCal;
            info.daysSinceCalibration = lastCal.daysTo(QDateTime::currentDateTime());
            info.profileName = profile;

            if (info.daysSinceCalibration > targetIntervalDays_) {
                info.status = CalibrationStatus::CalibrationExpired;
            } else if (info.daysSinceCalibration > targetIntervalDays_ * 0.8) {
                info.status = CalibrationStatus::RecentlyCalibrated;
            } else {
                info.status = CalibrationStatus::RecentlyCalibrated;
            }
        }
    }

    // Method 3: Check if system has active display profile
    if (info.status == CalibrationStatus::Unknown) {
        if (hasDisplayProfile()) {
            info.profileName = displayProfileName();
            info.status = CalibrationStatus::RecentlyCalibrated; // Assume OK if profile exists
        } else {
            info.status = CalibrationStatus::NotCalibrated;
        }
    }

    // Check colorimetric accuracy if profile exists
    if (info.status == CalibrationStatus::RecentlyCalibrated && !info.profileName.isEmpty()) {
        if (!isColorimetricallyAccurate()) {
            info.status = CalibrationStatus::OutOfGamut;
        }
    }

    // Default values for missing info
    if (info.maxLuminance == 0) info.maxLuminance = 250.0; // Typical display
    if (info.whitePointTemp == 0) info.whitePointTemp = 6500.0;
    if (info.deltaEToTarget < 0) info.deltaEToTarget = 2.0; // Default acceptable

    return info;
}

CalibrationInfo MonitorCalibrationMonitor::checkViaColorSync() {
    CalibrationInfo info;
    info.status = CalibrationStatus::Unknown;

    // Try to read system display profile via AppleScript
    QString script = R"(
        tell application "System Events"
            try
                tell application "System Preferences"
                    activate
                end tell
            end try
        end tell
    )";

    // Alternative: check ~/Library/ColorSync/Profiles
    QString colorSyncPath = QDir::homePath() + "/Library/ColorSync/Profiles";
    QDir profileDir(colorSyncPath);

    if (profileDir.exists()) {
        QStringList profiles = profileDir.entryList(QStringList() << "*.iccg" << "*.icc");
        for (const QString& profile : profiles) {
            if (profile.contains("Display") || profile.contains("Monitor")) {
                info.profileName = profile;
                info.status = CalibrationStatus::RecentlyCalibrated;

                // Try to read profile creation date
                QFileInfo fileInfo(profileDir.filePath(profile));
                info.lastCalibration = fileInfo.birthTime();
                info.daysSinceCalibration = fileInfo.birthTime().daysTo(QDateTime::currentDateTime());
                break;
            }
        }
    }

    // Check Apple ColorSync data
    QString syncDbPath = QDir::homePath() + "/Library/Application Support/Apple/System/Library/ColorSync";
    if (QDir(syncDbPath).exists()) {
        // Check for recent calibration files
    }

    // Use system_profiler for display info
    QProcess process;
    process.start("system_profiler", {"SPDisplaysDataType", "-json"});
    if (process.waitForFinished(2000)) {
        QByteArray output = process.readAllStandardOutput();
        QJsonDocument doc = QJsonDocument::fromJson(output);
        QJsonObject root = doc.object();

        if (root.contains("SPDisplaysDataType")) {
            QJsonArray displays = root["SPDisplaysDataType"].toArray();
            if (!displays.isEmpty()) {
                QJsonObject display = displays[0].toObject();
                if (display.contains("spdisplays_profile")) {
                    info.profileName = display["spdisplays_profile"].toString();
                    info.status = CalibrationStatus::RecentlyCalibrated;
                }
                if (display.contains("spdisplays_ manufacture")) {
                    info.deviceInfo = display["spdisplays_ manufacture"].toString();
                }
            }
        }
    }

    return info;
}

bool MonitorCalibrationMonitor::hasDisplayProfile() const {
    // Check if current display has a profile
    QProcess process;
    process.start("system_profiler", {"SPDisplaysDataType", "-json"});
    if (process.waitForFinished(2000)) {
        QByteArray output = process.readAllStandardOutput();
        if (output.contains("spdisplays_profile")) {
            return true;
        }
    }
    return !displayProfileName().isEmpty();
}

QString MonitorCalibrationMonitor::displayProfileName() const {
    QProcess process;
    process.start("system_profiler", {"SPDisplaysDataType", "-json"});
    if (process.waitForFinished(2000)) {
        QByteArray output = process.readAllStandardOutput();
        QJsonDocument doc = QJsonDocument::fromJson(output);
        QJsonObject root = doc.object();

        if (root.contains("SPDisplaysDataType")) {
            QJsonArray displays = root["SPDisplaysDataType"].toArray();
            if (!displays.isEmpty()) {
                QJsonObject display = displays[0].toObject();
                if (display.contains("spdisplays_profile")) {
                    return display["spdisplays_profile"].toString();
                }
            }
        }
    }
    return QString();
}

bool MonitorCalibrationMonitor::supportsCalibration() const {
    // All modern displays support calibration
    // Check if ColorSync is available
    return true;
}

void MonitorCalibrationMonitor::refresh() {
    CalibrationInfo newCheck = performSystemCheck();

    if (newCheck.status != lastCheck_.status) {
        lastCheck_ = newCheck;
        emit calibrationStatusChanged(newCheck.status);

        if (newCheck.status == CalibrationStatus::CalibrationExpired) {
            emit calibrationExpired();
        }
    }

    emit checkCompleted(newCheck);
}

void MonitorCalibrationMonitor::setTargetInterval(int days) {
    targetIntervalDays_ = qMax(7, qMin(365, days)); // 7 to 365 days
    refresh();
}

bool MonitorCalibrationMonitor::isColorimetricallyAccurate() {
    // Check if current profile is producing accurate colors
    // This is a simplified check - in production, would use more sophisticated methods

    CalibrationInfo info = lastCheck_;

    // Check days since calibration
    if (info.daysSinceCalibration > targetIntervalDays_) {
        return false;
    }

    // Check delta E to target
    if (info.deltaEToTarget > 5.0) {
        return false;
    }

    // Check white point
    if (qAbs(info.whitePointTemp - 6500) > 1000) { // More than 1000K off
        return false;
    }

    return true;
}

} // namespace monitor
} // namespace jcolour