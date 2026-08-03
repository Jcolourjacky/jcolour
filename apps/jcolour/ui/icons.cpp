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

        // ── 左工具条 ──
        case Icon::Cursor: {   // 指针箭头
            QPainterPath a;
            a.moveTo(6, 3); a.lineTo(6, 18); a.lineTo(10.2, 14.4);
            a.lineTo(13, 20.5); a.lineTo(15.6, 19.2); a.lineTo(12.8, 13.2);
            a.lineTo(18, 12.6); a.closeSubpath();
            p.drawPath(a);
            break;
        }
        case Icon::Type: {     // T
            p.drawLine(QPointF(5, 5), QPointF(19, 5));
            p.drawLine(QPointF(12, 5), QPointF(12, 19));
            p.drawLine(QPointF(9, 19), QPointF(15, 19));
            break;
        }
        case Icon::QrCode: {
            p.drawRect(QRectF(4, 4, 6, 6));
            p.drawRect(QRectF(14, 4, 6, 6));
            p.drawRect(QRectF(4, 14, 6, 6));
            dot(7, 7, 1.1); dot(17, 7, 1.1); dot(7, 17, 1.1);
            p.drawLine(QPointF(14, 14), QPointF(14, 20));
            p.drawLine(QPointF(14, 17), QPointF(20, 17));
            p.drawLine(QPointF(20, 14), QPointF(20, 20));
            break;
        }
        case Icon::Hand: {     // 抓手（掌+三指）
            QPainterPath h;
            h.moveTo(7, 12); h.lineTo(7, 7);
            h.arcTo(QRectF(7, 5, 3.2, 4), 180, -180);
            h.lineTo(10.2, 12);
            h.moveTo(10.2, 11); h.lineTo(10.2, 5.5);
            h.arcTo(QRectF(10.2, 3.6, 3.2, 3.8), 180, -180);
            h.lineTo(13.4, 11);
            h.moveTo(13.4, 11); h.lineTo(13.4, 6.5);
            h.arcTo(QRectF(13.4, 4.8, 3.2, 3.6), 180, -180);
            h.lineTo(16.6, 12.5);
            p.drawPath(h);
            QPainterPath palm;
            palm.moveTo(7, 12);
            palm.lineTo(7, 15.5);
            palm.quadTo(7, 20, 12, 20);
            palm.quadTo(16.6, 20, 16.6, 15.5);
            palm.lineTo(16.6, 12.5);
            p.drawPath(palm);
            break;
        }
        case Icon::Measure: {  // 对角尺 + 刻度
            p.save();
            p.translate(12, 12); p.rotate(-45); p.translate(-12, -12);
            p.drawRoundedRect(QRectF(3, 9, 18, 6), 1.5, 1.5);
            p.drawLine(QPointF(7, 9),  QPointF(7, 12));
            p.drawLine(QPointF(11, 9), QPointF(11, 12));
            p.drawLine(QPointF(15, 9), QPointF(15, 12));
            p.restore();
            break;
        }
        case Icon::Copy: {     // 两层矩形
            p.drawRoundedRect(QRectF(8, 8, 12, 12), 2, 2);
            QPainterPath c;
            c.moveTo(5, 15); c.lineTo(4, 15);
            c.quadTo(3, 15, 3, 14); c.lineTo(3, 5);
            c.quadTo(3, 4, 4, 4); c.lineTo(13, 4);
            c.quadTo(14, 4, 14, 5); c.lineTo(14, 5.5);
            p.drawPath(c);
            break;
        }
        case Icon::Copies: {   // 叠层 + 加号（×N 份）
            p.drawRoundedRect(QRectF(3, 3, 11, 11), 2, 2);
            p.drawLine(QPointF(17, 12), QPointF(17, 20));
            p.drawLine(QPointF(13, 16), QPointF(21, 16));
            p.drawLine(QPointF(6.5, 17.5), QPointF(10, 17.5));
            break;
        }
        case Icon::RotateCw: { // 顺时针弧箭头
            QPainterPath arc;
            arc.moveTo(19.5, 9.5);
            arc.arcTo(QRectF(4.5, 4.5, 15, 15), 55, 290);
            p.drawPath(arc);
            p.drawLine(QPointF(19.5, 4.5), QPointF(19.5, 9.5));
            p.drawLine(QPointF(19.5, 9.5), QPointF(14.8, 9.2));
            break;
        }
        case Icon::Trash: {
            p.drawLine(QPointF(4, 7), QPointF(20, 7));
            p.drawLine(QPointF(9, 7), QPointF(9, 4.5));
            p.drawLine(QPointF(9, 4.5), QPointF(15, 4.5));
            p.drawLine(QPointF(15, 4.5), QPointF(15, 7));
            QPainterPath bin;
            bin.moveTo(6, 7); bin.lineTo(7, 20); bin.lineTo(17, 20); bin.lineTo(18, 7);
            p.drawPath(bin);
            p.drawLine(QPointF(10.2, 10.5), QPointF(10.5, 17));
            p.drawLine(QPointF(13.8, 10.5), QPointF(13.5, 17));
            break;
        }
        case Icon::AlignLeftIc: {  // 左对齐线组
            p.drawLine(QPointF(4, 5),  QPointF(4, 19));
            p.drawLine(QPointF(7.5, 8),  QPointF(20, 8));
            p.drawLine(QPointF(7.5, 12), QPointF(15, 12));
            p.drawLine(QPointF(7.5, 16), QPointF(18, 16));
            break;
        }
        case Icon::StepRepeat: {   // 2×2 网格
            p.drawRoundedRect(QRectF(4, 4, 7, 7), 1.5, 1.5);
            p.drawRoundedRect(QRectF(13, 4, 7, 7), 1.5, 1.5);
            p.drawRoundedRect(QRectF(4, 13, 7, 7), 1.5, 1.5);
            p.drawRoundedRect(QRectF(13, 13, 7, 7), 1.5, 1.5);
            break;
        }
        case Icon::ContactSheet: { // 表格（联系表）
            p.drawRoundedRect(QRectF(3.5, 4.5, 17, 15), 2, 2);
            p.drawLine(QPointF(3.5, 10), QPointF(20.5, 10));
            p.drawLine(QPointF(3.5, 14.8), QPointF(20.5, 14.8));
            p.drawLine(QPointF(9.2, 10), QPointF(9.2, 19.5));
            p.drawLine(QPointF(14.9, 10), QPointF(14.9, 19.5));
            break;
        }
        case Icon::CropMarks: {    // 裁切角线（crop）
            p.drawLine(QPointF(7, 3), QPointF(7, 17));
            p.drawLine(QPointF(7, 17), QPointF(21, 17));
            p.drawLine(QPointF(3, 7), QPointF(17, 7));
            p.drawLine(QPointF(17, 7), QPointF(17, 21));
            break;
        }
        case Icon::Grid3: {        // 3×3 网格
            p.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
            p.drawLine(QPointF(9.3, 4),  QPointF(9.3, 20));
            p.drawLine(QPointF(14.7, 4), QPointF(14.7, 20));
            p.drawLine(QPointF(4, 9.3),  QPointF(20, 9.3));
            p.drawLine(QPointF(4, 14.7), QPointF(20, 14.7));
            break;
        }
        case Icon::RulerIc: {      // 角尺（L 形标尺）
            QPainterPath l;
            l.moveTo(4, 4); l.lineTo(9, 4); l.lineTo(9, 15); l.lineTo(20, 15);
            l.lineTo(20, 20); l.lineTo(4, 20); l.closeSubpath();
            p.drawPath(l);
            p.drawLine(QPointF(6.5, 8),  QPointF(9, 8));
            p.drawLine(QPointF(6.5, 12), QPointF(9, 12));
            p.drawLine(QPointF(12, 17.5), QPointF(12, 15));
            p.drawLine(QPointF(16, 17.5), QPointF(16, 15));
            break;
        }
        case Icon::ZoomIn:
        case Icon::ZoomOut: {
            p.drawEllipse(QPointF(10.5, 10.5), 7, 7);
            p.drawLine(QPointF(15.6, 15.6), QPointF(20.5, 20.5));
            p.drawLine(QPointF(7.5, 10.5), QPointF(13.5, 10.5));
            if (name == Icon::ZoomIn)
                p.drawLine(QPointF(10.5, 7.5), QPointF(10.5, 13.5));
            break;
        }
        case Icon::FitView: {      // 四角向外（适配）
            p.drawLine(QPointF(4, 9),  QPointF(4, 4));  p.drawLine(QPointF(4, 4),  QPointF(9, 4));
            p.drawLine(QPointF(15, 4), QPointF(20, 4)); p.drawLine(QPointF(20, 4), QPointF(20, 9));
            p.drawLine(QPointF(20, 15),QPointF(20, 20));p.drawLine(QPointF(20, 20),QPointF(15, 20));
            p.drawLine(QPointF(9, 20), QPointF(4, 20)); p.drawLine(QPointF(4, 20), QPointF(4, 15));
            break;
        }
        case Icon::LinkIc: {       // 链环（等比锁定）
            p.save();
            p.translate(12, 12); p.rotate(-45); p.translate(-12, -12);
            p.drawRoundedRect(QRectF(4, 9, 9, 6), 3, 3);
            p.drawRoundedRect(QRectF(11, 9, 9, 6), 3, 3);
            p.restore();
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
