#pragma once

#include <QDialog>
#include <QString>
#include <QImage>
#include <QSizeF>
#include <QPointF>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>

namespace jcolour {

struct StepRepeatConfig {
    // Layout
    int columns = 3;
    int rows = 2;
    double spacingMm = 5;
    double marginMm = 10;

    // Image
    double imageWidthMm = 100;
    double imageHeightMm = 150;
    bool maintainAspectRatio = true;

    // Options
    bool addCropMarks = true;
    double cropMarkLengthMm = 5;
    double cropMarkOffsetMm = 2;
    bool addLabels = false;
    bool addPageNumbers = false;
    int startNumber = 1;

    // Output
    QString outputPaperSize; // "A4", "Letter", "Custom"
    double customWidthMm = 297;
    double customHeightMm = 420;
};

class StepRepeatGenerator {
public:
    static QImage generate(const QImage& source,
                           const StepRepeatConfig& config);
    static QImage generateFromPath(const QString& imagePath,
                                    const StepRepeatConfig& config);
};

class StepRepeatDialog : public QDialog {
    Q_OBJECT
public:
    explicit StepRepeatDialog(QWidget* parent = nullptr);
    ~StepRepeatDialog() override;

    void setSourceImage(const QString& path);
    StepRepeatConfig getConfig() const;
    void setConfig(const StepRepeatConfig& config);

    QImage generatePreview() const;
    QString saveToFile(const QString& path) const;

signals:
    void configChanged();
    void generateRequested(const StepRepeatConfig& config);

private slots:
    void onColumnsChanged(int val);
    void onRowsChanged(int val);
    void onPaperSizeChanged(int index);
    void onBrowseOutput();

private:
    void updatePreview();

    QString sourceImagePath_;
    StepRepeatConfig config_;
    QLabel* previewLabel_;
    QSpinBox* columnsSpin_;
    QSpinBox* rowsSpin_;
    QComboBox* paperSizeCombo_;
    QDoubleSpinBox* spacingSpin_;
    QCheckBox* cropMarksCheck_;
    QCheckBox* labelsCheck_;
    QCheckBox* pageNumbersCheck_;
};

} // namespace jcolour