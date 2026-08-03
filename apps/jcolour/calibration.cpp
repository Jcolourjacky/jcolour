#include "calibration.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QProgressBar>
#include <QPainter>
#include <QPrinter>
#include <QPrintDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QDate>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

G7CalibrationWizard::G7CalibrationWizard(QWidget* parent)
    : QDialog(parent)
    , currentStep_(Step_Intro)
    , measurementsComplete_(false)
{
    setWindowTitle("G7+ Gray Balance Calibration");
    setMinimumSize(600, 500);
    setupUI();
}

G7CalibrationWizard::~G7CalibrationWizard() = default;

jcolour::calibration::CalibrationResult G7CalibrationWizard::getResult() const {
    jcolour::calibration::CalibrationResult result;
    result.success = measurementsComplete_;
    return result;
}

void G7CalibrationWizard::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // Step indicator
    auto* stepIndicator = new QHBoxLayout;
    QStringList stepNames = {"Intro", "Select Target", "Load Paper", "Align", "Measure", "Review", "Apply"};
    for (const QString& name : stepNames) {
        auto* label = new QLabel(name);
        label->setStyleSheet("padding: 4px 8px; color: gray;");
        stepIndicator->addWidget(label);
    }
    mainLayout->addLayout(stepIndicator);

    // Status
    statusLabel_ = new QLabel("Welcome to G7+ Gray Balance Calibration");
    statusLabel_->setStyleSheet("font-size: 14px; padding: 16px;");
    mainLayout->addWidget(statusLabel_);

    // Progress
    auto* progressBar = new QProgressBar;
    progressBar->setRange(0, 100);
    mainLayout->addWidget(progressBar);

    // Content area
    auto* contentWidget = new QWidget;
    mainLayout->addWidget(contentWidget, 1);

    // Navigation buttons
    auto* navLayout = new QHBoxLayout;
    auto* prevBtn = new QPushButton("Previous");
    auto* nextBtn = new QPushButton("Next");
    auto* cancelBtn = new QPushButton("Cancel");
    navLayout->addWidget(prevBtn);
    navLayout->addStretch();
    navLayout->addWidget(cancelBtn);
    navLayout->addWidget(nextBtn);
    mainLayout->addLayout(navLayout);

    connect(prevBtn, &QPushButton::clicked, this, &G7CalibrationWizard::onPreviousStep);
    connect(nextBtn, &QPushButton::clicked, this, &G7CalibrationWizard::onNextStep);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void G7CalibrationWizard::onNextStep() {
    if (currentStep_ < Step_Apply) {
        currentStep_ = static_cast<Step>(currentStep_ + 1);
        updateStepDisplay();
    }
}

void G7CalibrationWizard::onPreviousStep() {
    if (currentStep_ > Step_Intro) {
        currentStep_ = static_cast<Step>(currentStep_ - 1);
        updateStepDisplay();
    }
}

void G7CalibrationWizard::onFinish() {
    applyG7Compensation();
    accept();
}

void G7CalibrationWizard::onMeasurePatch() {
    // Measurement is handled via the i1Pro3 integration
    // This slot is called when a patch measurement is ready
    QMessageBox::information(this, "Measure", "Move to next patch");
}

void G7CalibrationWizard::updateStepDisplay() {
    switch (currentStep_) {
        case Step_Intro:
            statusLabel_->setText("G7+ Gray Balance Calibration\n\n"
                                 "This wizard will calibrate your printer to match the G7 gray scale.\n"
                                 "You'll need an i1Pro3 or compatible spectrophotometer.");
            break;
        case Step_SelectTarget:
            statusLabel_->setText("Select your measurement target:\n\n"
                                 "• N-36: 36 gray patches (recommended for most users)\n"
                                 "• N-21: 21 gray patches (faster)\n"
                                 "• Custom: Import your own target");
            break;
        case Step_Measure:
            statusLabel_->setText("Measure the gray ramp:\n\n"
                                 "1. Place the target on the printer\n"
                                 "2. Click 'Measure' and move the i1Pro3 across each patch\n"
                                 "3. Press Space to capture each measurement\n"
                                 "4. Continue until all patches are measured");
            break;
        case Step_Review:
            statusLabel_->setText("Review results:\n\n"
                                 "Max deviation: " + QString::number(maxDeviation_) + " ΔE\n"
                                 "Status: " + (maxDeviation_ < 3.0 ? "PASS" : "NEEDS ADJUSTMENT"));
            break;
        default:
            break;
    }
}

void G7CalibrationWizard::applyG7Compensation() {
    // Apply G7 compensation to the color pipeline
    // This would integrate with the engine's G7 module
}

// GrayBalanceCalibrator implementation
GrayBalanceCalibrator::GrayBalanceCalibrator(QWidget* parent)
    : QWidget(parent)
    , targetPatches_(21)
    , maxDeviation_(0.0)
{
    auto* layout = new QVBoxLayout(this);

    auto* infoGroup = new QGroupBox("Gray Balance Status");
    auto* infoLayout = new QVBoxLayout(infoGroup);

    auto* statusLayout = new QHBoxLayout;
    statusLayout->addWidget(new QLabel("Last Calibration:"));
    statusLayout->addWidget(new QLabel("Not calibrated"));
    statusLayout->addStretch();
    infoLayout->addLayout(statusLayout);

    auto* deviationLayout = new QHBoxLayout;
    deviationLayout->addWidget(new QLabel("Max Deviation:"));
    deviationLayout->addWidget(new QLabel("--"));
    deviationLayout->addStretch();
    infoLayout->addLayout(deviationLayout);

    auto* measureBtn = new QPushButton("Measure Gray Ramp");
    infoLayout->addWidget(measureBtn);

    auto* importBtn = new QPushButton("Import from File...");
    infoLayout->addWidget(importBtn);

    layout->addWidget(infoGroup);
    layout->addStretch();

    connect(measureBtn, &QPushButton::clicked, this, &GrayBalanceCalibrator::onMeasureGrayRamp);
    connect(importBtn, &QPushButton::clicked, this, &GrayBalanceCalibrator::onImportFromFile);
}

GrayBalanceCalibrator::~GrayBalanceCalibrator() = default;

void GrayBalanceCalibrator::setTargetPatches(int count) {
    targetPatches_ = count;
}

QVector<double> GrayBalanceCalibrator::getGrayCurves() const {
    return correctionCurves_;
}

void GrayBalanceCalibrator::onMeasureGrayRamp() {
    // Trigger i1Pro3 measurement
    emit calibrationNeeded(true);
}

void GrayBalanceCalibrator::onImportFromFile() {
    QString path = QFileDialog::getOpenFileName(this, "Import Measurement Data",
                                                QString(), "CSV Files (*.csv)");
    if (!path.isEmpty()) {
        // Parse CSV and calculate correction curves
    }
}

void GrayBalanceCalibrator::calculateGrayDeviation() {
    // Calculate deviation from ideal gray axis
}

void GrayBalanceCalibrator::generateCorrectionCurves() {
    correctionCurves_.resize(targetPatches_);
    for (int i = 0; i < targetPatches_; ++i) {
        double x = i / static_cast<double>(targetPatches_ - 1);
        correctionCurves_[i] = x; // Linear by default
    }
}

// InkCartridgeTracker implementation
InkCartridgeTracker::InkCartridgeTracker(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    auto* headerLayout = new QHBoxLayout;
    headerLayout->addWidget(new QLabel("Ink Cartridge Tracker"));
    headerLayout->addStretch();
    auto* addBtn = new QPushButton("Add Cartridge");
    headerLayout->addWidget(addBtn);
    layout->addLayout(headerLayout);

    // Ink level display
    auto* levelsGroup = new QGroupBox("Current Levels");
    auto* levelsLayout = new QVBoxLayout(levelsGroup);

    QStringList inkTypes = {"PK", "MK", "LC", "C", "LM", "M", "Y", "Or", "LK", "LLK", "V", "Gr"};
    for (const QString& ink : inkTypes) {
        auto* rowLayout = new QHBoxLayout;
        rowLayout->addWidget(new QLabel(ink));
        auto* progress = new QProgressBar;
        progress->setRange(0, 100);
        progress->setValue(100);
        rowLayout->addWidget(progress);
        layout->addLayout(rowLayout);
    }

    layout->addWidget(levelsGroup);
    layout->addStretch();

    connect(addBtn, &QPushButton::clicked, this, [this]() {
        // Show add cartridge dialog
    });
}

InkCartridgeTracker::~InkCartridgeTracker() = default;

bool InkCartridgeTracker::CartridgeInfo::isLow() {
    return fillPercent < 15;
}

bool InkCartridgeTracker::CartridgeInfo::isExpired() {
    return installDate.daysTo(QDate::currentDate()) > 180; // 6 months
}

void InkCartridgeTracker::addCartridge(const CartridgeInfo& info) {
    cartridges_.append(info);
    updateDisplay();
}

void InkCartridgeTracker::updateCartridgeLevel(const QString& inkType, int fillPercent) {
    for (auto& cartridge : cartridges_) {
        if (cartridge.inkType == inkType) {
            cartridge.fillPercent = fillPercent;
            if (fillPercent < 15) {
                emit lowInkWarning(inkType);
            }
            break;
        }
    }
    updateDisplay();
}

QVector<InkCartridgeTracker::CartridgeInfo> InkCartridgeTracker::getAllCartridges() const {
    return cartridges_;
}

void InkCartridgeTracker::updateDisplay() {
    // Refresh ink level display
}

void InkCartridgeTracker::checkForDrift(const QString& inkType) {
    // Compare current measurements to baseline
    // If deltaE > threshold, emit driftWarning
}

// TestPrintGenerator implementation
TestPrintGenerator::TestPrintGenerator(QWidget* parent)
    : QWidget(parent)
    , currentType_(TestType::GrayRamp)
{
    auto* layout = new QVBoxLayout(this);

    auto* typeGroup = new QGroupBox("Test Print Type");
    auto* typeLayout = new QVBoxLayout(typeGroup);

    auto* typeCombo = new QComboBox;
    typeCombo->addItems({
        "Gray Ramp (21 steps)",
        "Gray Ramp (16 steps)",
        "ColorChecker 24-patch",
        "Saturation Wedge",
        "Ink Density Patches",
        "Resolution Chart",
        "GMG Print Quality + Paper-Feed (280mm)"
    });
    typeLayout->addWidget(typeCombo);

    auto* generateBtn = new QPushButton("Generate");
    typeLayout->addWidget(generateBtn);

    auto* printBtn = new QPushButton("Print...");
    typeLayout->addWidget(printBtn);

    layout->addWidget(typeGroup);
    layout->addStretch();

    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                currentType_ = static_cast<TestType>(index);
            });
    connect(generateBtn, &QPushButton::clicked, this, [this]() {
                generateTestPrint(currentType_);
            });
    connect(printBtn, &QPushButton::clicked, this, [this]() {
        QPrinter printer(QPrinter::HighResolution);
        QPrintDialog dialog(&printer, this);
        if (dialog.exec() == QDialog::Accepted) {
            QPainter painter(&printer);
            // Honour the image's physical size (set for the GMG chart) so the
            // 280 mm rulers print at true scale; fall back to 1:1 otherwise.
            if (generatedImage_.dotsPerMeterX() > 0) {
                const double mPerInch = 0.0254;
                double wIn = generatedImage_.width()  / (generatedImage_.dotsPerMeterX() * mPerInch);
                double hIn = generatedImage_.height() / (generatedImage_.dotsPerMeterY() * mPerInch);
                QRectF target(0, 0, wIn * printer.logicalDpiX(), hIn * printer.logicalDpiY());
                painter.drawImage(target, generatedImage_);
            } else {
                painter.drawImage(QPoint(0, 0), generatedImage_);
            }
        }
    });
}

TestPrintGenerator::~TestPrintGenerator() = default;

// Physical scale used by the GMG-style quality chart so the printed control
// lines really span 280.00 mm (verify the print is sent at 100% / no scaling).
static constexpr double kQcDpi = 300.0;
static int mmToPx(double mm) { return static_cast<int>(std::lround(mm / 25.4 * kQcDpi)); }

void TestPrintGenerator::generateTestPrint(TestType type) {
    QSize size(2880, 1800); // 10x6 inches at 288 DPI
    bool physical = false;
    if (type == TestType::PrintQualityCheck) {
        size = QSize(mmToPx(210.0), mmToPx(297.0));  // A4 portrait
        physical = true;
    }

    generatedImage_ = QImage(size, QImage::Format_RGB32);
    generatedImage_.fill(Qt::white);
    if (physical) {
        const int dpm = static_cast<int>(std::lround(kQcDpi / 25.4 * 1000.0));  // dots per metre
        generatedImage_.setDotsPerMeterX(dpm);
        generatedImage_.setDotsPerMeterY(dpm);
    }

    QPainter painter(&generatedImage_);
    painter.setRenderHint(QPainter::Antialiasing);

    switch (type) {
        case TestType::GrayRamp:
            drawGrayRamp(&painter, size);
            break;
        case TestType::ColorChecker:
            drawColorChecker(&painter, size, 4, 6);
            break;
        case TestType::SaturationWedge:
            drawSaturationWedge(&painter, size);
            break;
        case TestType::InkDensity:
            drawInkDensityPatches(&painter, size);
            break;
        case TestType::PrintQualityCheck:
            drawPrintQualityCheck(&painter, size);
            break;
        default:
            break;
    }

    emit testPrintGenerated(generatedImage_);
}

QImage TestPrintGenerator::getTestImage() const {
    return generatedImage_;
}

void TestPrintGenerator::drawGrayRamp(QPainter* painter, const QSize& size) {
    int steps = 21;
    int stripWidth = size.width() / steps;

    for (int i = 0; i < steps; ++i) {
        int gray = static_cast<int>(255.0 * i / (steps - 1));
        QColor color(gray, gray, gray);
        painter->fillRect(i * stripWidth, 0, stripWidth, size.height(), color);

        // Patch number
        painter->setPen(Qt::black);
        painter->drawText(i * stripWidth + 10, size.height() - 20,
                         QString::number(i));
    }
}

void TestPrintGenerator::drawColorChecker(QPainter* painter, const QSize& size, int rows, int cols) {
    int patchWidth = size.width() / cols;
    int patchHeight = size.height() / rows;

    constexpr double colorchecker_rgb[24][3] = {
        {164, 82, 56}, {179, 119, 95}, {156, 96, 109}, {113, 83, 68},
        {99, 137, 78}, {91, 126, 150}, {62, 84, 146}, {132, 72, 61},
        {208, 110, 51}, {201, 126, 122}, {182, 174, 75}, {86, 117, 71},
        {70, 109, 107}, {50, 68, 155}, {166, 73, 60}, {179, 131, 126},
        {185, 161, 67}, {90, 123, 104}, {66, 69, 144}, {60, 71, 88},
        {161, 129, 69}, {134, 66, 62}, {193, 90, 64}, {150, 63, 75}
    };

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            int idx = row * cols + col;
            if (idx < 24) {
                QColor color(colorchecker_rgb[idx][0],
                           colorchecker_rgb[idx][1],
                           colorchecker_rgb[idx][2]);
                painter->fillRect(col * patchWidth, row * patchHeight,
                               patchWidth, patchHeight, color);
            }
        }
    }
}

void TestPrintGenerator::drawSaturationWedge(QPainter* painter, const QSize& size) {
    // Draw saturation wedge from gray to full color
    int strips = 12;
    int stripWidth = size.width() / strips;

    for (int i = 0; i < strips; ++i) {
        double saturation = i / static_cast<double>(strips - 1);
        QColor color = QColor::fromHsv(0, saturation * 255, 200);
        painter->fillRect(i * stripWidth, 0, stripWidth, size.height(), color);
    }
}

void TestPrintGenerator::drawInkDensityPatches(QPainter* painter, const QSize& size) {
    // Draw patches for measuring individual ink density
    QStringList inkNames = {"PK", "MK", "LC", "C", "LM", "M", "Y", "Or"};
    int rows = inkNames.size();
    int cols = 11; // 0%, 10%, 20% ... 100%
    int patchWidth = size.width() / cols;
    int patchHeight = size.height() / rows;

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            int density = col * 10;
            int gray = 255 - (density * 255 / 100);
            QColor color(gray, gray, gray);
            painter->fillRect(col * patchWidth, row * patchHeight,
                           patchWidth, patchHeight, color);
        }
        painter->drawText(5, (row + 1) * patchHeight - 5, inkNames[row]);
    }
}

// GMG-style EPSON x900 / 7500-9500 Print Quality Check chart.
// Renders calibration strips, a CMY-70% homogeneity field framed by Y-80%,
// measurement-point squares, and two 0→280 mm rulers for the paper-feed check.
void TestPrintGenerator::drawPrintQualityCheck(QPainter* painter, const QSize& size) {
    const int W = size.width();
    const int marginX = mmToPx(14);     // side margin (rulers live here)
    const int rulerLen = mmToPx(280.0); // the calibrated paper-feed distance

    // --- header ---
    painter->setPen(QColor("#23262b"));
    QFont title("Helvetica", 11, QFont::Bold);
    painter->setFont(title);
    painter->drawText(marginX, mmToPx(10),
                      QStringLiteral("jcolour · EPSON Print Quality + Paper-Feed Check"));
    QFont sub("Helvetica", 7);
    painter->setFont(sub);
    painter->setPen(QColor("#868c95"));
    painter->drawText(marginX, mmToPx(14),
                      QStringLiteral("Print at 100%% (no scaling). Both side rulers must measure exactly 280.00 mm."));

    // --- top calibration control strip (measurement patches) ---
    auto drawStrip = [&](int yTop) {
        const int n = 28;
        const int x0 = marginX + mmToPx(6);
        const int w  = W - 2 * (marginX + mmToPx(6));
        const int pw = w / n;
        const int h  = mmToPx(7);
        for (int i = 0; i < n; ++i) {
            int hue = (i * 360) / n;
            QColor c = QColor::fromHsv(hue, 180, 200);
            if (i % 4 == 0) { int g = (i * 255) / n; c = QColor(g, g, g); }  // gray steps
            painter->fillRect(x0 + i * pw, yTop, pw, h, c);
        }
        painter->setPen(QColor("#c4c9d0"));
        painter->drawRect(x0, yTop, n * pw, h);
    };
    drawStrip(mmToPx(18));

    // --- vertical paper-feed rulers (left & right), 0..280 mm ---
    const int ruler0 = mmToPx(40);                 // y of the 0 mm mark
    auto drawRuler = [&](int xc) {
        painter->setPen(QColor("#23262b"));
        // long axis
        painter->drawLine(xc, ruler0, xc, ruler0 + rulerLen);
        QFont rf("Helvetica", 6);
        painter->setFont(rf);
        for (int mm = 0; mm <= 280; mm += 10) {
            int y = ruler0 + mmToPx(mm);
            bool major = (mm % 50 == 0) || mm == 280;
            int tick = mmToPx(major ? 4.5 : 2.5);
            painter->drawLine(xc, y, xc + tick, y);
            if (major)
                painter->drawText(xc + tick + mmToPx(1), y + mmToPx(2),
                                  QString::number(mm) + ".00 mm");
        }
    };
    drawRuler(marginX);
    drawRuler(W - marginX);
    painter->save();
    painter->setPen(QColor("#868c95"));
    painter->translate(marginX - mmToPx(3), ruler0 + rulerLen / 2);
    painter->rotate(-90);
    painter->drawText(0, 0, QStringLiteral("Paper Feed Check"));
    painter->restore();

    // --- homogeneity field: CMY 70% gray, framed by Y 80% ---
    const int fx = marginX + mmToPx(8);
    const int fy = ruler0;
    const int fw = W - 2 * fx;
    const int fh = rulerLen;
    const int band = mmToPx(8);
    painter->fillRect(fx, fy, fw, fh, QColor(255, 235, 0));            // Y 80% frame
    painter->fillRect(fx + band, fy + band, fw - 2 * band, fh - 2 * band,
                      QColor(110, 100, 96));                           // CMY 70% gray

    // measurement-point squares (corners + centre)
    painter->setPen(QPen(QColor("#ffffff"), mmToPx(0.4)));
    const int sq = mmToPx(8);
    auto mark = [&](int x, int y) { painter->drawRect(x - sq / 2, y - sq / 2, sq, sq); };
    int ix = fx + band + mmToPx(10), iy = fy + band + mmToPx(10);
    int ax = fx + fw - band - mmToPx(10), ay = fy + fh - band - mmToPx(10);
    mark(ix, iy); mark(ax, iy); mark(ix, ay); mark(ax, ay);
    mark((fx + fw) / 2, (fy + fh) / 2);

    painter->setPen(QColor("#23262b"));
    QFont lf("Helvetica", 7, QFont::Bold);
    painter->setFont(lf);
    painter->drawText(fx + mmToPx(2), fy - mmToPx(2),
                      QStringLiteral("Homogeneity: Y 80%% (frame) · CMY 70%% (field)"));

    // --- bottom calibration control strip ---
    drawStrip(fy + fh + mmToPx(4));
}
