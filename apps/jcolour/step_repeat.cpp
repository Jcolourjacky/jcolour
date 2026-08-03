#include "step_repeat.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QPainter>
#include <QFileInfo>
#include <QImageReader>

namespace jcolour {

QImage StepRepeatGenerator::generate(const QImage& source,
                                      const StepRepeatConfig& config) {
    double dpi = 300.0;

    // Calculate output dimensions
    double outputWidthMm, outputHeightMm;
    if (config.outputPaperSize == "A4") {
        outputWidthMm = 210;
        outputHeightMm = 297;
    } else if (config.outputPaperSize == "Letter") {
        outputWidthMm = 215.9;
        outputHeightMm = 279.4;
    } else if (config.outputPaperSize == "A3") {
        outputWidthMm = 297;
        outputHeightMm = 420;
    } else {
        outputWidthMm = config.customWidthMm;
        outputHeightMm = config.customHeightMm;
    }

    int outputWidthPx = static_cast<int>(outputWidthMm * dpi / 25.4);
    int outputHeightPx = static_cast<int>(outputHeightMm * dpi / 25.4);

    QImage result(outputWidthPx, outputHeightPx, QImage::Format_RGB888);
    result.fill(Qt::white);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);

    // Calculate positions
    double marginPx = config.marginMm * dpi / 25.4;
    double spacingPx = config.spacingMm * dpi / 25.4;

    double cellWidthPx = (outputWidthPx - 2 * marginPx - (config.columns - 1) * spacingPx) / config.columns;
    double cellHeightPx = (outputHeightPx - 2 * marginPx - (config.rows - 1) * spacingPx) / config.rows;

    // Scale image to fit cell
    double scaleX = cellWidthPx / (config.imageWidthMm * dpi / 25.4);
    double scaleY = cellHeightPx / (config.imageHeightMm * dpi / 25.4);
    double scale = qMin(scaleX, scaleY);

    int scaledWidth = static_cast<int>((config.imageWidthMm * dpi / 25.4) * scale);
    int scaledHeight = static_cast<int>((config.imageHeightMm * dpi / 25.4) * scale);

    // Scale source image
    QImage scaled = source.scaled(scaledWidth, scaledHeight,
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // Draw each instance
    int count = 0;
    for (int row = 0; row < config.rows; ++row) {
        for (int col = 0; col < config.columns; ++col) {
            int x = static_cast<int>(marginPx + col * (cellWidthPx + spacingPx) + (cellWidthPx - scaledWidth) / 2);
            int y = static_cast<int>(marginPx + row * (cellHeightPx + spacingPx) + (cellHeightPx - scaledHeight) / 2);

            painter.drawImage(x, y, scaled);
            count++;

            // Draw label
            if (config.addLabels) {
                painter.setPen(Qt::black);
                painter.drawText(x, y + scaledHeight + 12,
                               QString("Image %1").arg(config.startNumber + count - 1));
            }
        }
    }

    // Draw crop marks
    if (config.addCropMarks) {
        painter.setPen(QColor(0, 0, 0));
        double cropMarkLengthPx = config.cropMarkLengthMm * dpi / 25.4;
        double cropMarkOffsetPx = config.cropMarkOffsetMm * dpi / 25.4;

        for (int row = 0; row <= config.rows; ++row) {
            for (int col = 0; col <= config.columns; ++col) {
                // Calculate corner positions
                double cx = marginPx + col * (cellWidthPx + spacingPx) - spacingPx / 2;
                double cy = marginPx + row * (cellHeightPx + spacingPx) - spacingPx / 2;

                // Draw crop marks (intersection of lines)
                // Horizontal lines
                painter.drawLine(static_cast<int>(cx - cropMarkLengthPx / 2), static_cast<int>(cy),
                               static_cast<int>(cx + cropMarkLengthPx / 2), static_cast<int>(cy));
                // Vertical lines
                painter.drawLine(static_cast<int>(cx), static_cast<int>(cy - cropMarkLengthPx / 2),
                               static_cast<int>(cx), static_cast<int>(cy + cropMarkLengthPx / 2));
            }
        }
    }

    // Draw page numbers
    if (config.addPageNumbers && config.rows > 1 && config.columns > 1) {
        painter.setPen(Qt::blue);
        painter.drawText(outputWidthPx - 50, outputHeightPx - 20,
                        QString("Page 1"));
    }

    return result;
}

QImage StepRepeatGenerator::generateFromPath(const QString& imagePath,
                                               const StepRepeatConfig& config) {
    QImage source(imagePath);
    if (source.isNull()) {
        return QImage();
    }
    return generate(source, config);
}

// ============================================
// StepRepeatDialog Implementation
// ============================================

StepRepeatDialog::StepRepeatDialog(QWidget* parent)
    : QDialog(parent)
    , config_()
{
    setWindowTitle("Step & Repeat");
    setMinimumSize(600, 500);
    setStyleSheet("QWidget { background-color: #e6e6e6; color: #1f1f1f; }");

    auto* mainLayout = new QVBoxLayout(this);

    // Preview
    previewLabel_ = new QLabel("No image loaded");
    previewLabel_->setMinimumSize(400, 300);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setStyleSheet("background-color: #ffffff; border: 1px solid #555;");
    mainLayout->addWidget(previewLabel_, 1);

    // Layout settings
    auto* layoutGroup = new QGroupBox("Layout");
    auto* layoutLayout = new QGridLayout(layoutGroup);

    layoutLayout->addWidget(new QLabel("Columns:"), 0, 0);
    columnsSpin_ = new QSpinBox;
    columnsSpin_->setRange(1, 10);
    columnsSpin_->setValue(3);
    connect(columnsSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StepRepeatDialog::onColumnsChanged);
    layoutLayout->addWidget(columnsSpin_, 0, 1);

    layoutLayout->addWidget(new QLabel("Rows:"), 0, 2);
    rowsSpin_ = new QSpinBox;
    rowsSpin_->setRange(1, 10);
    rowsSpin_->setValue(2);
    connect(rowsSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StepRepeatDialog::onRowsChanged);
    layoutLayout->addWidget(rowsSpin_, 0, 3);

    layoutLayout->addWidget(new QLabel("Spacing (mm):"), 1, 0);
    spacingSpin_ = new QDoubleSpinBox;
    spacingSpin_->setRange(0, 50);
    spacingSpin_->setValue(5);
    spacingSpin_->setSuffix(" mm");
    layoutLayout->addWidget(spacingSpin_, 1, 1);

    layoutLayout->addWidget(new QLabel("Paper:"), 1, 2);
    paperSizeCombo_ = new QComboBox;
    paperSizeCombo_->addItems({"A4", "A3", "Letter", "Custom"});
    connect(paperSizeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StepRepeatDialog::onPaperSizeChanged);
    layoutLayout->addWidget(paperSizeCombo_, 1, 3);

    mainLayout->addWidget(layoutGroup);

    // Options
    auto* optionsGroup = new QGroupBox("Options");
    auto* optionsLayout = new QHBoxLayout(optionsGroup);

    cropMarksCheck_ = new QCheckBox("Add Crop Marks");
    cropMarksCheck_->setChecked(true);
    optionsLayout->addWidget(cropMarksCheck_);

    labelsCheck_ = new QCheckBox("Labels");
    labelsCheck_->setChecked(false);
    optionsLayout->addWidget(labelsCheck_);

    pageNumbersCheck_ = new QCheckBox("Page Numbers");
    pageNumbersCheck_->setChecked(false);
    optionsLayout->addWidget(pageNumbersCheck_);

    mainLayout->addWidget(optionsGroup);

    // Buttons
    auto* buttonLayout = new QHBoxLayout;

    auto* loadBtn = new QPushButton("Load Image...");
    connect(loadBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Select Image",
            QString(), "Images (*.tif *.tiff *.jpg *.jpeg *.png)");
        if (!path.isEmpty()) {
            setSourceImage(path);
        }
    });
    buttonLayout->addWidget(loadBtn);

    buttonLayout->addStretch();

    auto* previewBtn = new QPushButton("Update Preview");
    connect(previewBtn, &QPushButton::clicked, this, &StepRepeatDialog::updatePreview);
    buttonLayout->addWidget(previewBtn);

    auto* saveBtn = new QPushButton("Save...");
    connect(saveBtn, &QPushButton::clicked, this, &StepRepeatDialog::onBrowseOutput);
    buttonLayout->addWidget(saveBtn);

    mainLayout->addLayout(buttonLayout);
}

StepRepeatDialog::~StepRepeatDialog() = default;

void StepRepeatDialog::setSourceImage(const QString& path) {
    sourceImagePath_ = path;
    QFileInfo info(path);
    previewLabel_->setText(QString("Ready: %1\n%2 × %3 instances")
        .arg(info.fileName())
        .arg(columnsSpin_->value())
        .arg(rowsSpin_->value()));
    updatePreview();
}

StepRepeatConfig StepRepeatDialog::getConfig() const {
    StepRepeatConfig config;
    config.columns = columnsSpin_->value();
    config.rows = rowsSpin_->value();
    config.spacingMm = spacingSpin_->value();
    config.outputPaperSize = paperSizeCombo_->currentText();
    config.addCropMarks = cropMarksCheck_->isChecked();
    config.addLabels = labelsCheck_->isChecked();
    config.addPageNumbers = pageNumbersCheck_->isChecked();
    return config;
}

void StepRepeatDialog::setConfig(const StepRepeatConfig& config) {
    config_ = config;
    columnsSpin_->setValue(config.columns);
    rowsSpin_->setValue(config.rows);
    spacingSpin_->setValue(config.spacingMm);
    paperSizeCombo_->setCurrentText(config.outputPaperSize);
    cropMarksCheck_->setChecked(config.addCropMarks);
    labelsCheck_->setChecked(config.addLabels);
    pageNumbersCheck_->setChecked(config.addPageNumbers);
}

QImage StepRepeatDialog::generatePreview() const {
    if (sourceImagePath_.isEmpty()) {
        return QImage();
    }
    return StepRepeatGenerator::generateFromPath(sourceImagePath_, getConfig());
}

QString StepRepeatDialog::saveToFile(const QString& path) const {
    QImage img = generatePreview();
    if (img.isNull()) return QString();

    if (img.save(path, "TIFF")) {
        return path;
    }
    return QString();
}

void StepRepeatDialog::onColumnsChanged(int) {
    config_.columns = columnsSpin_->value();
    emit configChanged();
    updatePreview();
}

void StepRepeatDialog::onRowsChanged(int) {
    config_.rows = rowsSpin_->value();
    emit configChanged();
    updatePreview();
}

void StepRepeatDialog::onPaperSizeChanged(int index) {
    QStringList papers = {"A4", "A3", "Letter", "Custom"};
    if (index >= 0 && index < papers.size()) {
        config_.outputPaperSize = papers[index];
    }
    emit configChanged();
}

void StepRepeatDialog::onBrowseOutput() {
    QString path = QFileDialog::getSaveFileName(this, "Save Step & Repeat",
        QString(), "TIFF Files (*.tif *.tiff)");
    if (!path.isEmpty()) {
        saveToFile(path);
    }
}

void StepRepeatDialog::updatePreview() {
    if (sourceImagePath_.isEmpty()) {
        previewLabel_->setText("No image loaded");
        return;
    }

    QImage preview = generatePreview();
    if (!preview.isNull()) {
        // Create scaled preview
        QImage scaled = preview.scaled(400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        previewLabel_->setPixmap(QPixmap::fromImage(scaled));
    }
}

} // namespace jcolour