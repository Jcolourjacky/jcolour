#include "test_print.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QRadioButton>
#include <QLabel>
#include <QPainter>
#include <QTransform>
#include <QFileInfo>
#include <algorithm>

namespace jcolour {

TestPrintDialog::TestPrintDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Test Print Options");
    setMinimumSize(500, 400);
    setStyleSheet("QDialog { background-color: #e6e6e6; } "
                  "QGroupBox { color: #1f1f1f; font-weight: bold; } "
                  "QLabel { color: #ccc; }");

    setupUI();
}

TestPrintDialog::~TestPrintDialog() = default;

void TestPrintDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // === Image info ===
    auto* imageGroup = new QGroupBox("Image");
    auto* imageLayout = new QVBoxLayout(imageGroup);

    infoLabel_ = new QLabel("No image loaded");
    infoLabel_->setStyleSheet("color: #999;");
    imageLayout->addWidget(infoLabel_);

    // Preview thumbnail
    previewLabel_ = new QLabel;
    previewLabel_->setMinimumSize(150, 100);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setStyleSheet("background-color: #ffffff; border: 1px solid #555;");
    imageLayout->addWidget(previewLabel_);

    mainLayout->addWidget(imageGroup);

    // === Print mode ===
    auto* modeGroup = new QGroupBox("Test Print Mode");
    auto* modeLayout = new QVBoxLayout(modeGroup);

    // Full size
    auto* fullRadio = new QRadioButton("Full Size");
    fullRadio->setChecked(true);
    modeLayout->addWidget(fullRadio);

    // Scaled down
    auto* scaledRadio = new QRadioButton("Scaled Down (for quick check)");
    modeLayout->addWidget(scaledRadio);

    scaledOptions_ = new QWidget;
    auto* scaledLayout = new QHBoxLayout(scaledOptions_);
    scaledLayout->setContentsMargins(20, 5, 0, 5);
    scaledLayout->addWidget(new QLabel("Size:"));
    widthSpin_ = new QSpinBox;
    widthSpin_->setRange(10, 500);
    widthSpin_->setValue(100);
    widthSpin_->setSuffix(" mm");
    scaledLayout->addWidget(widthSpin_);
    scaledLayout->addWidget(new QLabel("×"));
    heightSpin_ = new QSpinBox;
    heightSpin_->setRange(10, 500);
    heightSpin_->setValue(100);
    heightSpin_->setSuffix(" mm");
    scaledLayout->addWidget(heightSpin_);
    scaledLayout->addStretch();
    modeLayout->addWidget(scaledOptions_);

    // 1:1
    auto* oneToOneRadio = new QRadioButton("1:1 Pixel Mapping");
    modeLayout->addWidget(oneToOneRadio);
    auto* oneToOneInfo = new QLabel("  Each screen pixel = one printed dot (zoom 100%)");
    oneToOneInfo->setStyleSheet("color: gray; font-size: 11px;");
    modeLayout->addWidget(oneToOneInfo);

    // Same paper
    auto* samePaperRadio = new QRadioButton("Same Paper as Available");
    modeLayout->addWidget(samePaperRadio);

    auto* paperLayout = new QHBoxLayout;
    paperLayout->setContentsMargins(20, 5, 0, 5);
    paperLayout->addWidget(new QLabel("Select paper:"));
    paperCombo_ = new QComboBox;
    paperCombo_->addItem("-- No papers available --");
    paperLayout->addWidget(paperCombo_, 1);
    modeLayout->addLayout(paperLayout);

    autoRotateCheck_ = new QCheckBox("Auto-rotate to fit");
    autoRotateCheck_->setChecked(true);
    autoRotateCheck_->setStyleSheet("color: #ccc;");
    modeLayout->addWidget(autoRotateCheck_);

    mainLayout->addWidget(modeGroup);

    // === Options ===
    auto* optionsGroup = new QGroupBox("Options");
    auto* optionsLayout = new QGridLayout(optionsGroup);

    bleedCheck_ = new QCheckBox("Include bleed");
    bleedCheck_->setChecked(false);
    optionsLayout->addWidget(bleedCheck_, 0, 0);

    bleedSpin_ = new QSpinBox;
    bleedSpin_->setRange(1, 10);
    bleedSpin_->setValue(3);
    bleedSpin_->setSuffix(" mm");
    optionsLayout->addWidget(bleedSpin_, 0, 1);

    auto* colorModeLabel = new QLabel("Color mode:");
    optionsLayout->addWidget(colorModeLabel, 1, 0);

    auto* colorModeCombo = new QComboBox;
    colorModeCombo->addItems({"Color", "Grayscale", "B&W (Single Black)", "B&W (Multi-channel)"});
    optionsLayout->addWidget(colorModeCombo, 1, 1);

    mainLayout->addWidget(optionsGroup);

    // === Buttons ===
    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    auto* cancelBtn = new QPushButton("Cancel");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelBtn);

    auto* printBtn = new QPushButton("Print Test");
    printBtn->setDefault(true);
    printBtn->setStyleSheet("background-color: #0078d4; color: #1f1f1f; padding: 6px 24px;");
    connect(printBtn, &QPushButton::clicked, this, &TestPrintDialog::onPrint);
    buttonLayout->addWidget(printBtn);

    mainLayout->addLayout(buttonLayout);

    // Connect mode selection
    connect(fullRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (!checked) return;
        currentMode_ = TestPrintConfig::Mode::Full;
        scaledOptions_->setEnabled(false);
        paperCombo_->setEnabled(false);
        updatePreview();
    });
    connect(scaledRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (!checked) return;
        currentMode_ = TestPrintConfig::Mode::ScaledDown;
        scaledOptions_->setEnabled(true);
        paperCombo_->setEnabled(false);
        updatePreview();
    });
    connect(oneToOneRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (!checked) return;
        currentMode_ = TestPrintConfig::Mode::OneToOne;
        scaledOptions_->setEnabled(false);
        paperCombo_->setEnabled(false);
        updatePreview();
    });
    connect(samePaperRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (!checked) return;
        currentMode_ = TestPrintConfig::Mode::SamePaper;
        scaledOptions_->setEnabled(false);
        paperCombo_->setEnabled(true);
        updatePreview();
    });

    // Live preview when options change
    connect(widthSpin_,  QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int){ updatePreview(); });
    connect(heightSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int){ updatePreview(); });
    connect(bleedCheck_, &QCheckBox::toggled, this, [this](bool){ updatePreview(); });
    connect(autoRotateCheck_, &QCheckBox::toggled, this, [this](bool){ updatePreview(); });
    connect(paperCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){ updatePreview(); });

    // Initial enabled state (fullRadio is checked before connections exist).
    scaledOptions_->setEnabled(false);
    paperCombo_->setEnabled(false);
}

void TestPrintDialog::setSourceImage(const QImage& image) {
    sourceImage_ = image;
    updatePreview();
}

void TestPrintDialog::setImagePath(const QString& path) {
    sourceImagePath_ = path;
    sourceImage_.load(path);
    updatePreview();
}

void TestPrintDialog::addAvailablePaper(const PaperStock& paper) {
    QString item = QString("%1 (%2×%3mm) - %4")
        .arg(paper.name)
        .arg(paper.widthMm, 0, 'f', 0)
        .arg(paper.heightMm, 0, 'f', 0)
        .arg(paper.vendor);
    paperCombo_->addItem(item, QVariant::fromValue(paper));

    // Enable same paper option if we have papers
    if (paperCombo_->count() > 1) {
        paperCombo_->removeItem(0); // Remove placeholder
    }
}

void TestPrintDialog::clearAvailablePapers() {
    paperCombo_->clear();
    paperCombo_->addItem("-- No papers available --");
}

TestPrintConfig TestPrintDialog::getConfig() const {
    TestPrintConfig config;

    if (sourceImage_.isNull()) {
        return config;
    }

    config.mode = currentMode_;
    config.scaledWidthMm = widthSpin_->value();
    config.scaledHeightMm = heightSpin_->value();
    config.includeBleed = bleedCheck_->isChecked();
    config.bleedMm = bleedSpin_->value();
    config.autoRotate = autoRotateCheck_->isChecked();

    if (paperCombo_->currentIndex() > 0) {
        config.selectedPaper = paperCombo_->currentText();
    }

    return config;
}

void TestPrintDialog::onModeChanged(int index) {
    Q_UNUSED(index);
    updatePreview();
}

void TestPrintDialog::updatePreview() {
    if (sourceImage_.isNull()) {
        infoLabel_->setText("No image loaded");
        previewLabel_->clear();
        return;
    }
    constexpr double kDpi = 360.0;
    const double imgWmm = sourceImage_.width()  / kDpi * 25.4;
    const double imgHmm = sourceImage_.height() / kDpi * 25.4;

    // Canvas representing the sheet / test area.
    const int CW = 240, CH = 176;
    QPixmap canvas(CW, CH);
    canvas.fill(QColor(0xe6, 0xe6, 0xe6));
    QPainter p(&canvas);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QImage thumb = sourceImage_.scaled(CW, CH, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QString info;
    QFileInfo fi(sourceImagePath_);
    const QString base = QString("%1 — %2×%3 px")
        .arg(fi.fileName().isEmpty() ? QStringLiteral("image") : fi.fileName())
        .arg(sourceImage_.width()).arg(sourceImage_.height());

    auto drawSheet = [&](double sheetWmm, double sheetHmm, const QColor& paperCol) -> QRectF {
        // fit a sheet of the given mm aspect into the canvas, return its rect
        double margin = 12.0;
        double availW = CW - 2 * margin, availH = CH - 2 * margin;
        double ar = sheetWmm / sheetHmm;
        double w = availW, h = w / ar;
        if (h > availH) { h = availH; w = h * ar; }
        QRectF r((CW - w) / 2, (CH - h) / 2, w, h);
        p.fillRect(r, paperCol);
        p.setPen(QColor(0x88, 0x88, 0x88));
        p.drawRect(r);
        return r;
    };

    switch (currentMode_) {
    case TestPrintConfig::Mode::Full: {
        QPoint at((CW - thumb.width()) / 2, (CH - thumb.height()) / 2);
        p.drawImage(at, thumb);
        p.setPen(QColor(0x64, 0x64, 0x64));
        p.drawRect(QRect(at, thumb.size()).adjusted(0, 0, -1, -1));
        info = QString("%1 · Full size %2×%3 mm @%4dpi")
            .arg(base).arg(imgWmm, 0, 'f', 0).arg(imgHmm, 0, 'f', 0).arg(int(kDpi));
        break;
    }
    case TestPrintConfig::Mode::ScaledDown: {
        // sample placed on a reference 17"×22" sheet so its smallness is visible
        const double refWmm = 432.0, refHmm = 559.0;
        bool land = imgWmm >= imgHmm;
        QRectF sheet = drawSheet(land ? refWmm : refHmm, land ? refHmm : refWmm, Qt::white);
        double sw = widthSpin_->value(), sh = heightSpin_->value();
        double pxPerMm = sheet.width() / (land ? refWmm : refHmm);
        QImage s = sourceImage_.scaled(int(sw * pxPerMm), int(sh * pxPerMm),
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
        p.drawImage(QPointF(sheet.x() + 6, sheet.y() + 6), s);
        info = QString("%1 · Scaled sample %2×%3 mm on sheet")
            .arg(base).arg(int(sw)).arg(int(sh));
        break;
    }
    case TestPrintConfig::Mode::OneToOne: {
        // test area 203×254 mm (8×10"); only the region inside it prints 1:1
        const double areaWmm = 203.0, areaHmm = 254.0;
        QPoint at((CW - thumb.width()) / 2, (CH - thumb.height()) / 2);
        p.drawImage(at, thumb);
        double fracW = std::min(1.0, areaWmm / imgWmm);
        double fracH = std::min(1.0, areaHmm / imgHmm);
        QRectF region(at.x() + thumb.width()  * (1 - fracW) / 2,
                      at.y() + thumb.height() * (1 - fracH) / 2,
                      thumb.width() * fracW, thumb.height() * fracH);
        p.setPen(QPen(QColor(0xd0, 0x32, 0x2d), 2));
        p.drawRect(region);
        bool fits = imgWmm <= areaWmm && imgHmm <= areaHmm;
        info = QString("%1 · 1:1 = %2×%3 mm @%4dpi%5")
            .arg(base).arg(imgWmm, 0, 'f', 0).arg(imgHmm, 0, 'f', 0).arg(int(kDpi))
            .arg(fits ? QString() : QStringLiteral(" · only marked region fits 8×10\""));
        break;
    }
    case TestPrintConfig::Mode::SamePaper: {
        PaperStock ps;
        bool have = paperCombo_->currentIndex() >= 0 &&
                    paperCombo_->currentData().canConvert<PaperStock>();
        if (have) ps = paperCombo_->currentData().value<PaperStock>();
        double pw = have ? ps.widthMm : 210.0, ph = have ? ps.heightMm : 297.0;
        QRectF sheet = drawSheet(pw, ph, Qt::white);
        // place image into the sheet, auto-rotating if it fits better
        QImage img = sourceImage_;
        if (autoRotateCheck_->isChecked()) {
            bool imgLand = img.width() >= img.height();
            bool sheetLand = pw >= ph;
            if (imgLand != sheetLand) {
                QTransform t; t.rotate(90);
                img = img.transformed(t, Qt::SmoothTransformation);
            }
        }
        QImage placed = img.scaled(int(sheet.width()) - 8, int(sheet.height()) - 8,
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);
        p.drawImage(QPointF(sheet.x() + (sheet.width() - placed.width()) / 2,
                            sheet.y() + (sheet.height() - placed.height()) / 2), placed);
        info = QString("%1 · on %2 (%3×%4 mm)")
            .arg(base, have ? ps.name : QStringLiteral("default A4"))
            .arg(pw, 0, 'f', 0).arg(ph, 0, 'f', 0);
        break;
    }
    }

    p.end();
    previewLabel_->setPixmap(canvas);
    infoLabel_->setText(info);
}

void TestPrintDialog::onPrint() {
    TestPrintConfig config = getConfig();

    if (sourceImage_.isNull()) {
        return;
    }

    emit testPrintRequested(config);
    accept();
}

void TestPrintDialog::updateOneToOneInfo() {
    // 1:1 sizing is rendered inline by updatePreview() for the OneToOne mode.
    updatePreview();
}

} // namespace jcolour