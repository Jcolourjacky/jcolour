#include "contact_sheet.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QPainter>
#include <QFontDialog>
#include <QImageReader>
#include <QtMath>

namespace jcolour {

QImage ContactSheetGenerator::generate(const QVector<ContactSheetImage>& images,
                                        const ContactSheetConfig& config) {
    // Calculate dimensions
    double dpi = 300.0;
    int cellWidthPx = static_cast<int>(config.cellWidthMm * dpi / 25.4);
    int cellHeightPx = static_cast<int>(config.cellHeightMm * dpi / 25.4);
    int marginPx = static_cast<int>(config.marginMm * dpi / 25.4);
    int spacingPx = static_cast<int>(config.spacingMm * dpi / 25.4);

    int totalWidth = marginPx * 2 + config.columns * cellWidthPx + (config.columns - 1) * spacingPx;
    int totalHeight = marginPx * 2 + config.rows * cellHeightPx + (config.rows - 1) * spacingPx
                      + static_cast<int>(config.fontSize * 1.5); // Space for labels

    QImage result(totalWidth, totalHeight, QImage::Format_RGB888);
    result.fill(Qt::white);

    QPainter painter(&result);
    painter.setFont(QFont(config.fontFamily, config.fontSize));

    int imgIndex = 0;
    for (int row = 0; row < config.rows && imgIndex < images.size(); ++row) {
        for (int col = 0; col < config.columns && imgIndex < images.size(); ++col) {
            int x = marginPx + col * (cellWidthPx + spacingPx);
            int y = marginPx + row * (cellHeightPx + spacingPx);

            const ContactSheetImage& img = images[imgIndex];

            // Load and scale thumbnail
            QImage thumb(img.path);
            if (!thumb.isNull()) {
                thumb = thumb.scaled(cellWidthPx, cellHeightPx,
                                     Qt::KeepAspectRatio, Qt::SmoothTransformation);

                // Center in cell
                int offsetX = (cellWidthPx - thumb.width()) / 2;
                int offsetY = (cellHeightPx - thumb.height()) / 2;
                painter.drawImage(x + offsetX, y + offsetY, thumb);
            }

            // Draw border
            painter.setPen(QColor(200, 200, 200));
            painter.drawRect(x, y, cellWidthPx, cellHeightPx);

            // Draw label
            if (config.showLabels && !img.label.isEmpty()) {
                painter.setPen(Qt::black);
                painter.drawText(x, y + cellHeightPx + config.fontSize,
                                img.label);
            }

            // Draw number
            if (config.showImageNumbers) {
                painter.setPen(Qt::blue);
                painter.drawText(x + 2, y + config.fontSize + 2,
                                QString::number(img.number));
            }

            imgIndex++;
        }
    }

    return result;
}

QImage ContactSheetGenerator::generateFromPaths(const QStringList& paths,
                                                const ContactSheetConfig& config) {
    QVector<ContactSheetImage> images;
    for (int i = 0; i < paths.size(); ++i) {
        ContactSheetImage img;
        img.path = paths[i];
        img.number = i + 1;
        img.label = QFileInfo(paths[i]).baseName();
        images.append(img);
    }
    return generate(images, config);
}

// ============================================
// ContactSheetDialog Implementation
// ============================================

ContactSheetDialog::ContactSheetDialog(QWidget* parent)
    : QWidget(parent)
    , config_()
{
    setWindowTitle("Contact Sheet Generator");
    setMinimumSize(600, 500);
    setStyleSheet("QWidget { background-color: #e6e6e6; color: #1f1f1f; }");

    auto* mainLayout = new QVBoxLayout(this);

    // Preview area
    previewLabel_ = new QLabel("Drop images here or add via button");
    previewLabel_->setMinimumSize(400, 300);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setStyleSheet("background-color: #ffffff; border: 1px dashed #555;");
    mainLayout->addWidget(previewLabel_, 1);

    // Config panel
    auto* configGroup = new QGroupBox("Layout");
    auto* configLayout = new QGridLayout(configGroup);

    configLayout->addWidget(new QLabel("Columns:"), 0, 0);
    columnsSpin_ = new QSpinBox;
    columnsSpin_->setRange(1, 10);
    columnsSpin_->setValue(4);
    connect(columnsSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ContactSheetDialog::onColumnsChanged);
    configLayout->addWidget(columnsSpin_, 0, 1);

    configLayout->addWidget(new QLabel("Rows:"), 0, 2);
    rowsSpin_ = new QSpinBox;
    rowsSpin_->setRange(1, 20);
    rowsSpin_->setValue(5);
    connect(rowsSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ContactSheetDialog::onRowsChanged);
    configLayout->addWidget(rowsSpin_, 0, 3);

    configLayout->addWidget(new QLabel("Font Size:"), 1, 0);
    fontSizeSpin_ = new QSpinBox;
    fontSizeSpin_->setRange(6, 24);
    fontSizeSpin_->setValue(10);
    connect(fontSizeSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ContactSheetDialog::onFontChanged);
    configLayout->addWidget(fontSizeSpin_, 1, 1);

    configLayout->addWidget(new QLabel("Paper:"), 1, 2);
    auto* paperCombo = new QComboBox;
    paperCombo->addItems({"A4", "Letter", "A3", "Custom"});
    configLayout->addWidget(paperCombo, 1, 3);

    auto* labelCheck = new QCheckBox("Show Labels");
    labelCheck->setChecked(true);
    configLayout->addWidget(labelCheck, 2, 0);

    auto* numberCheck = new QCheckBox("Show Numbers");
    numberCheck->setChecked(true);
    configLayout->addWidget(numberCheck, 2, 1);

    mainLayout->addWidget(configGroup);

    // Action buttons
    auto* buttonLayout = new QHBoxLayout;

    auto* addBtn = new QPushButton("Add Images...");
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        QStringList files = QFileDialog::getOpenFileNames(this, "Select Images",
            QString(), "Images (*.tif *.tiff *.jpg *.jpeg *.png)");
        if (!files.isEmpty()) {
            imagePaths_ << files;
            loadThumbnails();
            updatePreview();
        }
    });
    buttonLayout->addWidget(addBtn);

    buttonLayout->addStretch();

    auto* previewBtn = new QPushButton("Update Preview");
    connect(previewBtn, &QPushButton::clicked, this, &ContactSheetDialog::updatePreview);
    buttonLayout->addWidget(previewBtn);

    auto* saveBtn = new QPushButton("Save...");
    connect(saveBtn, &QPushButton::clicked, this, &ContactSheetDialog::onBrowseOutput);
    buttonLayout->addWidget(saveBtn);

    mainLayout->addLayout(buttonLayout);
}

ContactSheetDialog::~ContactSheetDialog() = default;

void ContactSheetDialog::setImages(const QStringList& paths) {
    imagePaths_ = paths;
    loadThumbnails();
    updatePreview();
}

void ContactSheetDialog::setConfig(const ContactSheetConfig& config) {
    config_ = config;
    columnsSpin_->setValue(config.columns);
    rowsSpin_->setValue(config.rows);
    fontSizeSpin_->setValue(config.fontSize);
}

ContactSheetConfig ContactSheetDialog::getConfig() const {
    ContactSheetConfig config;
    config.columns = columnsSpin_->value();
    config.rows = rowsSpin_->value();
    config.fontSize = fontSizeSpin_->value();
    return config;
}

QImage ContactSheetDialog::generatePreview() const {
    return ContactSheetGenerator::generateFromPaths(imagePaths_, getConfig());
}

QString ContactSheetDialog::saveToFile(const QString& path) const {
    QImage img = generatePreview();
    if (img.save(path)) {
        return path;
    }
    return QString();
}

void ContactSheetDialog::loadThumbnails() {
    thumbnails_.clear();
    for (const QString& path : imagePaths_) {
        QImage img(path);
        if (!img.isNull()) {
            // Generate thumbnail
            thumbnails_.append(img.scaled(80, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

void ContactSheetDialog::updatePreview() {
    if (thumbnails_.isEmpty()) {
        previewLabel_->setText("Drop images here or add via button");
        return;
    }

    // Create composite preview
    int maxWidth = 400;
    int maxHeight = 300;
    int cols = qMin(columnsSpin_->value(), thumbnails_.size());
    int rows = (thumbnails_.size() + cols - 1) / cols;

    int thumbW = maxWidth / cols - 4;
    int thumbH = maxHeight / rows - 4;

    QImage preview(maxWidth, maxHeight, QImage::Format_RGB888);
    preview.fill(QColor(26, 26, 26));

    QPainter painter(&preview);
    int x = 2, y = 2;
    for (int i = 0; i < thumbnails_.size() && i < cols * rows; ++i) {
        QImage thumb = thumbnails_[i].scaled(thumbW, thumbH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawImage(x + (thumbW - thumb.width()) / 2, y + (thumbH - thumb.height()) / 2, thumb);

        x += thumbW + 4;
        if ((i + 1) % cols == 0) {
            x = 2;
            y += thumbH + 4;
        }
    }

    previewLabel_->setPixmap(QPixmap::fromImage(preview));
}

void ContactSheetDialog::onColumnsChanged(int) {
    config_.columns = columnsSpin_->value();
    updatePreview();
}

void ContactSheetDialog::onRowsChanged(int) {
    config_.rows = rowsSpin_->value();
    updatePreview();
}

void ContactSheetDialog::onFontChanged() {
    config_.fontSize = fontSizeSpin_->value();
    emit configChanged();
}

void ContactSheetDialog::onBrowseOutput() {
    QString path = QFileDialog::getSaveFileName(this, "Save Contact Sheet",
        QString(), "Images (*.tif *.tiff *.png)");
    if (!path.isEmpty()) {
        saveToFile(path);
    }
}

} // namespace jcolour