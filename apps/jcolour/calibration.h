#pragma once

#include <QWidget>
#include <QVector>
#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QDate>

namespace jcolour {
namespace calibration {

struct G7Target {
    int patches;
    QVector<double> expected_L;
    QVector<double> expected_a;
    QVector<double> expected_b;
};

struct CalibrationResult {
    bool success;
    QString message;
    double max_deviation;
    double avg_deviation;
    QVector<double> correction_curves;
};

enum class CalibratorState {
    Idle,
    WaitingForTarget,
    Measuring,
    Processing,
    Complete,
    Error
};

}
}

class G7CalibrationWizard : public QDialog {
    Q_OBJECT
public:
    explicit G7CalibrationWizard(QWidget* parent = nullptr);
    ~G7CalibrationWizard() override;

    jcolour::calibration::CalibrationResult getResult() const;

signals:
    void calibrationStarted();
    void calibrationProgress(int percent);
    void calibrationComplete();
    void calibrationFailed(const QString& error);

private slots:
    void onNextStep();
    void onPreviousStep();
    void onMeasurePatch();
    void onFinish();

private:
    void setupUI();
    void setupSteps();
    void updateStepDisplay();
    void runAutoAlignment();
    void applyG7Compensation();

    enum Step {
        Step_Intro,
        Step_SelectTarget,
        Step_LoadPaper,
        Step_AutoAlign,
        Step_Measure,
        Step_Review,
        Step_Apply
    };

    Step currentStep_;
    QVector<QWidget*> stepWidgets_;
    QLabel* statusLabel_;
    double maxDeviation_{0.0};

    jcolour::calibration::G7Target currentTarget_;
    QVector<jcolour::calibration::CalibrationResult> measurementResults_;
    bool measurementsComplete_;
};

class GrayBalanceCalibrator : public QWidget {
    Q_OBJECT
public:
    explicit GrayBalanceCalibrator(QWidget* parent = nullptr);
    ~GrayBalanceCalibrator() override;

    void setTargetPatches(int count);
    QVector<double> getGrayCurves() const;

signals:
    void calibrationNeeded(bool needed);

private slots:
    void onMeasureGrayRamp();
    void onImportFromFile();

private:
    void calculateGrayDeviation();
    void generateCorrectionCurves();

    int targetPatches_;
    QVector<double> measuredGray_;
    QVector<double> correctionCurves_;
    double maxDeviation_;
};

class InkCartridgeTracker : public QWidget {
    Q_OBJECT
public:
    explicit InkCartridgeTracker(QWidget* parent = nullptr);
    ~InkCartridgeTracker() override;

    struct CartridgeInfo {
        QString inkType;
        QString batchNumber;
        QDate installDate;
        int fillPercent;
        bool isLow();
        bool isExpired();
    };

    void addCartridge(const CartridgeInfo& info);
    void updateCartridgeLevel(const QString& inkType, int fillPercent);
    QVector<CartridgeInfo> getAllCartridges() const;
    void checkDrift();

signals:
    void lowInkWarning(const QString& inkType);
    void driftWarning(const QString& inkType, double deltaE);

private:
    void updateDisplay();
    void checkForDrift(const QString& inkType);

    QVector<CartridgeInfo> cartridges_;
    QDate lastCalibrationDate_;
};

class TestPrintGenerator : public QWidget {
    Q_OBJECT
public:
    explicit TestPrintGenerator(QWidget* parent = nullptr);
    ~TestPrintGenerator() override;

    enum class TestType {
        GrayRamp,
        ColorChecker,
        GrayCard,
        SaturationWedge,
        ResolutionChart,
        InkDensity,
        PrintQualityCheck,  // GMG-style 列印品質 + 走紙(米尺) 測試圖
        Custom
    };

    void generateTestPrint(TestType type);
    QImage getTestImage() const;

signals:
    void testPrintGenerated(const QImage& image);

private:
    void drawGrayRamp(QPainter* painter, const QSize& size);
    void drawColorChecker(QPainter* painter, const QSize& size, int rows, int cols);
    void drawSaturationWedge(QPainter* painter, const QSize& size);
    void drawInkDensityPatches(QPainter* painter, const QSize& size);
    void drawPrintQualityCheck(QPainter* painter, const QSize& size);

    TestType currentType_;
    QImage generatedImage_;
};
