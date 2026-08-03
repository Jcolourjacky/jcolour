#include "icons.h"

#include <cmath>

#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>

namespace jcolour {
namespace ic {

namespace {

// Draw one icon into a painter whose coordinate system is the 24×24 viewBox.
// Strokes only (no fills) except where a filled dot reads better — matching the
// Lucide line-icon language: 2px stroke, round caps + joins.
void draw(QPainter& p, Icon name, const QColor& color) {
    QPen pen(color, 2.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    auto dot = [&](qreal x, qreal y, qreal r) {
        p.save();
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(QPointF(x, y), r, r);
        p.restore();
    };

    switch (name) {
        case Icon::Printer: {
            QPainterPath top;
            top.moveTo(6, 9);  top.lineTo(6, 2); top.lineTo(18, 2); top.lineTo(18, 9);
            p.drawPath(top);
            p.drawRoundedRect(QRectF(2, 9, 20, 8), 2, 2);
            p.drawRoundedRect(QRectF(6, 14, 12, 6), 1, 1);
            dot(18, 12, 0.9);
            break;
        }
        case Icon::Sliders: {
            p.drawLine(QPointF(4, 7),  QPointF(20, 7));
            p.drawLine(QPointF(4, 12), QPointF(20, 12));
            p.drawLine(QPointF(4, 17), QPointF(20, 17));
            p.save();
            p.setBrush(QColor(Qt::white));
            p.drawEllipse(QPointF(15, 7),  2.3, 2.3);
            p.drawEllipse(QPointF(9, 12),  2.3, 2.3);
            p.drawEllipse(QPointF(16, 17), 2.3, 2.3);
            p.restore();
            break;
        }
        case Icon::Clock: {
            p.drawEllipse(QPointF(12, 12), 9, 9);
            p.drawLine(QPointF(12, 12), QPointF(12, 7));
            p.drawLine(QPointF(12, 12), QPointF(16, 13.5));
            break;
        }
        case Icon::Settings: {
            // Cog: toothed outer ring + centre hole. Teeth are short trapezoids
            // so the silhouette reads as a gear rather than a sunburst.
            const double rIn = 5.6, rOut = 8.4, half = 0.34;  // half = tooth half-angle (rad)
            QPainterPath gear;
            for (int i = 0; i < 8; ++i) {
                double a = i * M_PI / 4.0;
                QPointF i0(12 + rIn * std::cos(a - half), 12 + rIn * std::sin(a - half));
                QPointF o0(12 + rOut * std::cos(a - half), 12 + rOut * std::sin(a - half));
                QPointF o1(12 + rOut * std::cos(a + half), 12 + rOut * std::sin(a + half));
                double an = a + M_PI / 4.0;
                QPointF i1(12 + rIn * std::cos(an - half), 12 + rIn * std::sin(an - half));
                if (i == 0) gear.moveTo(i0); else gear.lineTo(i0);
                gear.lineTo(o0);
                gear.lineTo(o1);
                gear.lineTo(i1);
            }
            gear.closeSubpath();
            p.drawPath(gear);
            p.drawEllipse(QPointF(12, 12), 2.6, 2.6);
            break;
        }
        case Icon::FilePlus: {
            QPainterPath f;
            f.moveTo(6, 2); f.lineTo(14, 2); f.lineTo(20, 8);
            f.lineTo(20, 22); f.lineTo(6, 22); f.closeSubpath();
            p.drawPath(f);
            QPainterPath fold;
            fold.moveTo(14, 2); fold.lineTo(14, 8); fold.lineTo(20, 8);
            p.drawPath(fold);
            p.drawLine(QPointF(13, 12), QPointF(13, 18));
            p.drawLine(QPointF(10, 15), QPointF(16, 15));
            break;
        }
        case Icon::FolderOpen: {
            QPainterPath f;
            f.moveTo(3, 7); f.lineTo(9, 7); f.lineTo(11, 9.5); f.lineTo(21, 9.5);
            p.drawPath(f);
            QPainterPath body;
            body.moveTo(3, 7); body.lineTo(3, 19); body.lineTo(21, 19);
            body.lineTo(21, 9.5);
            p.drawPath(body);
            break;
        }
        case Icon::Save: {
            QPainterPath o;
            o.moveTo(5, 4); o.lineTo(16, 4); o.lineTo(20, 8); o.lineTo(20, 20);
            o.lineTo(4, 20); o.lineTo(4, 5);
            o.closeSubpath();
            p.drawPath(o);
            p.drawRect(QRectF(8, 4, 7, 4));   // slider notch
            p.drawRect(QRectF(7, 13, 10, 7)); // label
            break;
        }
        case Icon::Undo: {
            QPainterPath chev;
            chev.moveTo(9, 14); chev.lineTo(4, 9); chev.lineTo(9, 4);
            p.drawPath(chev);
            QPainterPath arc;
            arc.moveTo(4, 9); arc.lineTo(14.5, 9);
            arc.cubicTo(18, 9, 20, 11.5, 20, 14.5);
            arc.cubicTo(20, 17.5, 18, 20, 14.5, 20);
            arc.lineTo(11, 20);
            p.drawPath(arc);
            break;
        }
        case Icon::Redo: {
            QPainterPath chev;
            chev.moveTo(15, 14); chev.lineTo(20, 9); chev.lineTo(15, 4);
            p.drawPath(chev);
            QPainterPath arc;
            arc.moveTo(20, 9); arc.lineTo(9.5, 9);
            arc.cubicTo(6, 9, 4, 11.5, 4, 14.5);
            arc.cubicTo(4, 17.5, 6, 20, 9.5, 20);
            arc.lineTo(13, 20);
            p.drawPath(arc);
            break;
        }
        case Icon::ImagePlus: {
            p.drawRoundedRect(QRectF(3, 4, 14, 14), 2, 2);
            QPainterPath m;
            m.moveTo(3.5, 15); m.lineTo(7.5, 11); m.lineTo(11, 14.5);
            m.lineTo(13.5, 12); m.lineTo(16.5, 15.5);
            p.drawPath(m);
            dot(7, 8, 1.4);
            p.drawLine(QPointF(20, 4), QPointF(20, 10));
            p.drawLine(QPointF(17, 7), QPointF(23, 7));
            break;
        }
        case Icon::Eye: {
            QPainterPath e;
            e.moveTo(2, 12); e.quadTo(12, 4, 22, 12); e.quadTo(12, 20, 2, 12);
            p.drawPath(e);
            p.drawEllipse(QPointF(12, 12), 3, 3);
            break;
        }
        case Icon::AlertTriangle: {
            QPainterPath t;
            t.moveTo(12, 3.5); t.lineTo(21.5, 20); t.lineTo(2.5, 20);
            t.closeSubpath();
            p.drawPath(t);
            p.drawLine(QPointF(12, 9.5), QPointF(12, 14.5));
            dot(12, 17.5, 1.0);
            break;
        }
        case Icon::Grid: {
            p.drawRoundedRect(QRectF(3, 3, 8, 8), 1.2, 1.2);
            p.drawRoundedRect(QRectF(13, 3, 8, 8), 1.2, 1.2);
            p.drawRoundedRect(QRectF(3, 13, 8, 8), 1.2, 1.2);
            p.drawRoundedRect(QRectF(13, 13, 8, 8), 1.2, 1.2);
            break;
        }
        case Icon::Minus: {
            p.drawLine(QPointF(5, 12), QPointF(19, 12));
            break;
        }
        case Icon::Plus: {
            p.drawLine(QPointF(12, 5), QPointF(12, 19));
            p.drawLine(QPointF(5, 12), QPointF(19, 12));
            break;
        }
    }
}

}  // namespace

QPixmap pixmap(Icon name, const QColor& color, int px, qreal dpr) {
    QPixmap pm(QSize(px, px) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(px / 24.0, px / 24.0);
    draw(p, name, color);
    return pm;
}

QIcon icon(Icon name, const QColor& color, int px) {
    QIcon out;
    out.addPixmap(pixmap(name, color, px, 1.0));
    out.addPixmap(pixmap(name, color, px, 2.0));
    return out;
}

QIcon toggleIcon(Icon name, const QColor& off, const QColor& on, int px) {
    QIcon out;
    for (qreal dpr : {1.0, 2.0}) {
        out.addPixmap(pixmap(name, off, px, dpr), QIcon::Normal, QIcon::Off);
        out.addPixmap(pixmap(name, on,  px, dpr), QIcon::Normal, QIcon::On);
    }
    return out;
}

}  // namespace ic
}  // namespace jcolour
