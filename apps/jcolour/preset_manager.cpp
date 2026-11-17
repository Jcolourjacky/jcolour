#include "preset_manager.h"
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QCoreApplication>

QList<PrintPreset> PresetManager::defaultPresets_;

PresetManager::PresetManager(QObject* parent)
    : QObject(parent)
{
    if (defaultPresets_.isEmpty()) {
        // Initialize default presets
        defaultPresets_ = getDefaultPresets();
    }
}

PresetManager::~PresetManager() = default;

QString PresetManager::presetDirectory() const {
    // Application Support (inside the sandbox container for a MAS build). This is
    // the Apple-sanctioned location for app-managed data files like presets.json;
    // QStandardPaths returns the container-relative path automatically when
    // sandboxed. Fall back to a temp-derived path only if it is unavailable.
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QDir::homePath() + "/.jcolour";
    }
    QDir dir(dataDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning("PresetManager: failed to create preset directory %s",
                 qUtf8Printable(dataDir));
    }
    return dataDir;
}

QJsonObject PresetManager::loadSettings() const {
    QFile file(presetDirectory() + "/presets.json");
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        return QJsonDocument::fromJson(data).object();
    }
    return QJsonObject();
}

void PresetManager::saveSettings(const QJsonObject& settings) {
    // Atomic write: QSaveFile writes to a temp file then renames, so an
    // interrupted/killed sandboxed process cannot corrupt presets.json.
    QSaveFile file(presetDirectory() + "/presets.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(settings).toJson());
        if (!file.commit()) {
            qWarning("PresetManager: failed to commit presets.json");
        }
    } else {
        qWarning("PresetManager: cannot open presets.json for writing");
    }
}

void PresetManager::savePreset(const PrintPreset& preset) {
    QJsonObject settings = loadSettings();
    QJsonObject presetJson;
    presetJson["name"] = preset.name;
    presetJson["paperName"] = preset.paperName;
    presetJson["iccProfilePath"] = preset.iccProfilePath;
    presetJson["inkMode"] = preset.inkMode;
    presetJson["renderingIntent"] = preset.renderingIntent;
    presetJson["oogStrategy"] = preset.oogStrategy;
    presetJson["blackPointCompensation"] = preset.blackPointCompensation;
    presetJson["inkLimit"] = preset.inkLimit;
    presetJson["lcfLimit"] = preset.lcfLimit;
    presetJson["densityAdjustment"] = preset.densityAdjustment;
    presetJson["screenFrequency"] = preset.screenFrequency;
    presetJson["halftoneAlgorithm"] = preset.halftoneAlgorithm;
    presetJson["metadata"] = preset.metadata;

    settings[preset.name] = presetJson;
    saveSettings(settings);

    emit presetSaved(preset.name);
    emit presetsChanged();
}

void PresetManager::deletePreset(const QString& name) {
    QJsonObject settings = loadSettings();
    settings.remove(name);
    saveSettings(settings);

    emit presetDeleted(name);
    emit presetsChanged();
}

QList<PrintPreset> PresetManager::getAllPresets() const {
    QList<PrintPreset> result;

    // Add default presets first
    result.append(defaultPresets_);

    // Add user presets
    QJsonObject settings = loadSettings();
    for (auto it = settings.begin(); it != settings.end(); ++it) {
        QJsonObject obj = it.value().toObject();
        PrintPreset preset;
        preset.name = obj["name"].toString();
        preset.paperName = obj["paperName"].toString();
        preset.iccProfilePath = obj["iccProfilePath"].toString();
        preset.inkMode = obj["inkMode"].toString();
        preset.renderingIntent = obj["renderingIntent"].toInt(1);
        preset.oogStrategy = obj["oogStrategy"].toInt(0);   // 旧预设无此键 → 视觉保真
        preset.blackPointCompensation = obj["blackPointCompensation"].toBool(true);
        preset.inkLimit = obj["inkLimit"].toDouble(400);
        preset.lcfLimit = obj["lcfLimit"].toDouble(200);
        preset.densityAdjustment = obj["densityAdjustment"].toDouble(0);
        preset.screenFrequency = obj["screenFrequency"].toDouble(1440);
        preset.halftoneAlgorithm = obj["halftoneAlgorithm"].toString("Stochastic");
        preset.metadata = obj["metadata"].toObject();
        result.append(preset);
    }

    return result;
}

PrintPreset PresetManager::getPreset(const QString& name) const {
    for (const auto& preset : defaultPresets_) {
        if (preset.name == name) return preset;
    }

    QJsonObject settings = loadSettings();
    QJsonObject obj = settings[name].toObject();
    if (!obj.isEmpty()) {
        PrintPreset preset;
        preset.name = obj["name"].toString();
        preset.paperName = obj["paperName"].toString();
        preset.iccProfilePath = obj["iccProfilePath"].toString();
        preset.inkMode = obj["inkMode"].toString();
        preset.renderingIntent = obj["renderingIntent"].toInt(1);
        preset.oogStrategy = obj["oogStrategy"].toInt(0);   // 旧预设无此键 → 视觉保真
        preset.blackPointCompensation = obj["blackPointCompensation"].toBool(true);
        preset.inkLimit = obj["inkLimit"].toDouble(400);
        preset.lcfLimit = obj["lcfLimit"].toDouble(200);
        preset.densityAdjustment = obj["densityAdjustment"].toDouble(0);
        preset.screenFrequency = obj["screenFrequency"].toDouble(1440);
        preset.halftoneAlgorithm = obj["halftoneAlgorithm"].toString("Stochastic");
        preset.metadata = obj["metadata"].toObject();
        return preset;
    }

    return PrintPreset();
}

QList<PrintPreset> PresetManager::getDefaultPresets() {
    QList<PrintPreset> defaults;

    // Neutral B&W
    PrintPreset neutral;
    neutral.name = "Neutral B&W";
    neutral.inkMode = "MultiChannel";
    neutral.renderingIntent = 1;
    neutral.blackPointCompensation = true;
    neutral.inkLimit = 400;
    neutral.halftoneAlgorithm = "Stochastic";
    defaults.append(neutral);

    // Selenium (Cool)
    PrintPreset selenium;
    selenium.name = "Selenium (Cool)";
    selenium.inkMode = "MultiChannel";
    selenium.renderingIntent = 1;
    selenium.blackPointCompensation = true;
    selenium.inkLimit = 380;
    selenium.halftoneAlgorithm = "Stochastic";
    defaults.append(selenium);

    // Sepia (Warm)
    PrintPreset sepia;
    sepia.name = "Sepia (Warm)";
    sepia.inkMode = "MultiChannel";
    sepia.renderingIntent = 1;
    sepia.blackPointCompensation = true;
    sepia.inkLimit = 360;
    sepia.halftoneAlgorithm = "Stochastic";
    defaults.append(sepia);

    // Split-tone Cool Highlight
    PrintPreset splitCool;
    splitCool.name = "Split-tone Cool Highlight";
    splitCool.inkMode = "MultiChannel";
    splitCool.renderingIntent = 1;
    splitCool.blackPointCompensation = true;
    splitCool.inkLimit = 400;
    splitCool.halftoneAlgorithm = "Stochastic";
    defaults.append(splitCool);

    // Split-tone Warm Highlight
    PrintPreset splitWarm;
    splitWarm.name = "Split-tone Warm Highlight";
    splitWarm.inkMode = "MultiChannel";
    splitWarm.renderingIntent = 1;
    splitWarm.blackPointCompensation = true;
    splitWarm.inkLimit = 400;
    splitWarm.halftoneAlgorithm = "Stochastic";
    defaults.append(splitWarm);

    // Pure Carbon Black (Archive)
    PrintPreset carbon;
    carbon.name = "Pure Carbon Black (Archive)";
    carbon.inkMode = "PhotoBlack";
    carbon.renderingIntent = 1;
    carbon.blackPointCompensation = true;
    carbon.inkLimit = 100; // Only black ink
    carbon.halftoneAlgorithm = "Stochastic";
    defaults.append(carbon);

    return defaults;
}