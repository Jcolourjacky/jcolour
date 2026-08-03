#pragma once

#include <QWidget>
#include <QVector>
#include <QPaintEvent>

class InkChannelWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int channelIndex READ channelIndex WRITE setChannelIndex)
    Q_PROPERTY(double curveValue READ curveValue WRITE setCurveValue)

public:
    explicit InkChannelWidget(QWidget* parent = nullptr);
    ~InkChannelWidget() override;

    enum InkChannel : uint8_t {
        PhotoBlack = 0,
        MatteBlack = 1,
        LightCyan = 2,
        Cyan = 3,
        LightMagenta = 4,
        Magenta = 5,
        Yellow = 6,
        Orange = 7,
        LightBlack = 8,
        PhotoLightBlack = 9,
        Violet = 10,
        Gray = 11
    };

    void setChannelIndex(int index);
    int channelIndex() const { return channelIndex_; }

    void setCurveValue(double value); // 0-1 normalized
    double curveValue() const { return curveValue_; }

    void setLinearizationCurve(const QVector<double>& curve);
    void setMeasurementData(const QVector<double>& measured);
    const QVector<double>& getLinearizationCurve() const { return linearizationCurve_; }

    QColor channelColor() const;

signals:
    void curveChanged();
    void linearizationRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void drawChannelBar(QPainter* painter);
    void drawCurveLine(QPainter* painter);
    void drawMeasurementPoints(QPainter* painter);

    int channelIndex_;
    double curveValue_;
    QVector<double> linearizationCurve_;
    QVector<double> measurementData_;
    static constexpr int BAR_WIDTH = 30;
};

class InkChannelList : public QWidget {
    Q_OBJECT
public:
    explicit InkChannelList(QWidget* parent = nullptr);
    ~InkChannelList() override;

    void setActiveChannel(int index);
    void setAllCurves(const QVector<QVector<double>>& curves);
    QVector<QVector<double>> getAllCurves() const;

    static constexpr int NUM_CHANNELS = 12;

signals:
    void channelSelected(int index);
    void curvesChanged();

private:
    QVector<InkChannelWidget*> channelWidgets_;
    int activeChannel_;
};