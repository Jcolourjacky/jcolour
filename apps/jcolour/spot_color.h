#pragma once

#include <QWidget>
#include <QDialog>
#include <QString>
#include <QSlider>
#include <QLabel>

class InfluenceRadiusSlider : public QWidget {
    Q_OBJECT
public:
    explicit InfluenceRadiusSlider(QWidget* parent = nullptr);
    ~InfluenceRadiusSlider() override;

    double value() const { return slider_->value() / 100.0; }
    void setValue(double val) {
        slider_->setValue(static_cast<int>(val * 100));
        valueLabel_->setText(QString::number(slider_->value()) + "%");
    }

signals:
    void valueChanged(double value);

private:
    QSlider* slider_;
    QLabel* valueLabel_;
};

class HoneycombSpotDialog : public QDialog {
    Q_OBJECT
public:
    explicit HoneycombSpotDialog(QWidget* parent = nullptr);
    ~HoneycombSpotDialog() override;

    struct SpotColor {
        QString name;
        double target_L;
        double target_a;
        double target_b;
        double influenceRadius; // 0-1 (normalized)
        int iterations;
    };

    SpotColor getSpotColor() const;
    void setSourceImage(const QString& path);

signals:
    void spotColorAccepted(const SpotColor& color);

private slots:
    void onSelectColor();
    void onApply();

private:
    void loadImage();
    void updatePreview();
    void updateInfluencePreview(double radius);

    struct Impl;
    Impl* pImpl_;
};

class SpotColorCorrectionTool : public QWidget {
    Q_OBJECT
public:
    explicit SpotColorCorrectionTool(QWidget* parent = nullptr);
    ~SpotColorCorrectionTool() override;

    void setImage(const QString& imagePath);

    struct Correction {
        QRectF region;
        double target_L, target_a, target_b;
        double influenceRadius;
    };

    QVector<Correction> getCorrections() const;
    void addCorrection(const Correction& correction);
    void removeCorrection(int index);
    void clearCorrections();

signals:
    void correctionsChanged();

private slots:
    void onAddSpot();
    void onPreview();

private:
    void renderPreview();

    QString currentImagePath_;
    QVector<HoneycombSpotDialog::SpotColor> spotColors_;
    QVector<Correction> corrections_;
};

class ColorPickerFromImage : public QWidget {
    Q_OBJECT
public:
    explicit ColorPickerFromImage(QWidget* parent = nullptr);
    ~ColorPickerFromImage() override;

    void setImage(const QString& path);
    QPoint getSelectedPoint() const { return selectedPoint_; }
    void setSelectedPoint(const QPoint& point);

signals:
    void pointSelected(const QPoint& point, double L, double a, double b);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QString imagePath_;
    QPoint selectedPoint_;
    QImage displayImage_;
};