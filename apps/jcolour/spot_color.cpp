#include "spot_color.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QFileDialog>
#include <QPainter>
#include <QMouseEvent>
#include <QColorDialog>
#include <QSlider>
#include <QListWidget>
#include <QToolButton>
#include <algorithm>
#include <cmath>

namespace {
// CIELAB (D65) → sRGB for showing the actual spot colour in the preview.
QColor labToColor(double L, double a, double b) {
    auto finv = [](double t) {
        return t > 6.0 / 29.0 ? t * t * t : 3.0 * (6.0 / 29.0) * (6.0 / 29.0) * (t - 4.0 / 29.0);
    };
    double fy = (L + 16.0) / 116.0, fx = fy + a / 500.0, fz = fy - b / 200.0;
    double X = 0.95047 * finv(fx), Y = 1.0 * finv(fy), Z = 1.08883 * finv(fz);
    double r =  3.2406 * X - 1.5372 * Y - 0.4986 * Z;
    double g = -0.9689 * X + 1.8758 * Y + 0.0415 * Z;
    double bl =  0.0557 * X - 0.2040 * Y + 1.0570 * Z;
    auto enc = [](double c) {
        c = std::clamp(c, 0.0, 1.0);
        return c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
    };
    return QColor::fromRgbF(enc(r), enc(g), enc(bl));
}
}  // namespace

// ============================================
// InfluenceRadiusSlider Implementation
// ============================================

InfluenceRadiusSlider::InfluenceRadiusSlider(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    slider_ = new QSlider(Qt::Horizontal);
    slider_->setRange(1, 100);
    slider_->setValue(30);
    slider_->setTickPosition(QSlider::TicksBelow);
    slider_->setTickInterval(10);

    valueLabel_ = new QLabel("30%");
    valueLabel_->setFixedWidth(50);

    connect(slider_, &QSlider::valueChanged, this, [this](int val) {
        valueLabel_->setText(QString::number(val) + "%");
        emit valueChanged(val / 100.0);
    });

    layout->addWidget(new QLabel("Small"));
    layout->addWidget(slider_, 1);
    layout->addWidget(new QLabel("Large"));
    layout->addWidget(valueLabel_);
}

InfluenceRadiusSlider::~InfluenceRadiusSlider() = default;

// ============================================
// HoneycombSpotDialog::Impl
// ============================================

struct HoneycombSpotDialog::Impl {
    QString imagePath;
    double selected_L = 50;
    double selected_a = 0;
    double selected_b = 0;
    double influenceRadius = 0.3;
    int iterations = 3;

    // UI elements for live update
    QLabel* previewLabel = nullptr;
    QLabel* influencePreviewLabel = nullptr;
    QDoubleSpinBox* lSpin = nullptr;
    QDoubleSpinBox* aSpin = nullptr;
    QDoubleSpinBox* bSpin = nullptr;
    QSpinBox* iterSpin = nullptr;
    QLabel* colorSwatchLabel = nullptr;
};

HoneycombSpotDialog::HoneycombSpotDialog(QWidget* parent)
    : QDialog(parent)
    , pImpl_(new Impl)
{
    setWindowTitle("Spot Color Correction - Select Color");
    setMinimumSize(700, 500);
    setStyleSheet("QDialog { background-color: #e6e6e6; } "
                  "QGroupBox { color: #1f1f1f; font-weight: bold; } "
                  "QLabel { color: #ccc; }");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // === Image picker with live color preview ===
    auto* imageGroup = new QGroupBox("1. Pick Color from Image");
    auto* imageLayout = new QVBoxLayout(imageGroup);

    auto* pickerLayout = new QHBoxLayout;
    auto* picker = new ColorPickerFromImage;
    picker->setMinimumSize(400, 250);
    pickerLayout->addWidget(picker, 1);

    // Live color preview panel
    auto* previewPanel = new QVBoxLayout;
    previewPanel->addWidget(new QLabel("Selected Color:"));

    pImpl_->colorSwatchLabel = new QLabel;
    pImpl_->colorSwatchLabel->setMinimumSize(80, 80);
    pImpl_->colorSwatchLabel->setStyleSheet("background-color: rgb(128,128,128); border: 1px solid #555;");
    pImpl_->colorSwatchLabel->setAlignment(Qt::AlignCenter);
    previewPanel->addWidget(pImpl_->colorSwatchLabel);

    auto* labLabel = new QLabel("L*a*b*: --");
    labLabel->setObjectName("labLabel");
    previewPanel->addWidget(labLabel);

    auto* rgbLabel = new QLabel("RGB: --");
    rgbLabel->setObjectName("rgbLabel");
    previewPanel->addWidget(rgbLabel);

    auto* pickFromSwatchBtn = new QPushButton("Pick from Swatch...");
    previewPanel->addWidget(pickFromSwatchBtn);
    previewPanel->addStretch();

    pickerLayout->addLayout(previewPanel);
    imageLayout->addLayout(pickerLayout);

    connect(picker, &ColorPickerFromImage::pointSelected,
            this, [this, labLabel, rgbLabel](const QPoint&, double L, double a, double b) {
                pImpl_->selected_L = L;
                pImpl_->selected_a = a;
                pImpl_->selected_b = b;

                labLabel->setText(QString("L*a*b*: %1, %2, %3")
                    .arg(L, 0, 'f', 1).arg(a, 0, 'f', 1).arg(b, 0, 'f', 1));

                // Convert Lab to RGB for swatch
                double r = L / 100.0;
                double g = a / 50.0 + 0.5;
                double bVal = b / 50.0 + 0.5;
                int ri = qBound(0, static_cast<int>(r * 255), 255);
                int gi = qBound(0, static_cast<int>(g * 255), 255);
                int bi = qBound(0, static_cast<int>(bVal * 255), 255);
                pImpl_->colorSwatchLabel->setStyleSheet(
                    QString("background-color: rgb(%1,%2,%3); border: 1px solid #555;").arg(ri).arg(gi).arg(bi));
                rgbLabel->setText(QString("RGB: %1, %2, %3").arg(ri).arg(gi).arg(bi));

                // Update spin boxes
                if (pImpl_->lSpin) pImpl_->lSpin->setValue(L);
                if (pImpl_->aSpin) pImpl_->aSpin->setValue(a);
                if (pImpl_->bSpin) pImpl_->bSpin->setValue(b);
            });

    connect(pickFromSwatchBtn, &QPushButton::clicked, this, [this, labLabel, rgbLabel]() {
        QColor color = QColorDialog::getColor(Qt::gray, this, "Select Target Color");
        if (color.isValid()) {
            // Convert RGB to Lab (simplified)
            double r = color.redF();
            double g = color.greenF();
            double bVal = color.blueF();
            pImpl_->selected_L = (r + g + bVal) / 3.0 * 100;
            pImpl_->selected_a = (r - bVal) * 50;
            pImpl_->selected_b = (g - (r + bVal) / 2.0) * 50;

            labLabel->setText(QString("L*a*b*: %1, %2, %3")
                .arg(pImpl_->selected_L, 0, 'f', 1)
                .arg(pImpl_->selected_a, 0, 'f', 1)
                .arg(pImpl_->selected_b, 0, 'f', 1));
            pImpl_->colorSwatchLabel->setStyleSheet(
                QString("background-color: %1; border: 1px solid #555;").arg(color.name()));
            rgbLabel->setText(QString("RGB: %1, %2, %3").arg(color.red()).arg(color.green()).arg(color.blue()));

            if (pImpl_->lSpin) pImpl_->lSpin->setValue(pImpl_->selected_L);
            if (pImpl_->aSpin) pImpl_->aSpin->setValue(pImpl_->selected_a);
            if (pImpl_->bSpin) pImpl_->bSpin->setValue(pImpl_->selected_b);
        }
    });

    mainLayout->addWidget(imageGroup);

    // === Target color manual adjustment ===
    auto* targetGroup = new QGroupBox("2. Fine-tune Target Color (Lab)");
    auto* targetLayout = new QGridLayout(targetGroup);

    targetLayout->addWidget(new QLabel("L* (Lightness):"), 0, 0);
    pImpl_->lSpin = new QDoubleSpinBox;
    pImpl_->lSpin->setRange(0, 100);
    pImpl_->lSpin->setValue(50);
    pImpl_->lSpin->setDecimals(1);
    connect(pImpl_->lSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) { pImpl_->selected_L = val;
                                       updateInfluencePreview(pImpl_->influenceRadius); });
    targetLayout->addWidget(pImpl_->lSpin, 0, 1);

    targetLayout->addWidget(new QLabel("a* (Green-Red):"), 0, 2);
    pImpl_->aSpin = new QDoubleSpinBox;
    pImpl_->aSpin->setRange(-128, 128);
    pImpl_->aSpin->setValue(0);
    pImpl_->aSpin->setDecimals(1);
    connect(pImpl_->aSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) { pImpl_->selected_a = val;
                                       updateInfluencePreview(pImpl_->influenceRadius); });
    targetLayout->addWidget(pImpl_->aSpin, 0, 3);

    targetLayout->addWidget(new QLabel("b* (Blue-Yellow):"), 0, 4);
    pImpl_->bSpin = new QDoubleSpinBox;
    pImpl_->bSpin->setRange(-128, 128);
    pImpl_->bSpin->setValue(0);
    pImpl_->bSpin->setDecimals(1);
    connect(pImpl_->bSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) { pImpl_->selected_b = val;
                                       updateInfluencePreview(pImpl_->influenceRadius); });
    targetLayout->addWidget(pImpl_->bSpin, 0, 5);

    mainLayout->addWidget(targetGroup);

    // === Influence radius slider ===
    auto* influenceGroup = new QGroupBox("3. Set Influence Radius");
    auto* influenceLayout = new QVBoxLayout(influenceGroup);

    auto* radiusLayout = new QHBoxLayout;
    radiusLayout->addWidget(new QLabel("Gradient transition size:"));

    auto* radiusSlider = new InfluenceRadiusSlider;
    radiusSlider->setValue(0.3);
    connect(radiusSlider, &InfluenceRadiusSlider::valueChanged,
            this, [this](double val) {
                pImpl_->influenceRadius = val;
                updateInfluencePreview(val);
            });
    radiusLayout->addWidget(radiusSlider);
    influenceLayout->addLayout(radiusLayout);

    // Visual influence preview
    auto* previewLayout = new QHBoxLayout;
    previewLayout->addWidget(new QLabel("Influence preview:"));
    pImpl_->influencePreviewLabel = new QLabel;
    pImpl_->influencePreviewLabel->setMinimumSize(200, 40);
    pImpl_->influencePreviewLabel->setStyleSheet("background-color: #ffffff; border: 1px solid #555;");
    previewLayout->addWidget(pImpl_->influencePreviewLabel, 1);
    influenceLayout->addLayout(previewLayout);

    auto* infoLabel = new QLabel(
        "Small radius = sharp transition, Large radius = smooth gradient blend");
    infoLabel->setStyleSheet("color: gray; font-size: 11px;");
    influenceLayout->addWidget(infoLabel);

    // Iterations
    auto* iterLayout = new QHBoxLayout;
    iterLayout->addWidget(new QLabel("Correction iterations:"));
    pImpl_->iterSpin = new QSpinBox;
    pImpl_->iterSpin->setRange(1, 10);
    pImpl_->iterSpin->setValue(3);
    connect(pImpl_->iterSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int val) { pImpl_->iterations = val; });
    iterLayout->addWidget(pImpl_->iterSpin);
    iterLayout->addWidget(new QLabel("(higher = stronger correction, slower)"));
    iterLayout->addStretch();
    influenceLayout->addLayout(iterLayout);

    mainLayout->addWidget(influenceGroup);

    // === Buttons ===
    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    auto* resetBtn = new QPushButton("Reset");
    connect(resetBtn, &QPushButton::clicked, this, [this, labLabel, rgbLabel]() {
        pImpl_->selected_L = 50; pImpl_->selected_a = 0; pImpl_->selected_b = 0;
        pImpl_->influenceRadius = 0.3; pImpl_->iterations = 3;
        if (pImpl_->lSpin) pImpl_->lSpin->setValue(50);
        if (pImpl_->aSpin) pImpl_->aSpin->setValue(0);
        if (pImpl_->bSpin) pImpl_->bSpin->setValue(0);
        labLabel->setText("L*a*b*: 50.0, 0.0, 0.0");
        rgbLabel->setText("RGB: 128, 128, 128");
        pImpl_->colorSwatchLabel->setStyleSheet("background-color: gray; border: 1px solid #555;");
    });
    buttonLayout->addWidget(resetBtn);

    auto* cancelBtn = new QPushButton("Cancel");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelBtn);

    auto* applyBtn = new QPushButton("Apply Correction");
    applyBtn->setDefault(true);
    applyBtn->setStyleSheet("background-color: #0078d4; color: #1f1f1f; padding: 6px 20px;");
    connect(applyBtn, &QPushButton::clicked, this, &HoneycombSpotDialog::onApply);
    buttonLayout->addWidget(applyBtn);

    mainLayout->addLayout(buttonLayout);

    // Initialize preview
    updateInfluencePreview(0.3);
}

HoneycombSpotDialog::~HoneycombSpotDialog() = default;

HoneycombSpotDialog::SpotColor HoneycombSpotDialog::getSpotColor() const {
    return SpotColor{
        "Custom Spot",
        pImpl_->selected_L,
        pImpl_->selected_a,
        pImpl_->selected_b,
        0.3, // influenceRadius
        3    // iterations
    };
}

void HoneycombSpotDialog::setSourceImage(const QString& path) {
    pImpl_->imagePath = path;
}

void HoneycombSpotDialog::onSelectColor() {
    // Show color picker
}

void HoneycombSpotDialog::onApply() {
    emit spotColorAccepted(getSpotColor());
    accept();
}

void HoneycombSpotDialog::updateInfluencePreview(double radius) {
    if (!pImpl_->influencePreviewLabel) return;

    // Visualise the influence falloff: centre = full spot colour over the
    // surrounding image, fading to the neighbour tone at the radius edge with a
    // smoothstep transition (larger radius = wider, softer blend).
    const int w = 200, h = 40;
    QPixmap pix(w, h);
    QPainter painter(&pix);

    const QColor spot   = labToColor(pImpl_->selected_L, pImpl_->selected_a, pImpl_->selected_b);
    const QColor around(210, 210, 210);   // stand-in for the surrounding pixels
    const int centerX = w / 2;
    const double radiusPx = std::max(1.0, w * 0.5 * radius);

    for (int x = 0; x < w; ++x) {
        double t = std::min(1.0, std::abs(x - centerX) / radiusPx);  // 0 centre .. 1 edge
        double s = t * t * (3.0 - 2.0 * t);                          // smoothstep
        double k = 1.0 - s;                                          // spot weight
        int r = static_cast<int>(spot.red()   * k + around.red()   * (1 - k));
        int g = static_cast<int>(spot.green() * k + around.green() * (1 - k));
        int b = static_cast<int>(spot.blue()  * k + around.blue()  * (1 - k));
        painter.setPen(QColor(r, g, b));
        painter.drawLine(x, 0, x, h);
    }

    // Centre marker + radius edges
    painter.setPen(QColor(255, 255, 255, 180));
    painter.drawLine(centerX, 0, centerX, h);
    painter.setPen(QColor(0, 0, 0, 90));
    painter.drawLine(static_cast<int>(centerX - radiusPx), 0, static_cast<int>(centerX - radiusPx), h);
    painter.drawLine(static_cast<int>(centerX + radiusPx), 0, static_cast<int>(centerX + radiusPx), h);

    pImpl_->influencePreviewLabel->setPixmap(pix);
}

// ColorPickerFromImage implementation
ColorPickerFromImage::ColorPickerFromImage(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(300, 200);
    setMouseTracking(true);
}

ColorPickerFromImage::~ColorPickerFromImage() = default;

void ColorPickerFromImage::setImage(const QString& path) {
    imagePath_ = path;
    displayImage_.load(path);
    selectedPoint_ = QPoint(displayImage_.width() / 2, displayImage_.height() / 2);
    update();
}

void ColorPickerFromImage::mousePressEvent(QMouseEvent* event) {
    setSelectedPoint(event->pos());
}

void ColorPickerFromImage::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        setSelectedPoint(event->pos());
    }
}

void ColorPickerFromImage::paintEvent(QPaintEvent*) {
    QPainter painter(this);

    if (!displayImage_.isNull()) {
        QPixmap pixmap = QPixmap::fromImage(displayImage_);
        pixmap = pixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawPixmap(0, 0, pixmap);
    } else {
        painter.fillRect(rect(), Qt::darkGray);
        painter.drawText(rect(), Qt::AlignCenter, "No image loaded");
    }

    // Draw crosshair at selected point
    if (!selectedPoint_.isNull()) {
        painter.setPen(Qt::white);
        int x = selectedPoint_.x();
        int y = selectedPoint_.y();
        painter.drawLine(x - 10, y, x + 10, y);
        painter.drawLine(x, y - 10, x, y + 10);
        painter.drawEllipse(selectedPoint_, 5, 5);
    }
}

void ColorPickerFromImage::setSelectedPoint(const QPoint& point) {
    selectedPoint_ = point;

    // Get color at point
    if (!displayImage_.isNull()) {
        QPoint scaledPoint;
        scaledPoint.setX(point.x() * displayImage_.width() / width());
        scaledPoint.setY(point.y() * displayImage_.height() / height());

        if (scaledPoint.x() >= 0 && scaledPoint.x() < displayImage_.width() &&
            scaledPoint.y() >= 0 && scaledPoint.y() < displayImage_.height()) {

            QRgb rgb = displayImage_.pixel(scaledPoint);
            double r = qRed(rgb) / 255.0;
            double g = qGreen(rgb) / 255.0;
            double b = qBlue(rgb) / 255.0;

            // Simplified RGB to Lab (should use spectral here)
            double L = (r + g + b) / 3.0 * 100;
            double a = (r - g) * 50;
            double lab_b = (g - b) * 50;

            emit pointSelected(point, L, a, lab_b);
        }
    }

    update();
}

// SpotColorCorrectionTool implementation
SpotColorCorrectionTool::SpotColorCorrectionTool(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    auto* infoLabel = new QLabel(
        "Honeycomb Spot Color Mode A: Select a region from the image, "
        "pick the target color, and control the influence radius for gradient blending.");
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    auto* addBtn = new QPushButton("Add Spot Color Correction...");
    connect(addBtn, &QPushButton::clicked, this, &SpotColorCorrectionTool::onAddSpot);
    layout->addWidget(addBtn);

    layout->addStretch();
}

SpotColorCorrectionTool::~SpotColorCorrectionTool() = default;

void SpotColorCorrectionTool::setImage(const QString& imagePath) {
    currentImagePath_ = imagePath;
}

QVector<SpotColorCorrectionTool::Correction> SpotColorCorrectionTool::getCorrections() const {
    return corrections_;
}

void SpotColorCorrectionTool::addCorrection(const Correction& correction) {
    corrections_.append(correction);
    emit correctionsChanged();
}

void SpotColorCorrectionTool::removeCorrection(int index) {
    if (index >= 0 && index < corrections_.size()) {
        corrections_.remove(index);
        emit correctionsChanged();
    }
}

void SpotColorCorrectionTool::clearCorrections() {
    corrections_.clear();
    emit correctionsChanged();
}

void SpotColorCorrectionTool::onAddSpot() {
    HoneycombSpotDialog dialog(this);
    dialog.setSourceImage(currentImagePath_);
    if (dialog.exec() == QDialog::Accepted) {
        auto spot = dialog.getSpotColor();
        HoneycombSpotDialog::SpotColor sc = spot;
        Q_UNUSED(sc);
    }
}

void SpotColorCorrectionTool::onPreview() {
    renderPreview();
}

void SpotColorCorrectionTool::renderPreview() {
    // Apply spot color corrections to preview
}