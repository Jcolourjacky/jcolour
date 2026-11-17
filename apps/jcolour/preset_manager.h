#pragma once

#include <QWidget>
#include <QString>
#include <QJsonObject>

struct PrintPreset {
    QString name;
    QString paperName;
    QString iccProfilePath;
    QString inkMode; // "PhotoBlack", "MatteBlack", "MultiChannel"
    int renderingIntent; // 0=Perceptual, 1=Relative, 2=Saturation, 3=Absolute
    int oogStrategy = 0; // 色域外处理：0 视觉保真（默认）/ 1 色度最近 / 2 ICC 表
    bool blackPointCompensation;
    double inkLimit; // Total ink limit percentage
    double lcfLimit; // Light cyan/magenta limit
    double densityAdjustment;
    double screenFrequency;
    QString halftoneAlgorithm;
    QJsonObject metadata;
};

class PresetManager : public QObject {
    Q_OBJECT
public:
    explicit PresetManager(QObject* parent = nullptr);
    ~PresetManager() override;

    // Preset management
    void savePreset(const PrintPreset& preset);
    void deletePreset(const QString& name);
    QList<PrintPreset> getAllPresets() const;
    PrintPreset getPreset(const QString& name) const;

    // Built-in presets
    static QList<PrintPreset> getDefaultPresets();

signals:
    void presetSaved(const QString& name);
    void presetDeleted(const QString& name);
    void presetsChanged();

private:
    QString presetDirectory() const;
    QJsonObject loadSettings() const;
    void saveSettings(const QJsonObject& settings);

    static QList<PrintPreset> defaultPresets_;
};