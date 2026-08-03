#include "ink_channel_widget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QLabel>
#include <QPaintEvent>

InkChannelWidget::InkChannelWidget(QWidget* parent)
    : QWidget(parent)
    , channelIndex_(0)
    , curveValue_(0.5)
{
    setMinimumHeight(150);
    setMaximumWidth(60);
}

InkChannelWidget::~InkChannelWidget() = default;

QColor InkChannelWidget::channelColor() const {
    switch (channelIndex_) {
        case PhotoBlack:      return QColor(30, 30, 30);  // Dark gray
        case MatteBlack:      return QColor(50, 50, 50);  // Medium gray
        case LightCyan:       return QColor(150, 220, 240);
        case Cyan:            return QColor(0, 180, 220);
        case LightMagenta:    return QColor(230, 150, 200);
        case Magenta:         return QColor(200, 50, 150);
        case Yellow:          return QColor(240, 220, 50);
        case Orange:          return QColor(240, 130, 50);
        case LightBlack:      return QColor(100, 100, 100);
        case PhotoLightBlack: return QColor(80, 80, 80);
        case Violet:          return QColor(120, 50, 180);
        case Gray:            return QColor(120, 120, 120);
        default:              return Qt::gray;
    }
}

void InkChannelWidget::setChannelIndex(int index) {
    channelIndex_ = index;
    update();
}

void InkChannelWidget::setCurveValue(double value) {
    curveValue_ = qBound(0.0, value, 1.0);
    update();
}

void InkChannelWidget::setLinearizationCurve(const QVector<double>& curve) {
    linearizationCurve_ = curve;
    update();
}

void InkChannelWidget::setMeasurementData(const QVector<double>& measured) {
    measurementData_ = measured;
    update();
}

void InkChannelWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), QColor(40, 40, 40));

    drawChannelBar(&painter);
    drawCurveLine(&painter);
    drawMeasurementPoints(&painter);

    // Channel name
    painter.setPen(Qt::white);
    QStringList names = {"PK", "MK", "LC", "C", "LM", "M",
                         "Y", "Or", "LK", "LLK", "V", "Gr"};
    if (channelIndex_ < names.size()) {
        painter.drawText(rect(), Qt::AlignBottom | Qt::AlignHCenter, names[channelIndex_]);
    }
}

void InkChannelWidget::drawChannelBar(QPainter* painter) {
    int barX = 5;
    int barHeight = height() - 30;
    int barHeightPx = static_cast<int>(barHeight * curveValue_);

    QColor color = channelColor();
    QLinearGradient gradient(rect().topLeft(), rect().bottomLeft());
    gradient.setColorAt(0, color.lighter(150));
    gradient.setColorAt(1, color.darker(200));

    painter->fillRect(barX, height() - 25 - barHeightPx, BAR_WIDTH, barHeightPx, gradient);

    // Border
    painter->setPen(QColor(80, 80, 80));
    painter->drawRect(barX, height() - 25 - barHeightPx, BAR_WIDTH, barHeightPx);
}

void InkChannelWidget::drawCurveLine(QPainter* painter) {
    if (linearizationCurve_.isEmpty()) return;

    painter->setPen(QColor(255, 200, 100));
    QPainterPath path;
    int maxX = width() - 10;
    int maxY = height() - 30;

    for (int i = 0; i < linearizationCurve_.size(); ++i) {
        int x = 5 + (i * maxX) / linearizationCurve_.size();
        int y = maxY - static_cast<int>(linearizationCurve_[i] * maxY);
        if (i == 0) path.moveTo(x, y);
        else path.lineTo(x, y);
    }
    painter->drawPath(path);
}

void InkChannelWidget::drawMeasurementPoints(QPainter* painter) {
    if (measurementData_.isEmpty()) return;

    painter->setBrush(channelColor().lighter());
    painter->setPen(Qt::NoPen);
    int maxX = width() - 10;
    int maxY = height() - 30;

    for (int i = 0; i < measurementData_.size(); ++i) {
        int x = 5 + (i * maxX) / measurementData_.size();
        int y = maxY - static_cast<int>(measurementData_[i] * maxY);
        painter->drawEllipse(QPoint(x, y), 3, 3);
    }
}

void InkChannelWidget::mousePressEvent(QMouseEvent* event) {
    if (event->y() < height() - 25) {
        double newValue = 1.0 - (event->y() / static_cast<double>(height() - 25));
        setCurveValue(newValue);
        emit curveChanged();
    }
}

void InkChannelWidget::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        double newValue = 1.0 - (event->y() / static_cast<double>(height() - 25));
        setCurveValue(newValue);
        emit curveChanged();
    }
}

// InkChannelList implementation
InkChannelList::InkChannelList(QWidget* parent)
    : QWidget(parent)
    , activeChannel_(0)
{
    auto* layout = new QHBoxLayout(this);
    layout->setSpacing(2);
    layout->setContentsMargins(0, 0, 0, 0);

    for (int i = 0; i < NUM_CHANNELS; ++i) {
        auto* widget = new InkChannelWidget;
        widget->setChannelIndex(i);
        connect(widget, &InkChannelWidget::curveChanged, this, &InkChannelList::curvesChanged);
        connect(widget, &InkChannelWidget::linearizationRequested, this, &InkChannelList::curvesChanged);
        channelWidgets_.append(widget);
        layout->addWidget(widget);
    }
}

InkChannelList::~InkChannelList() = default;

void InkChannelList::setActiveChannel(int index) {
    activeChannel_ = index;
    for (int i = 0; i < channelWidgets_.size(); ++i) {
        channelWidgets_[i]->setStyleSheet(
            i == index ? "border: 2px solid white;" : "");
    }
}

void InkChannelList::setAllCurves(const QVector<QVector<double>>& curves) {
    for (int i = 0; i < qMin(curves.size(), channelWidgets_.size()); ++i) {
        channelWidgets_[i]->setLinearizationCurve(curves[i]);
    }
}

QVector<QVector<double>> InkChannelList::getAllCurves() const {
    QVector<QVector<double>> curves;
    for (auto* widget : channelWidgets_) {
        const auto& curve = widget->getLinearizationCurve();
        curves.append(curve.isEmpty()
            ? QVector<double>(256, 0.5)  // Default linear curve
            : curve);
    }
    return curves;
}