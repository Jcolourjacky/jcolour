#pragma once

#include <QWidget>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QGroupBox>
#include <QStackedWidget>
#include <QFileDialog>

namespace jcolour {
enum class RenderingIntent {
    Perceptual = 0,
    Relative = 1,
    Saturation = 2,
    Absolute = 3
};
}

class SoftProofingWidget : public QWidget {
    Q_OBJECT
public:
    explicit SoftProofingWidget(QWidget* parent = nullptr);
    ~SoftProofingWidget() override;

    bool isEnabled() const { return enabledCheck_->isChecked(); }
    void setEnabled(bool enabled) { enabledCheck_->setChecked(enabled); }

    jcolour::RenderingIntent getRenderingIntent() const;
    void setRenderingIntent(jcolour::RenderingIntent intent);

    bool isBPCEnabled() const { return bpcCheck_->isChecked(); }
    bool isPaperWhiteSimulationEnabled() const { return paperWhiteSimCheck_->isChecked(); }

    double getPaperWhiteL() const { return paperWhiteL_; }
    double getPaperWhiteA() const { return paperWhiteA_; }
    double getPaperWhiteB() const { return paperWhiteB_; }

    double getDeltaEThreshold() const { return deltaEThreshold_; }
    bool isDeltaEHeatmapEnabled() const { return deltaEHeatmapCheck_->isChecked(); }

    double getAdaptationState() const { return adaptationSlider_->value() / 100.0; }

    // Feed image dark-pixel ratio (0..1) so the BPC advisor can factor in
    // shadow content. <0 = unknown.
    void setShadowRatio(double ratio);

signals:
    void softProofSettingsChanged();
    void deltaEWarning(double maxDE, double avgDE);

private slots:
    void onEnabledChanged(bool checked);
    void onRenderingIntentChanged(int index);
    void onBPCToggled(bool checked);
    void onPaperWhiteSimToggled(bool checked);
    void onPaperWhiteColorClicked();
    void onDeltaEThresholdChanged(int value);
    void onDeltaEHeatmapToggled(bool checked);
    void updateIntentRecommendation();

private:
    void setupUI();
    void updateBPCVisibility();
    void updatePaperWhiteLabel();
    void applyPaperWhiteSimulation(QImage& image);

    QCheckBox* enabledCheck_;
    QComboBox* renderingIntentCombo_;
    QCheckBox* bpcCheck_;
    QLabel* bpcRecommendationLabel_;
    QPushButton* bpcApplyBtn_{nullptr};
    double shadowRatio_{-1.0};   // dark-pixel fraction of current image; <0 = unknown
    bool recommendedBpc_{true};  // last computed BPC recommendation

    QCheckBox* paperWhiteSimCheck_;
    QLabel* paperWhiteColorLabel_;
    QPushButton* paperWhiteColorBtn_;
    double paperWhiteL_{95.0}, paperWhiteA_{0.0}, paperWhiteB_{0.0};

    QGroupBox* accuracyGroup_;
    QSlider* deltaEThresholdSlider_;
    QLabel* deltaEThresholdLabel_;
    QCheckBox* deltaEHeatmapCheck_;
    QLabel* deltaERecommendationLabel_;
    double deltaEThreshold_{2.0};

    QSlider* adaptationSlider_;
    QLabel* adaptationLabel_;
};

class ImagePreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit ImagePreviewWidget(QWidget* parent = nullptr);
    ~ImagePreviewWidget() override;

    void loadImage(const QString& path);
    QString currentImagePath() const { return currentImagePath_; }

    void setSoftProofingWidget(SoftProofingWidget* widget) { softProofingWidget_ = widget; }

    void setDeltaEWarning(double maxDE, double avgDE);
    void updateGrayScale(int grayLevels);

signals:
    void imageLoaded(const QString& path);

private slots:
    void onToggleSoftProof(bool enabled);
    void onToggleDeltaEHeatmap(bool enabled);

private:
    void renderPreview();
    void renderDeltaEHeatmap();
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void applyPaperWhiteSimulation(QImage& image);

    QString currentImagePath_;
    QImage sourceImage_;
    QImage displayImage_;
    QImage deltaEHeatmap_;
    bool needsRerender_{true};
    bool needsDeltaERerender_{true};

    SoftProofingWidget* softProofingWidget_{nullptr};

    QLabel* statusLabel_;
    QLabel* deltaELabel_;
    QPushButton* softProofBtn_;
    QPushButton* deltaEBtn_;
};

class PrintPreviewDialog : public QWidget {
    Q_OBJECT
public:
    explicit PrintPreviewDialog(QWidget* parent = nullptr);
    ~PrintPreviewDialog() override;

    void setImage(const QImage& image);
    void setPaperSize(double widthMm, double heightMm);
    void setMargins(double top, double right, double bottom, double left);
    void setIncludeBleed(bool include) { includeBleed_ = include; }
    void setBleedAmount(double mm) { bleedMm_ = mm; }

    QImage getPrintImage() const;

    // Test print modes
    enum class TestPrintMode {
        Full,
        ScaledDown,
        OneToOne,
        SamePaper
    };
    void setTestPrintMode(TestPrintMode mode);
    TestPrintMode getTestPrintMode() const { return testPrintMode_; }

    // Paper pool for same-paper mode
    void addAvailablePaper(const QString& name, double widthMm, double heightMm);
    void clearAvailablePapers();
    QString selectedPaper() const { return selectedPaper_; }

signals:
    void printRequested();
    void cancelled();

private:
    QImage sourceImage_;
    double paperWidthMm_{297}, paperHeightMm_{420};
    double margins_[4]{10, 10, 10, 10};
    bool includeBleed_{false};
    double bleedMm_{3};
    TestPrintMode testPrintMode_{TestPrintMode::Full};
    QStringList availablePapers_;
    QString selectedPaper_;
};