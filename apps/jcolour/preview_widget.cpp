#include "preview_widget.h"
#include "translations.h"
#include <algorithm>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QImage>
#include <QFileInfo>
#include <QColorDialog>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QGroupBox>
#include <QFrame>
#include <QMessageBox>
#include <QFileDialog>
#include <algorithm>
#include <cmath>
#include <QtMath>

// ============================================
// SoftProofingWidget Implementation
// ============================================

SoftProofingWidget::SoftProofingWidget(QWidget* parent)
    : QWidget(parent)
    , paperWhiteL_(95.0), paperWhiteA_(0.0), paperWhiteB_(0.0)
    , deltaEThreshold_(3.0)
{
    setupUI();
}

SoftProofingWidget::~SoftProofingWidget() = default;

void SoftProofingWidget::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // Master enable checkbox
    enabledCheck_ = new QCheckBox("Enable Soft Proofing");
    enabledCheck_->setChecked(false);
    connect(enabledCheck_, &QCheckBox::toggled, this, &SoftProofingWidget::onEnabledChanged);
    mainLayout->addWidget(enabledCheck_);

    // Rendering Intent section
    auto* intentGroup = new QGroupBox("Rendering Intent");
    auto* intentLayout = new QVBoxLayout(intentGroup);

    renderingIntentCombo_ = new QComboBox;
    renderingIntentCombo_->addItems({
        "Perceptual (Recommended for photos)",
        "Relative Colorimetric (Recommended for logos)",
        "Saturation (Recommended for graphics)",
        "Absolute Colorimetric (For proof matching)"
    });
    connect(renderingIntentCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SoftProofingWidget::onRenderingIntentChanged);
    intentLayout->addWidget(renderingIntentCombo_);

    // BPC option
    bpcCheck_ = new QCheckBox("Black Point Compensation (BPC)");
    bpcCheck_->setChecked(true);
    bpcRecommendationLabel_ = new QLabel("BPC recommended for photographic images");
    bpcRecommendationLabel_->setStyleSheet("color: gray; font-size: 11px;");
    connect(bpcCheck_, &QCheckBox::toggled, this, &SoftProofingWidget::onBPCToggled);
    intentLayout->addWidget(bpcCheck_);
    intentLayout->addWidget(bpcRecommendationLabel_);

    bpcApplyBtn_ = new QPushButton("Apply BPC suggestion");
    bpcApplyBtn_->setCursor(Qt::PointingHandCursor);
    bpcApplyBtn_->setStyleSheet("QPushButton{font-size:11px;padding:3px 8px;}");
    connect(bpcApplyBtn_, &QPushButton::clicked, this, [this] {
        bpcCheck_->setChecked(recommendedBpc_);
        updateBPCVisibility();
    });
    intentLayout->addWidget(bpcApplyBtn_, 0, Qt::AlignLeft);

    mainLayout->addWidget(intentGroup);

    // Paper White Simulation
    auto* paperGroup = new QGroupBox("Paper Simulation");
    auto* paperLayout = new QVBoxLayout(paperGroup);

    paperWhiteSimCheck_ = new QCheckBox("Simulate paper white color");
    paperWhiteSimCheck_->setChecked(false);
    connect(paperWhiteSimCheck_, &QCheckBox::toggled, this, &SoftProofingWidget::onPaperWhiteSimToggled);
    paperLayout->addWidget(paperWhiteSimCheck_);

    auto* colorLayout = new QHBoxLayout;
    colorLayout->addWidget(new QLabel("Paper color:"));
    paperWhiteColorLabel_ = new QLabel("L*=95, a*=0, b*=0 (Neutral White)");
    paperWhiteColorLabel_->setStyleSheet("background: white; padding: 4px 8px; border: 1px solid #555;");
    paperWhiteColorBtn_ = new QPushButton("Choose...");
    connect(paperWhiteColorBtn_, &QPushButton::clicked, this, &SoftProofingWidget::onPaperWhiteColorClicked);
    colorLayout->addWidget(paperWhiteColorLabel_, 1);
    colorLayout->addWidget(paperWhiteColorBtn_);
    paperLayout->addLayout(colorLayout);

    // Preset paper colors
    auto* presetLayout = new QHBoxLayout;
    presetLayout->addWidget(new QLabel("Presets:"));
    QStringList presets = {"Neutral White (95)", "Warm White (94)", "Cool White (96)", "Off-White (90)"};
    for (const QString& preset : presets) {
        auto* btn = new QPushButton(preset.split(" ").first());
        btn->setFixedWidth(60);
        connect(btn, &QPushButton::clicked, this, [this, preset]() {
            if (preset.contains("Neutral")) { paperWhiteL_ = 95; paperWhiteA_ = 0; paperWhiteB_ = 0; }
            else if (preset.contains("Warm")) { paperWhiteL_ = 94; paperWhiteA_ = 1; paperWhiteB_ = 8; }
            else if (preset.contains("Cool")) { paperWhiteL_ = 96; paperWhiteA_ = -1; paperWhiteB_ = -3; }
            else if (preset.contains("Off")) { paperWhiteL_ = 90; paperWhiteA_ = 0; paperWhiteB_ = 5; }
            updatePaperWhiteLabel();
            emit softProofSettingsChanged();
        });
        presetLayout->addWidget(btn);
    }
    presetLayout->addStretch();
    paperLayout->addLayout(presetLayout);

    mainLayout->addWidget(paperGroup);

    // Accuracy / Delta E section
    accuracyGroup_ = new QGroupBox("Gamut Warning");
    auto* accuracyLayout = new QVBoxLayout(accuracyGroup_);

    auto* thresholdLayout = new QHBoxLayout;
    thresholdLayout->addWidget(new QLabel("ΔE Threshold:"));
    deltaEThresholdSlider_ = new QSlider(Qt::Horizontal);
    deltaEThresholdSlider_->setRange(1, 10);
    deltaEThresholdSlider_->setValue(3);
    deltaEThresholdSlider_->setTickPosition(QSlider::TicksBelow);
    deltaEThresholdSlider_->setTickInterval(1);
    connect(deltaEThresholdSlider_, &QSlider::valueChanged, this, &SoftProofingWidget::onDeltaEThresholdChanged);
    thresholdLayout->addWidget(deltaEThresholdSlider_);
    deltaEThresholdLabel_ = new QLabel("3.0");
    deltaEThresholdLabel_->setFixedWidth(40);
    thresholdLayout->addWidget(deltaEThresholdLabel_);
    accuracyLayout->addLayout(thresholdLayout);

    // Delta E recommendation
    deltaERecommendationLabel_ = new QLabel(
        "ΔE > 3: significant color difference\n"
        "ΔE 1-3: noticeable to trained eye\n"
        "ΔE < 1: imperceptible"
    );
    deltaERecommendationLabel_->setStyleSheet("color: gray; font-size: 11px; padding: 4px;");
    accuracyLayout->addWidget(deltaERecommendationLabel_);

    // Heatmap toggle
    deltaEHeatmapCheck_ = new QCheckBox("Show ΔE heatmap overlay");
    connect(deltaEHeatmapCheck_, &QCheckBox::toggled, this, &SoftProofingWidget::onDeltaEHeatmapToggled);
    accuracyLayout->addWidget(deltaEHeatmapCheck_);

    mainLayout->addWidget(accuracyGroup_);

    // Adaptation state
    auto* adaptLayout = new QHBoxLayout;
    adaptLayout->addWidget(new QLabel("Adaptation:"));
    adaptationSlider_ = new QSlider(Qt::Horizontal);
    adaptationSlider_->setRange(0, 100);
    adaptationSlider_->setValue(100);
    connect(adaptationSlider_, &QSlider::valueChanged, this, [this](int val) {
        adaptationLabel_->setText(QString::number(val) + "%");
        emit softProofSettingsChanged();
    });
    adaptLayout->addWidget(adaptationSlider_);
    adaptationLabel_ = new QLabel("100%");
    adaptationLabel_->setFixedWidth(50);
    adaptLayout->addWidget(adaptationLabel_);
    mainLayout->addLayout(adaptLayout);

    mainLayout->addStretch();
}

void SoftProofingWidget::onEnabledChanged(bool checked) {
    // Enable/disable child widgets
    for (auto* child : findChildren<QWidget*>()) {
        if (child != enabledCheck_) {
            child->setEnabled(checked);
        }
    }
    emit softProofSettingsChanged();
}

void SoftProofingWidget::onRenderingIntentChanged(int index) {
    updateBPCVisibility();
    emit softProofSettingsChanged();
}

void SoftProofingWidget::onBPCToggled(bool checked) {
    Q_UNUSED(checked);
    updateBPCVisibility();   // refresh "matches recommendation" state
    emit softProofSettingsChanged();
}

void SoftProofingWidget::onPaperWhiteSimToggled(bool checked) {
    paperWhiteColorBtn_->setEnabled(checked);
    emit softProofSettingsChanged();
}

void SoftProofingWidget::onPaperWhiteColorClicked() {
    // Convert Lab to approximate RGB for color picker
    // Simple conversion for demo
    QColor initial(255, 255, 245); // Warm white default
    QColor color = QColorDialog::getColor(initial, this, "Select Paper Color");
    if (color.isValid()) {
        // Convert RGB to Lab (simplified)
        double r = color.redF();
        double g = color.greenF();
        double b = color.blueF();

        // Simple Lab approximation
        paperWhiteL_ = (r + g + b) / 3.0 * 100;
        paperWhiteA_ = (r - b) * 50;
        paperWhiteB_ = (g - (r + b) / 2.0) * 50;

        updatePaperWhiteLabel();
        updateBPCVisibility();   // paper black point affects the BPC recommendation
        emit softProofSettingsChanged();
    }
}

void SoftProofingWidget::onDeltaEThresholdChanged(int value) {
    deltaEThreshold_ = value / 1.0;
    deltaEThresholdLabel_->setText(QString::number(deltaEThreshold_, 'f', 1));
    emit softProofSettingsChanged();
}

void SoftProofingWidget::onDeltaEHeatmapToggled(bool checked) {
    Q_UNUSED(checked);
    emit softProofSettingsChanged();
}

void SoftProofingWidget::updateBPCVisibility() {
    const int intent = renderingIntentCombo_->currentIndex();

    // Absolute colorimetric: BPC is meaningless (paper black is reproduced as-is).
    if (intent == 3) {
        recommendedBpc_ = false;
        bpcCheck_->setChecked(false);
        bpcCheck_->setEnabled(false);
        if (bpcApplyBtn_) bpcApplyBtn_->setVisible(false);
        bpcRecommendationLabel_->setText(TR("BPC not applicable for Absolute"));
        bpcRecommendationLabel_->setStyleSheet("color: gray; font-size: 11px;");
        return;
    }
    bpcCheck_->setEnabled(true);
    if (bpcApplyBtn_) bpcApplyBtn_->setVisible(true);

    // Data-driven decision: intent + paper black point (proxied by paper white L*,
    // lower = matte/cotton with weaker Dmax) + image shadow content.
    const bool matte       = paperWhiteL_ < 92.0;
    const bool shadowHeavy = shadowRatio_ >= 0.0 && shadowRatio_ > 0.12;

    QStringList reasons;
    if (intent == 0 || intent == 1) {       // Perceptual / Relative
        recommendedBpc_ = true;
        reasons << TR("preserves shadow detail into paper black");
        if (matte)       reasons << TR("matte paper (low Dmax)");
        if (shadowHeavy) reasons << QString("%1 %2%")
                                    .arg(TR("deep shadows")).arg(shadowRatio_ * 100.0, 0, 'f', 0);
    } else {                                 // Saturation
        recommendedBpc_ = matte || shadowHeavy;
        reasons << TR("optional for Saturation");
        if (matte)       reasons << TR("matte paper favours BPC");
        if (shadowHeavy) reasons << TR("deep shadows favour BPC");
    }

    const bool matchesUser = (bpcCheck_->isChecked() == recommendedBpc_);
    const QString verb = recommendedBpc_ ? TR("Enable BPC") : TR("Disable BPC");
    bpcRecommendationLabel_->setText(
        QString("%1 — %2").arg(matchesUser ? TR("BPC setting matches recommendation") : verb,
                               reasons.join(" · ")));
    bpcRecommendationLabel_->setStyleSheet(
        matchesUser ? "color: #2e8b57; font-size: 11px;"     // sea green: aligned
                    : "color: #c47f1a; font-size: 11px;");   // amber: action suggested
    if (bpcApplyBtn_) bpcApplyBtn_->setEnabled(!matchesUser);
}

void SoftProofingWidget::setShadowRatio(double ratio) {
    shadowRatio_ = ratio;
    updateBPCVisibility();
}

void SoftProofingWidget::updateIntentRecommendation() {
    // Analyze image content and recommend rendering intent
    // This is called when image properties change
    if (renderingIntentCombo_) {
        // Default recommendation based on image characteristics
        // For now, keep user's selection
    }
}

void SoftProofingWidget::updatePaperWhiteLabel() {
    paperWhiteColorLabel_->setText(
        QString("L*=%1, a*=%2, b*=%3").arg(paperWhiteL_, 0, 'f', 1)
            .arg(paperWhiteA_, 0, 'f', 1).arg(paperWhiteB_, 0, 'f', 1)
    );
    // Visual indicator
    int gray = static_cast<int>(paperWhiteL_ * 2.55);
    paperWhiteColorLabel_->setStyleSheet(
        QString("background: rgb(%1,%1,%2); padding: 4px 8px; border: 1px solid #555;")
            .arg(gray).arg(qMin(255, gray + 10))
    );
}

jcolour::RenderingIntent SoftProofingWidget::getRenderingIntent() const {
    return static_cast<jcolour::RenderingIntent>(renderingIntentCombo_->currentIndex());
}

void SoftProofingWidget::setRenderingIntent(jcolour::RenderingIntent intent) {
    renderingIntentCombo_->setCurrentIndex(static_cast<int>(intent));
}

// ============================================
// ImagePreviewWidget Implementation
// ============================================

ImagePreviewWidget::ImagePreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(400, 300);
    setAcceptDrops(true);
    setStyleSheet("background-color: #7a7a7a;");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Status bar with controls
    auto* statusLayout = new QHBoxLayout;
    statusLayout->setSpacing(8);

    statusLabel_ = new QLabel("Drop image or click Open");
    statusLabel_->setStyleSheet("color: gray; padding: 4px;");
    statusLayout->addWidget(statusLabel_, 1);

    deltaELabel_ = new QLabel("");
    deltaELabel_->setStyleSheet("color: #ff9800; padding: 4px;");
    deltaELabel_->setVisible(false);
    statusLayout->addWidget(deltaELabel_);

    softProofBtn_ = new QPushButton("Soft Proof");
    softProofBtn_->setCheckable(true);
    softProofBtn_->setEnabled(false);
    connect(softProofBtn_, &QPushButton::toggled, this, &ImagePreviewWidget::onToggleSoftProof);
    statusLayout->addWidget(softProofBtn_);

    deltaEBtn_ = new QPushButton("ΔE");
    deltaEBtn_->setCheckable(true);
    deltaEBtn_->setEnabled(false);
    connect(deltaEBtn_, &QPushButton::toggled, this, &ImagePreviewWidget::onToggleDeltaEHeatmap);
    statusLayout->addWidget(deltaEBtn_);

    mainLayout->addLayout(statusLayout);
}

ImagePreviewWidget::~ImagePreviewWidget() = default;

void ImagePreviewWidget::loadImage(const QString& path) {
    if (!QFileInfo::exists(path)) return;

    currentImagePath_ = path;
    sourceImage_.load(path);

    if (!sourceImage_.isNull()) {
        int bitDepth = sourceImage_.depth();
        statusLabel_->setText(QString("%1 — %2×%3 %4-bit")
            .arg(QFileInfo(path).fileName())
            .arg(sourceImage_.width())
            .arg(sourceImage_.height())
            .arg(bitDepth));
        statusLabel_->setStyleSheet("color: #1f1f1f; padding: 4px;");

        softProofBtn_->setEnabled(true);
        deltaEBtn_->setEnabled(true);

        // Compute dark-pixel ratio (luma < 0.18) to drive the BPC advisor.
        if (softProofingWidget_) {
            QImage s = sourceImage_.convertToFormat(QImage::Format_RGB32);
            qint64 dark = 0, tot = static_cast<qint64>(s.width()) * s.height();
            const int step = std::max(1, s.height() / 256);   // subsample tall images
            qint64 sampled = 0;
            for (int y = 0; y < s.height(); y += step) {
                const auto* ln = reinterpret_cast<const QRgb*>(s.scanLine(y));
                for (int x = 0; x < s.width(); ++x) {
                    double luma = (0.2126 * qRed(ln[x]) + 0.7152 * qGreen(ln[x])
                                 + 0.0722 * qBlue(ln[x])) / 255.0;
                    if (luma < 0.18) ++dark;
                    ++sampled;
                }
            }
            Q_UNUSED(tot);
            softProofingWidget_->setShadowRatio(sampled > 0 ? double(dark) / sampled : -1.0);
        }

        needsRerender_ = true;
        needsDeltaERerender_ = true;
        renderPreview();
        update();
        emit imageLoaded(path);
    }
}

void ImagePreviewWidget::setDeltaEWarning(double maxDE, double avgDE) {
    if (maxDE > 3.0) {
        deltaELabel_->setText(QString("⚠ ΔE max: %1 avg: %2").arg(maxDE, 0, 'f', 1).arg(avgDE, 0, 'f', 1));
        deltaELabel_->setStyleSheet("color: #f44336; padding: 4px; font-weight: bold;");
    } else if (maxDE > 1.0) {
        deltaELabel_->setText(QString("Δ max: %1 avg: %2").arg(maxDE, 0, 'f', 1).arg(avgDE, 0, 'f', 1));
        deltaELabel_->setStyleSheet("color: #ff9800; padding: 4px;");
    } else {
        deltaELabel_->setText(QString("✓ ΔE max: %1").arg(maxDE, 0, 'f', 1));
        deltaELabel_->setStyleSheet("color: #4caf50; padding: 4px;");
    }
    deltaELabel_->setVisible(true);
}

void ImagePreviewWidget::updateGrayScale(int grayLevels) {
    if (!sourceImage_.isNull()) {
        QImage grayPreview(sourceImage_.size(), QImage::Format_RGB32);
        grayPreview.fill(Qt::white);
        QPainter p(&grayPreview);
        int levels = grayLevels > 0 ? grayLevels : 64;
        for (int y = 0; y < sourceImage_.height(); ++y) {
            for (int x = 0; x < sourceImage_.width(); ++x) {
                QRgb pixel = sourceImage_.pixel(x, y);
                int gray = qGray(pixel);
                gray = (gray / (256 / levels)) * (256 / levels);
                p.setPen(QColor(gray, gray, gray));
                p.drawPoint(x, y);
            }
        }
        p.end();
        displayImage_ = grayPreview.convertToFormat(QImage::Format_RGB16);
        needsRerender_ = false;
        update();
    }
}

void ImagePreviewWidget::onToggleSoftProof(bool enabled) {
    if (!softProofingWidget_) {
        softProofBtn_->setChecked(false);
        QMessageBox::information(this, "No Soft Proofing Widget",
            "Please set a SoftProofingWidget first.");
        return;
    }

    if (enabled) {
        // Sync with soft proofing widget
        softProofingWidget_->setEnabled(true);
    }

    needsRerender_ = true;
    renderPreview();
    update();
}

void ImagePreviewWidget::onToggleDeltaEHeatmap(bool enabled) {
    Q_UNUSED(enabled);
    needsDeltaERerender_ = true;
    if (!deltaEHeatmap_.isNull()) {
        update();
    }
}

void ImagePreviewWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        QUrl url = event->mimeData()->urls().first();
        QString ext = QFileInfo(url.toLocalFile()).suffix().toLower();
        if (ext == "tif" || ext == "tiff" || ext == "jpg" || ext == "jpeg" || ext == "png") {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void ImagePreviewWidget::dropEvent(QDropEvent* event) {
    QString path = event->mimeData()->urls().first().toLocalFile();
    loadImage(path);
}

void ImagePreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    needsRerender_ = true;
    needsDeltaERerender_ = true;
}

void ImagePreviewWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    // Checkerboard canvas (matches design/ui-mockup.html .canvas).
    const int t = 22;
    const QColor c1(0xdf, 0xe1, 0xe4), c2(0xe9, 0xeb, 0xee);
    for (int yy = 0; yy < height(); yy += t)
        for (int xx = 0; xx < width(); xx += t)
            painter.fillRect(xx, yy, t, t, (((xx / t) + (yy / t)) & 1) ? c1 : c2);

    if (!displayImage_.isNull()) {
        // Draw display image scaled to fit
        QImage scaled = displayImage_.scaled(width(), height(),
                                            Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation);
        int x = (width() - scaled.width()) / 2;
        int y = (height() - scaled.height()) / 2;
        // Soft drop shadow so the sheet appears to float.
        painter.setPen(Qt::NoPen);
        for (int s = 8; s >= 1; --s) {
            painter.setBrush(QColor(0, 0, 0, 5));
            painter.drawRect(x - s, y - s + 3, scaled.width() + 2 * s, scaled.height() + 2 * s);
        }
        painter.drawImage(x, y, scaled);

        // Overlay Delta E heatmap if enabled
        if (!deltaEHeatmap_.isNull() && softProofBtn_->isChecked() &&
            softProofingWidget_ && softProofingWidget_->isDeltaEHeatmapEnabled()) {
            painter.setOpacity(0.5);
            painter.drawImage(x, y, deltaEHeatmap_.scaled(scaled.size(),
                                                           Qt::IgnoreAspectRatio,
                                                           Qt::SmoothTransformation));
            painter.setOpacity(1.0);
        }

        // Draw paper white border simulation
        if (softProofingWidget_ && softProofingWidget_->isEnabled() &&
            softProofingWidget_->isPaperWhiteSimulationEnabled()) {
            // Simulated paper background around image
            QColor paperColor;
            paperColor.setHsl(50, 5, 92); // Neutral white
            painter.fillRect(0, 0, width(), y, paperColor);
            painter.fillRect(0, y + scaled.height(), width(), height() - y - scaled.height(), paperColor);
            painter.fillRect(0, y, x, scaled.height(), paperColor);
            painter.fillRect(x + scaled.width(), y, width() - x - scaled.width(), scaled.height(), paperColor);
        }
    } else {
        // Placeholder text
        painter.setPen(QColor(80, 80, 80));
        QFont f = painter.font();
        f.setPointSize(14);
        painter.setFont(f);
        painter.drawText(rect(), Qt::AlignCenter,
                         "Drop 16-bit TIFF/PNG here\nor click Open");
    }
}

void ImagePreviewWidget::renderPreview() {
    if (sourceImage_.isNull()) {
        displayImage_ = QImage();
        return;
    }

    // Convert to display format
    QImage::Format targetFormat = (sourceImage_.depth() > 8)
        ? QImage::Format_RGB16 : QImage::Format_ARGB32;
    displayImage_ = sourceImage_.convertToFormat(targetFormat);

    // Apply soft proofing if enabled
    if (softProofBtn_->isChecked() && softProofingWidget_ &&
        softProofingWidget_->isEnabled()) {

        // Apply paper white simulation
        if (softProofingWidget_->isPaperWhiteSimulationEnabled()) {
            applyPaperWhiteSimulation(displayImage_);
        }

        // Apply rendering intent preview (simplified)
        // In production, this would call the color engine
    }

    needsRerender_ = false;
}

void ImagePreviewWidget::renderDeltaEHeatmap() {
    if (sourceImage_.isNull() || !softProofingWidget_) {
        deltaEHeatmap_ = QImage();
        return;
    }

    double threshold = softProofingWidget_->getDeltaEThreshold();
    int w = sourceImage_.width();
    int h = sourceImage_.height();

    deltaEHeatmap_ = QImage(w, h, QImage::Format_ARGB32);
    deltaEHeatmap_.fill(Qt::transparent);

    // Generate heatmap based on simulated color differences
    // In production, this would compare with actual ICC transform
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            QRgb pixel = sourceImage_.pixel(x, y);
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);

            // Simulate gamut warning based on saturation
            double sat = std::sqrt((r - g) * (r - g) + (g - b) * (g - b) + (b - r) * (b - r)) / 441.0;
            double simulatedDE = sat * threshold * 2;

            if (simulatedDE > threshold) {
                // Red for out of gamut
                int alpha = std::min(200, static_cast<int>((simulatedDE - threshold) * 50));
                deltaEHeatmap_.setPixel(x, y, qRgba(255, 0, 0, alpha));
            } else if (simulatedDE > threshold * 0.5) {
                // Yellow for near gamut edge
                deltaEHeatmap_.setPixel(x, y, qRgba(255, 255, 0, 100));
            }
        }
    }
}

void ImagePreviewWidget::applyPaperWhiteSimulation(QImage& image) {
    if (image.isNull()) return;

    double L = softProofingWidget_->getPaperWhiteL();
    double a = softProofingWidget_->getPaperWhiteA();
    double b = softProofingWidget_->getPaperWhiteB();

    // Blend image with paper color
    QImage result = image.copy();
    QPainter painter(&result);
    painter.setOpacity(0.1);
    painter.fillRect(result.rect(), QColor(
        static_cast<int>(L * 2.55),
        static_cast<int>((127 + a) * 1.0),
        static_cast<int>((127 + b) * 1.0)
    ));
    painter.end();
    image = result;
}

// ============================================
// PrintPreviewDialog Implementation
// ============================================

PrintPreviewDialog::PrintPreviewDialog(QWidget* parent)
    : QWidget(parent)
    , paperWidthMm_(297), paperHeightMm_(420)
{
    margins_[0] = margins_[1] = margins_[2] = margins_[3] = 10.0;

    setWindowTitle("Print Preview — Test Print Options");
    setMinimumSize(800, 1000);

    setStyleSheet("background-color: #7a7a7a;");
}

PrintPreviewDialog::~PrintPreviewDialog() = default;

void PrintPreviewDialog::setImage(const QImage& image) {
    sourceImage_ = image;
    update();
}

void PrintPreviewDialog::setPaperSize(double widthMm, double heightMm) {
    paperWidthMm_ = widthMm;
    paperHeightMm_ = heightMm;
    update();
}

void PrintPreviewDialog::setMargins(double top, double right, double bottom, double left) {
    margins_[0] = top;
    margins_[1] = right;
    margins_[2] = bottom;
    margins_[3] = left;
    update();
}

void PrintPreviewDialog::setTestPrintMode(TestPrintMode mode) {
    testPrintMode_ = mode;
    update();
}

QImage PrintPreviewDialog::getPrintImage() const {
    if (sourceImage_.isNull()) return QImage();

    QImage result = sourceImage_;

    switch (testPrintMode_) {
        case TestPrintMode::ScaledDown: {
            // Scale to fit within 100mm x 100mm for quick test
            double scale = qMin(100.0 / sourceImage_.width(), 100.0 / sourceImage_.height());
            result = sourceImage_.scaled(
                static_cast<int>(sourceImage_.width() * scale),
                static_cast<int>(sourceImage_.height() * scale),
                Qt::KeepAspectRatio, Qt::SmoothTransformation
            );
            break;
        }
        case TestPrintMode::OneToOne: {
            // 1:1 pixel mapping — no scaling, show pixel boundaries
            // This would be actual 1:1 print
            break;
        }
        case TestPrintMode::SamePaper: {
            // Check if selected paper can fit the image
            // If not, scale proportionally
            break;
        }
        case TestPrintMode::Full:
        default:
            break;
    }

    return result;
}

void PrintPreviewDialog::addAvailablePaper(const QString& name, double widthMm, double heightMm) {
    availablePapers_ << name;
    Q_UNUSED(widthMm);
    Q_UNUSED(heightMm);
}

void PrintPreviewDialog::clearAvailablePapers() {
    availablePapers_.clear();
}