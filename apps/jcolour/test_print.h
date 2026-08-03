#pragma once

#include <QDialog>
#include <QWidget>
#include <QString>
#include <QImage>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>

namespace jcolour {

struct TestPrintConfig {
    enum class Mode {
        Full,           // Full size print
        ScaledDown,     // Scaled to fit in test area
        OneToOne,       // 1:1 pixel mapping
        SamePaper       // Same paper type as available
    };
    Mode mode = Mode::Full;

    double scaledWidthMm = 100;  // For ScaledDown mode
    double scaledHeightMm = 100;
    bool includeBleed = false;
    double bleedMm = 3.0;

    // Same paper mode
    QString selectedPaper;
    bool autoRotate = true;
};

struct PaperStock {
    QString name;
    QString vendor;
    double widthMm;
    double heightMm;
    bool isAvailable = true;
};

class TestPrintDialog : public QDialog {
    Q_OBJECT
public:
    explicit TestPrintDialog(QWidget* parent = nullptr);
    ~TestPrintDialog() override;

    void setSourceImage(const QImage& image);
    void setImagePath(const QString& path);
    void addAvailablePaper(const PaperStock& paper);
    void clearAvailablePapers();

    TestPrintConfig getConfig() const;

signals:
    void testPrintRequested(const TestPrintConfig& config);

private slots:
    void onModeChanged(int index);
    void updatePreview();
    void onPrint();

private:
    void setupUI();
    void updateOneToOneInfo();

    QImage sourceImage_;
    QString sourceImagePath_;

    TestPrintConfig::Mode currentMode_ = TestPrintConfig::Mode::Full;
    QWidget* scaledOptions_;
    QSpinBox* widthSpin_;
    QSpinBox* heightSpin_;
    QCheckBox* bleedCheck_;
    QSpinBox* bleedSpin_;
    QCheckBox* autoRotateCheck_;
    QComboBox* paperCombo_;
    QLabel* previewLabel_;
    QLabel* infoLabel_;
};

} // namespace jcolour