#pragma once

// SplitToneWheel — the 三段色環 for 多色黑 split-toning.
//
// One CIELAB a*/b* disc carrying three draggable handles (Highlights / Midtones
// / Shadows). Each handle's position is the tint applied to that tonal zone:
//   x = a* (right = red/magenta, left = green)
//   y = b* (up = yellow/warm,    down = blue/cool)
//   radius = tint strength (0 at center = neutral, 1 at rim = full)
// This mirrors the engine's Wf3SplitConfig::target_a/target_b + TintDirection.

#include <QWidget>
#include <array>

namespace jcolour {
namespace ui {

class SplitToneWheel : public QWidget {
    Q_OBJECT
public:
    enum Zone { Highlight = 0, Midtone = 1, Shadow = 2, kZones = 3 };

    explicit SplitToneWheel(QWidget* parent = nullptr);

    // a, b in [-1, 1] (clamped to the unit disc). No signal emitted.
    void setZone(int zone, float a, float b);
    float zoneA(int zone) const { return pts_[zone].a; }
    float zoneB(int zone) const { return pts_[zone].b; }
    void  resetAll();   // all zones → neutral centre

signals:
    void zoneChanged(int zone, float a, float b);   // user dragged a handle

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    struct Pt { float a = 0.f; float b = 0.f; };

    QPointF centre() const;
    double  radiusPx() const;
    QPointF toScreen(const Pt& p) const;
    Pt      fromScreen(const QPointF& s) const;   // clamped to unit disc
    int     hitTest(const QPointF& s) const;
    void    rebuildDisc();

    std::array<Pt, kZones> pts_{};
    int      dragging_ = -1;
    QPixmap  disc_;        // cached a*/b* colour wheel
    QSize    discSize_;
};

}  // namespace ui
}  // namespace jcolour
