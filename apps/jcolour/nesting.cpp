#include "nesting.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QFileDialog>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollArea>
#include <QUuid>
#include <algorithm>
#include <cmath>

NestingWidget::NestingWidget(QWidget* parent)
    : QWidget(parent)
    , gutter_(5.0)
{
    paperSize_ = QSizeF(297, 420); // A3 default
    margins_[0] = margins_[1] = margins_[2] = margins_[3] = 10.0;
    algorithm_ = jcolour::nesting::NestingAlgorithm::Simple;

    auto* mainLayout = new QVBoxLayout(this);

    // Controls
    auto* controlsLayout = new QHBoxLayout;

    // Algorithm selector
    auto* algoLayout = new QHBoxLayout;
    algoLayout->addWidget(new QLabel("Algorithm:"));
    auto* algoCombo = new QComboBox;
    algoCombo->addItems({"Simple (Rectangle)", "Row Major", "True Shape"});
    algoLayout->addWidget(algoCombo);
    connect(algoCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NestingWidget::onAlgorithmChanged);
    controlsLayout->addLayout(algoLayout);

    // Paper size
    auto* paperLayout = new QHBoxLayout;
    paperLayout->addWidget(new QLabel("Paper:"));
    auto* widthSpin = new QDoubleSpinBox;
    widthSpin->setRange(50, 1000);
    widthSpin->setValue(297);
    widthSpin->setSuffix(" mm");
    paperLayout->addWidget(widthSpin);
    auto* heightSpin = new QDoubleSpinBox;
    heightSpin->setRange(50, 1500);
    heightSpin->setValue(420);
    heightSpin->setSuffix(" mm");
    paperLayout->addWidget(heightSpin);
    controlsLayout->addLayout(paperLayout);
    connect(widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double w) { paperSize_.setWidth(w); update(); });
    connect(heightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double h) { paperSize_.setHeight(h); update(); });

    // Gutter
    auto* gutterLayout = new QHBoxLayout;
    gutterLayout->addWidget(new QLabel("Gutter:"));
    auto* gutterSpin = new QDoubleSpinBox;
    gutterSpin->setRange(0, 50);
    gutterSpin->setValue(5);
    gutterSpin->setSuffix(" mm");
    gutterLayout->addWidget(gutterSpin);
    controlsLayout->addLayout(gutterLayout);
    connect(gutterSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double g) { gutter_ = g; update(); });

    controlsLayout->addStretch();
    mainLayout->addLayout(controlsLayout);

    // Preview area
    auto* previewArea = new QScrollArea;
    previewArea->setBackgroundRole(QPalette::Dark);
    auto* previewWidget = new QWidget;
    previewWidget->setMinimumSize(600, 800);
    previewArea->setWidget(previewWidget);
    mainLayout->addWidget(previewArea, 1);

    // Action buttons
    auto* buttonLayout = new QHBoxLayout;
    auto* addBtn = new QPushButton("Add Images...");
    auto* removeBtn = new QPushButton("Remove Selected");
    auto* nestBtn = new QPushButton("Run Nesting");
    auto* printBtn = new QPushButton("Print...");
    nestBtn->setDefault(true);
    buttonLayout->addWidget(addBtn);
    buttonLayout->addWidget(removeBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(nestBtn);
    buttonLayout->addWidget(printBtn);
    mainLayout->addLayout(buttonLayout);

    connect(addBtn, &QPushButton::clicked, this, &NestingWidget::onAddItem);
    connect(removeBtn, &QPushButton::clicked, this, &NestingWidget::onRemoveSelected);
    connect(nestBtn, &QPushButton::clicked, this, &NestingWidget::onRunNesting);
    connect(printBtn, &QPushButton::clicked, this, [this]() {
        // Print the nested layout
    });

    // Scale for display
    setMinimumSize(800, 600);
}

NestingWidget::~NestingWidget() = default;

void NestingWidget::addItem(const jcolour::nesting::PrintItem& item) {
    items_.append(item);
    update();
    emit itemsChanged();
}

void NestingWidget::removeItem(const QString& id) {
    items_.erase(
        std::remove_if(items_.begin(), items_.end(),
                      [&id](const jcolour::nesting::PrintItem& item) {
                          return item.id == id;
                      }),
        items_.end());
    update();
    emit itemsChanged();
}

void NestingWidget::clearItems() {
    items_.clear();
    update();
    emit itemsChanged();
}

QVector<jcolour::nesting::PrintItem> NestingWidget::getItems() const {
    return items_;
}

void NestingWidget::setPaperSize(double widthMm, double heightMm) {
    paperSize_.setWidth(widthMm);
    paperSize_.setHeight(heightMm);
    update();
}

void NestingWidget::setMargins(double top, double right, double bottom, double left) {
    margins_[0] = top;
    margins_[1] = right;
    margins_[2] = bottom;
    margins_[3] = left;
    update();
}

void NestingWidget::setGutter(double gutterMm) {
    gutter_ = gutterMm;
    update();
}

QRectF NestingWidget::getPrintableArea() const {
    return QRectF(
        margins_[3],                    // left
        margins_[0],                    // top
        paperSize_.width() - margins_[1] - margins_[3],
        paperSize_.height() - margins_[0] - margins_[2]
    );
}

jcolour::nesting::NestingResult NestingWidget::nest() {
    QRectF area = getPrintableArea();
    jcolour::nesting::NestingResult result;

    switch (algorithm_) {
        case jcolour::nesting::NestingAlgorithm::Simple:
            result = SimpleNester::nest(items_, paperSize_, area, gutter_);
            break;
        case jcolour::nesting::NestingAlgorithm::RowMajor: {
            // Shelf packing is tighter when the tallest items lead each row.
            QVector<jcolour::nesting::PrintItem> sorted = items_;
            std::sort(sorted.begin(), sorted.end(),
                      [](const jcolour::nesting::PrintItem& a,
                         const jcolour::nesting::PrintItem& b) {
                          return a.bounds.height() > b.bounds.height();
                      });
            result = SimpleNester::nest(sorted, paperSize_, area, gutter_);
            break;
        }
        case jcolour::nesting::NestingAlgorithm::TrueShape:
            result = TrueShapeNester::nest(items_, paperSize_, area, gutter_);
            break;
    }

    lastResult_ = result;
    update();
    emit nestingComplete(result);
    return result;
}

void NestingWidget::drawPaper(QPainter* painter) {
    // Draw paper outline
    QRectF paperRect(0, 0, paperSize_.width(), paperSize_.height());
    painter->fillRect(paperRect, Qt::white);
    painter->setPen(QPen(Qt::black, 0.5));
    painter->drawRect(paperRect);

    // Draw margins
    QRectF marginRect(margins_[3], margins_[0],
                     paperSize_.width() - margins_[1] - margins_[3],
                     paperSize_.height() - margins_[0] - margins_[2]);
    painter->setPen(QPen(Qt::gray, 0.3, Qt::DashLine));
    painter->drawRect(marginRect);
}

void NestingWidget::drawItems(QPainter* painter) {
    painter->setPen(QPen(Qt::blue, 0.3));

    for (const auto& item : lastResult_.placedItems) {
        QRectF itemRect(item.bounds);
        itemRect.moveTopLeft(item.bounds.topLeft());
        painter->drawRect(itemRect);
    }
}

void NestingWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Scale to fit window
    double scaleX = width() / (paperSize_.width() + 40);
    double scaleY = height() / (paperSize_.height() + 40);
    double scale = qMin(scaleX, scaleY);

    painter.translate(20, 20);
    painter.scale(scale, scale);

    drawPaper(&painter);
    drawItems(&painter);
}

void NestingWidget::onAddItem() {
    QStringList files = QFileDialog::getOpenFileNames(this, "Add Images",
                                                      QString(),
                                                      "Images (*.tif *.tiff *.png *.jpg)");
    for (const QString& file : files) {
        jcolour::nesting::PrintItem item;
        item.id = QUuid::createUuid().toString();
        item.imagePath = file;
        item.bounds = QRectF(0, 0, 100, 150); // Default 10x15cm
        item.rotation = 0;
        item.copies = 1;
        addItem(item);
    }
}

void NestingWidget::onRemoveSelected() {
    // Remove selected items
}

void NestingWidget::onRunNesting() {
    nest();
}

void NestingWidget::onAlgorithmChanged(int index) {
    algorithm_ = static_cast<jcolour::nesting::NestingAlgorithm>(index);
}

// SimpleNester implementation — shelf (row) packing with per-copy expansion and
// auto-rotation to fit the remaining row width.
jcolour::nesting::NestingResult SimpleNester::nest(
    const QVector<jcolour::nesting::PrintItem>& items,
    const QSizeF& sheetSize,
    const QRectF& printableArea,
    double gutter) {

    Q_UNUSED(sheetSize);
    jcolour::nesting::NestingResult result;
    result.pagesRequired = 1;

    // Expand copies into individual placements.
    QVector<jcolour::nesting::PrintItem> queue;
    for (const auto& it : items) {
        int n = qMax(1, it.copies);
        for (int c = 0; c < n; ++c) {
            jcolour::nesting::PrintItem copy = it;
            copy.copies = 1;
            if (n > 1) copy.id = it.id + QString("#%1").arg(c + 1);
            queue.append(copy);
        }
    }

    QVector<jcolour::nesting::PrintItem> placed;
    double currentX = printableArea.left();
    double currentY = printableArea.top();
    double maxHeightInRow = 0;
    double usedArea = 0;

    auto rotate = [](jcolour::nesting::PrintItem& it) {
        it.bounds.setSize(QSizeF(it.bounds.height(), it.bounds.width()));
        it.rotation = (it.rotation == 90.0) ? 0.0 : 90.0;
    };

    for (auto item : queue) {
        double w = item.bounds.width();
        double h = item.bounds.height();
        double remaining = printableArea.right() - currentX;

        // If it doesn't fit the remaining row width but would fit rotated, rotate.
        if (w > remaining + 1 && h <= remaining + 1 && h <= printableArea.width() + 1) {
            rotate(item);
            w = item.bounds.width();
            h = item.bounds.height();
        }

        // Wrap to a new row when the row width is exceeded.
        if (currentX + w > printableArea.right() + 1) {
            currentX = printableArea.left();
            currentY += maxHeightInRow + gutter;
            maxHeightInRow = 0;
        }

        // New page when the column height is exceeded.
        if (currentY + h > printableArea.bottom() + 1) {
            result.pagesRequired++;
            currentX = printableArea.left();
            currentY = printableArea.top();
            maxHeightInRow = 0;
        }

        item.bounds.moveTopLeft(QPointF(currentX, currentY));
        placed.append(item);
        usedArea += w * h;
        currentX += w + gutter;
        maxHeightInRow = qMax(maxHeightInRow, h);
    }

    result.placedItems = placed;
    const double sheetArea = printableArea.width() * printableArea.height()
                             * qMax(1, result.pagesRequired);
    result.utilization = sheetArea > 0 ? qMin(1.0, usedArea / sheetArea) : 0.0;
    return result;
}

// TrueShapeNester implementation
jcolour::nesting::NestingResult TrueShapeNester::nest(
    const QVector<jcolour::nesting::PrintItem>& items,
    const QSizeF& sheetSize,
    const QRectF& printableArea,
    double gutter) {

    // For now, fall back to simple nester
    // True shape nesting would use more sophisticated algorithms
    return SimpleNester::nest(items, sheetSize, printableArea, gutter);
}