#include "split_tone_wheel.h"

#include <QPainter>
#include <QMouseEvent>
#include <QtMath>
#include <algorithm>

namespace jcolour {
namespace ui {

namespace {
constexpr double kMargin   = 14.0;   // px inset for handles to stay on canvas
constexpr double kHandleR  = 9.0;    // handle radius
constexpr double kGrabR    = 16.0;   // grab distance
const char* kLabels[SplitToneWheel::kZones] = {"H", "M", "S"};

// CIELAB (D65) → sRGB, for painting the wheel. a*/b* fed in display units.
QColor labToColor(double L, double a, double b) {
    auto finv = [](double t) {
        return t > 6.0 / 29.0 ? t * t * t : 3.0 * (6.0 / 29.0) * (6.0 / 29.0) * (t - 4.0 / 29.0);
    };
    double fy = (L + 16.0) / 116.0;
    double fx = fy + a / 500.0;
    double fz = fy - b / 200.0;
    double X = 0.95047 * finv(fx);
    double Y = 1.00000 * finv(fy);
    double Z = 1.08883 * finv(fz);
    double r =  3.2406 * X - 1.5372 * Y - 0.4986 * Z;
    double g = -0.9689 * X + 1.8758 * Y + 0.0415 * Z;
    double bl =  0.0557 * X - 0.2040 * Y + 1.0570 * Z;
    auto enc = [](double c) {
        c = std::clamp(c, 0.0, 1.0);
        return c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
    };
    return QColor::fromRgbF(enc(r), enc(g), enc(bl));
}
}  // namespace

SplitToneWheel::SplitToneWheel(QWidget* parent) : QWidget(parent) {
    setMinimumSize(200, 200);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

void SplitToneWheel::setZone(int zone, float a, float b) {
    if (zone < 0 || zone >= kZones) return;
    Pt p{a, b};
    double r = std::hypot(p.a, p.b);
    if (r > 1.0) { p.a /= r; p.b /= r; }
    pts_[zone] = p;
    update();
}

void SplitToneWheel::resetAll() {
    for (auto& p : pts_) p = Pt{};
    update();
}

QPointF SplitToneWheel::centre() const {
    return QPointF(width() / 2.0, height() / 2.0);
}

double SplitToneWheel::radiusPx() const {
    return std::min(width(), height()) / 2.0 - kMargin;
}

QPointF SplitToneWheel::toScreen(const Pt& p) const {
    QPointF c = centre();
    double R = radiusPx();
    return QPointF(c.x() + p.a * R, c.y() - p.b * R);   // b+ points up
}

SplitToneWheel::Pt SplitToneWheel::fromScreen(const QPointF& s) const {
    QPointF c = centre();
    double R = radiusPx();
    double a = (s.x() - c.x()) / R;
    double b = (c.y() - s.y()) / R;
    double r = std::hypot(a, b);
    if (r > 1.0) { a /= r; b /= r; }
    return Pt{static_cast<float>(a), static_cast<float>(b)};
}

int SplitToneWheel::hitTest(const QPointF& s) const {
    int best = -1;
    double bestD = kGrabR;
    for (int i = 0; i < kZones; ++i) {
        double d = QLineF(s, toScreen(pts_[i])).length();
        if (d <= bestD) { bestD = d; best = i; }
    }
    return best;
}

void SplitToneWheel::rebuildDisc() {
    int side = static_cast<int>(radiusPx() * 2.0);
    if (side <= 0) { disc_ = QPixmap(); discSize_ = QSize(); return; }
    QImage img(side, side, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    double R = side / 2.0;
    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            double dx = (x - R) / R;
            double dy = (R - y) / R;          // b+ up
            double dist = std::hypot(dx, dy);
            if (dist > 1.0) continue;
            // scale unit disc to ±55 a*/b* at a light L for a readable wheel
            QColor col = labToColor(78.0, dx * 55.0, dy * 55.0);
            img.setPixelColor(x, y, col);
        }
    }
    disc_ = QPixmap::fromImage(img);
    discSize_ = size();
}

void SplitToneWheel::resizeEvent(QResizeEvent*) {
    rebuildDisc();
}

void SplitToneWheel::paintEvent(QPaintEvent*) {
    if (disc_.isNull() || discSize_ != size()) rebuildDisc();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPointF c = centre();
    double R = radiusPx();

    // wheel
    QRectF discRect(c.x() - R, c.y() - R, 2 * R, 2 * R);
    if (!disc_.isNull()) p.drawPixmap(discRect.toRect(), disc_);

    // rim + neutral cross
    p.setPen(QPen(QColor("#c4c9cf"), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(discRect);
    p.setPen(QPen(QColor(0, 0, 0, 40), 1.0));
    p.drawLine(QPointF(c.x() - R, c.y()), QPointF(c.x() + R, c.y()));
    p.drawLine(QPointF(c.x(), c.y() - R), QPointF(c.x(), c.y() + R));

    // axis hints
    p.setPen(QColor(60, 60, 60, 150));
    QFont f = p.font(); f.setPointSizeF(8.0); p.setFont(f);
    p.drawText(QRectF(c.x() - 40, c.y() - R - 14, 80, 12), Qt::AlignCenter, QStringLiteral("暖 ▲"));
    p.drawText(QRectF(c.x() - 40, c.y() + R + 2, 80, 12), Qt::AlignCenter, QStringLiteral("冷 ▼"));

    // handles
    for (int i = 0; i < kZones; ++i) {
        QPointF s = toScreen(pts_[i]);
        QColor fill = labToColor(78.0, pts_[i].a * 55.0, pts_[i].b * 55.0);
        p.setPen(QPen(i == dragging_ ? QColor("#2e7dd7") : QColor("#3b3f46"),
                      i == dragging_ ? 2.4 : 1.6));
        p.setBrush(fill);
        p.drawEllipse(s, kHandleR, kHandleR);
        p.setPen(QColor(20, 22, 26));
        QFont hf = p.font(); hf.setBold(true); hf.setPointSizeF(8.5); p.setFont(hf);
        p.drawText(QRectF(s.x() - kHandleR, s.y() - kHandleR, 2 * kHandleR, 2 * kHandleR),
                   Qt::AlignCenter, QString::fromLatin1(kLabels[i]));
    }
}

void SplitToneWheel::mousePressEvent(QMouseEvent* e) {
    int hit = hitTest(e->position());
    if (hit < 0) return;
    dragging_ = hit;
    Pt p = fromScreen(e->position());
    pts_[hit] = p;
    update();
    emit zoneChanged(hit, p.a, p.b);
}

void SplitToneWheel::mouseMoveEvent(QMouseEvent* e) {
    if (dragging_ < 0) return;
    Pt p = fromScreen(e->position());
    pts_[dragging_] = p;
    update();
    emit zoneChanged(dragging_, p.a, p.b);
}

void SplitToneWheel::mouseReleaseEvent(QMouseEvent*) {
    dragging_ = -1;
    update();
}

}  // namespace ui
}  // namespace jcolour
