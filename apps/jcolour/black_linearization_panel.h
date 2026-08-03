#pragma once

// BlackLinearizationPanel — Per-ink 線性化 for the 多色黑 workflow's four black
// channels: PK / MK / GY / LGY (engine channel_layout.h ActiveChannel MAIN_K +
// GY + LGY, with PK/MK shown as the two physical mains). Each tab is an
// InkCurveEditor mapping input L* → ink output, so the operator can flatten the
// per-channel response after measurement. Curves feed the engine LUT builders.

#include <QWidget>
#include <QVector>

class InkCurveEditor;
class QTabWidget;

namespace jcolour {
namespace ui {

class BlackLinearizationPanel : public QWidget {
    Q_OBJECT
public:
    enum BlackChannel { PK = 0, MK = 1, GY = 2, LGY = 3, kChannels = 4 };

    explicit BlackLinearizationPanel(QWidget* parent = nullptr);

    // input→output linearization curve for one black channel (values 0..1).
    QVector<double> curve(int channel, int resolution = 256) const;
    void resetAll();   // every channel → linear

signals:
    void curvesChanged();

private:
    QTabWidget*              tabs_ = nullptr;
    QVector<InkCurveEditor*> editors_;
};

}  // namespace ui
}  // namespace jcolour
