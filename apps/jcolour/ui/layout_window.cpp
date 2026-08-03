#include "layout_window.h"
#include "print_env_manager.h"
#include "page_measurement_devices.h"
#include "icons.h"
#include "../paper_state.h"
#include "../queue.h"
#include "../nesting.h"
#include "../step_repeat.h"
#include "../contact_sheet.h"
#include "state_machine.h"

#include <QInputDialog>
#include <QDialog>

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QToolButton>
#include <QCheckBox>
#include <QSlider>
#include <QProgressBar>
#include <QStackedWidget>
#include <QTabWidget>
#include <QFontComboBox>
#include <QScrollArea>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDateTime>
#include <QFileDialog>

namespace cal = jcolour::calibration;

namespace jcolour::ui {

// ============================================================================
// shared mode table — the 5 print modes, each linearization curve is bound to
// resolution + direction (写进名字), depth differs per mode (user spec).
// ============================================================================
namespace {

constexpr double kPaperW = 609.6, kPaperH = 914.4;   // mm
constexpr int    kRuler  = 18;                        // px

struct ModeDef {
    const char* key;
    const char* name;
    const char* lin;     // resolution · direction the curve is bound to
    const char* depth;   // 线性 / 线性+G7 / 线性+G7+ICC
    int         days;    // demo age of this mode's curve
};
const ModeDef kModes[] = {
    {"mono",  "单黑",     "1440×720 · 单向",  "线性",             9},
    {"gray",  "黑+灰",    "2880×1440 · 双向", "线性",             9},
    {"tone",  "黑+灰+彩", "2880×1440 · 双向", "线性 + G7",        9},
    {"rgb",   "RGB",      "2880×1440 · 双向", "线性 + G7 + ICC", 35},
    {"rgb16", "16位RGB",  "2880×1440 · 单向", "线性 + G7 + ICC",  9},
};
const ModeDef* modeDef(const QString& key) {
    for (const auto& m : kModes) if (key == m.key) return &m;
    return &kModes[2];
}
// engine decides freshness from the curve age — single source of truth
cal::CalibState modeState(const QString& key) {
    const ModeDef* m = modeDef(key);
    const cal::Timestamp now = QDateTime::currentSecsSinceEpoch();
    cal::ParamSet ps;
    ps.state = cal::CalibState::FRESH;
    ps.key = {"#A7", "photo-rag-308"};
    ps.calibrated_at = now - cal::Timestamp(m->days) * 86400;
    return cal::compute_state(ps, now, cal::InkBatch{"#A7"}, cal::Paper{"photo-rag-308"});
}

struct ImgInfo { const char* size; const char* icc; bool warn; const char* depth; const char* dpi; };
ImgInfo imgInfo(const QString& name) {
    if (name == "landscape_02") return {"7200×5040px",  "ProPhoto RGB",   false, "16-bit", "300 dpi"};
    if (name == "detail_03")    return {"3600×3720px",  "Gray Gamma 2.2", false, "16-bit", "360 dpi"};
    if (name == "abstract_04")  return {"5400×7392px",  "ProPhoto RGB",   false, "16-bit", "300 dpi"};
    if (name == "seascape_05")  return {"7900×5228px",  "sRGB",           true,  "8-bit",  "240 dpi"};
    return {"8400×10500px", "AdobeRGB", true, "16-bit", "350 dpi"};
}

QToolButton* glyphBtn(const QString& glyph, const QString& tip, QWidget* parent = nullptr) {
    auto* b = new QToolButton(parent);
    b->setText(glyph);
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setFixedSize(24, 22);
    return b;
}

// Toolbar icon button using the shared vector icon set. Checkable buttons get a
// two-state icon (gray → accent) so the active toggle reads clearly.
QToolButton* iconBtn(ic::Icon glyph, const QString& tip, QWidget* parent = nullptr) {
    static const QColor kOff(0x4a, 0x4f, 0x57);
    static const QColor kOn(0x2e, 0x7d, 0xd7);
    auto* b = new QToolButton(parent);
    b->setIcon(ic::toggleIcon(glyph, kOff, kOn, 17));
    b->setIconSize(QSize(17, 17));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setFixedSize(26, 24);
    return b;
}
QFrame* vsep() {
    auto* f = new QFrame;
    f->setFrameShape(QFrame::VLine);
    f->setStyleSheet("color:#cfd2d6;");
    f->setFixedHeight(16);
    return f;
}

}  // namespace

// ============================================================================
// LayoutCanvas
// ============================================================================

LayoutCanvas::LayoutCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(360, 300);
    // mm layout identical to the design mockup
    blocks_ = {
        {16,  17,  286, 380, QColor("#3a5468"), "portrait_01",  "tone"},
        {318, 17,  276, 193, QColor("#3a5040"), "landscape_02", "rgb16"},
        {318, 227, 154, 159, QColor("#4a3830"), "detail_03",    "mono"},
        {16,  432, 228, 312, QColor("#3c3050"), "abstract_04",  "gray"},
        {260, 432, 334, 221, QColor("#2a4840"), "seascape_05",  "rgb"},
        {16,  761, 566, 74,  QColor(0, 0, 0, 0), "标题文字",     "",  true,
         QStringLiteral("展览标题 2026 · Exhibition")},
    };
}

QRectF LayoutCanvas::paperRectPx() const {
    const double vw = width() - kRuler, vh = height() - kRuler;
    const double z  = zoom_ > 0 ? zoom_ : 0.8;
    const double pw = vw * z, ph = pw * kPaperH / kPaperW;
    const double x  = kRuler + (vw - pw) / 2.0 + scrollX_;
    double y = kRuler + 16 - scroll_;
    if (ph + 32 <= vh) y = kRuler + (vh - ph) / 2.0;   // fits → centre
    return {x, y, pw, ph};
}

QRectF LayoutCanvas::blockRectPx(const CanvasBlock& b) const {
    const QRectF p = paperRectPx();
    const double s = p.width() / kPaperW;
    return {p.x() + b.x * s, p.y() + b.y * s, b.w * s, b.h * s};
}

QPointF LayoutCanvas::pxToMm(const QPointF& px) const {
    const QRectF p = paperRectPx();
    const double s = p.width() / kPaperW;
    return {(px.x() - p.x()) / s, (px.y() - p.y()) / s};
}

int LayoutCanvas::handleAt(const QPointF& px) const {
    if (sel_ < 0 || sel_ >= blocks_.size()) return -1;
    const QRectF r = blockRectPx(blocks_[sel_]);
    const QPointF c[8] = {
        r.topLeft(), {r.center().x(), r.top()}, r.topRight(),
        {r.left(), r.center().y()}, {r.right(), r.center().y()},
        r.bottomLeft(), {r.center().x(), r.bottom()}, r.bottomRight()};
    for (int i = 0; i < 8; ++i)
        if (QRectF(c[i].x() - 5, c[i].y() - 5, 10, 10).contains(px)) return i;
    return -1;
}

void LayoutCanvas::clampScroll() {
    const double vh = height() - kRuler;
    const double ph = paperRectPx().height();
    const double maxS = qMax(0.0, ph + 32 - vh);
    scroll_ = qBound(0.0, scroll_, maxS);
    // 横向：纸张比视口宽时允许平移，最多露出纸缘 40px
    const double vw = width() - kRuler;
    const double pw = vw * (zoom_ > 0 ? zoom_ : 0.8);
    const double maxX = qMax(0.0, (pw - vw) / 2.0 + 40);
    scrollX_ = qBound(-maxX, scrollX_, maxX);
}

void LayoutCanvas::select(int idx) {
    if (idx == sel_) return;
    sel_ = idx;
    update();
    emit selectionChanged(sel_);
}

int LayoutCanvas::zoomPercent() const {
    return int((zoom_ > 0 ? zoom_ : 0.8) / 0.8 * 100 + 0.5);
}

void LayoutCanvas::zoomBy(int dp) {
    double z = (zoom_ > 0 ? zoom_ : 0.8) / 0.8 * 100.0 + dp;
    z = qBound(10.0, z, 300.0);
    zoom_ = z / 100.0 * 0.8;
    clampScroll();
    update();
    emit zoomChanged(zoomPercent());
}

void LayoutCanvas::zoomFit() {
    const double vw = width() - kRuler - 32, vh = height() - kRuler - 32;
    if (vw <= 0 || vh <= 0) return;
    const double targetW = qMin(vw, vh * kPaperW / kPaperH);
    zoom_ = targetW / (width() - kRuler);
    scroll_ = 0;
    update();
    emit zoomChanged(zoomPercent());
}

void LayoutCanvas::paintEvent(QPaintEvent*) {
    if (zoom_ <= 0) { zoom_ = 0.0001; zoomFit(); }
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);

    // checkerboard workspace
    g.fillRect(rect(), QColor("#e9ebee"));
    for (int yy = 0; yy < height(); yy += 22)
        for (int xx = ((yy / 22) % 2) * 22; xx < width(); xx += 44)
            g.fillRect(xx, yy, 22, 22, QColor("#dfe1e4"));

    const QRectF p = paperRectPx();
    const double s = p.width() / kPaperW;

    // paper shadow + sheet
    g.setPen(Qt::NoPen);
    g.setBrush(QColor(0, 0, 0, 36));
    g.drawRoundedRect(p.adjusted(-1, 2, 5, 7), 2, 2);
    g.setBrush(Qt::white);
    g.setPen(QColor(0, 0, 0, 30));
    g.drawRect(p);
    g.setPen(QColor(0, 0, 0, 82));
    QFont f = g.font(); f.setPointSizeF(8); g.setFont(f);
    g.drawText(QRectF(p.x(), p.y() + 3, p.width(), 14), Qt::AlignHCenter,
               QStringLiteral("609.6 × 914.4 mm · Photo Rag 308gsm"));

    // blocks
    for (int i = 0; i < blocks_.size(); ++i) {
        const CanvasBlock& b = blocks_[i];
        const QRectF r = blockRectPx(b);
        if (b.isText) {
            QFont tf = g.font();
            tf.setPointSizeF(qMax(7.0, r.height() * 0.42));
            tf.setBold(true);
            g.setFont(tf);
            g.setPen(QColor("#2b2e33"));
            g.drawText(r, Qt::AlignVCenter | Qt::AlignLeft, "  " + b.text);
        } else if (b.isQr) {
            // QR placeholder — deterministic modules from the payload hash
            const double side = qMin(r.width(), r.height());
            const QRectF q(r.center().x() - side / 2, r.center().y() - side / 2, side, side);
            g.setPen(QColor(0, 0, 0, 26));
            g.setBrush(Qt::white);
            g.drawRect(q);
            g.setPen(Qt::NoPen);
            g.setBrush(QColor("#1a1c1f"));
            const int n = 21;
            const double m = side / n;
            uint hsh = qHash(b.qrData);
            for (int yy = 0; yy < n; ++yy)
                for (int xx = 0; xx < n; ++xx) {
                    hsh = hsh * 1664525u + 1013904223u;
                    const bool finder = (xx < 7 && yy < 7) || (xx >= n - 7 && yy < 7) ||
                                        (xx < 7 && yy >= n - 7);
                    bool onBit;
                    if (finder) {
                        const int fx = (xx >= n - 7) ? xx - (n - 7) : xx;
                        const int fy = (yy >= n - 7) ? yy - (n - 7) : yy;
                        onBit = (fx == 0 || fx == 6 || fy == 0 || fy == 6) ||
                                (fx >= 2 && fx <= 4 && fy >= 2 && fy <= 4);
                    } else {
                        onBit = (hsh >> 13) & 1;
                    }
                    if (onBit)
                        g.drawRect(QRectF(q.x() + xx * m, q.y() + yy * m, m + 0.4, m + 0.4));
                }
        } else {
            g.setPen(QColor(0, 0, 0, 26));
            g.setBrush(b.color);
            g.drawRect(r);
            // photo glyph
            g.setPen(QPen(QColor(255, 255, 255, 110), 1.4));
            g.setBrush(Qt::NoBrush);
            const double ic = qMin(r.width(), r.height()) * 0.16;
            const QRectF gr(r.center().x() - ic, r.center().y() - ic * 0.8, ic * 2, ic * 1.6);
            g.drawRoundedRect(gr, 2, 2);
            QPainterPath mt(QPointF(gr.x() + 2, gr.bottom() - 3));
            mt.lineTo(gr.x() + gr.width() * 0.38, gr.y() + gr.height() * 0.35);
            mt.lineTo(gr.x() + gr.width() * 0.62, gr.bottom() - 6);
            mt.lineTo(gr.x() + gr.width() * 0.78, gr.y() + gr.height() * 0.55);
            mt.lineTo(gr.right() - 2, gr.bottom() - 3);
            g.drawPath(mt);
        }
        // name label under block
        QFont lf = g.font(); lf.setPointSizeF(7); lf.setBold(false); g.setFont(lf);
        g.setPen(QColor(0, 0, 0, 120));
        const QString lbl = (i == 0)
            ? QStringLiteral("%1 · %2×%3mm").arg(b.name).arg(int(b.w)).arg(int(b.h))
            : b.name;
        g.drawText(QRectF(r.x(), r.bottom() + 1, r.width(), 12), Qt::AlignHCenter, lbl);

        if (i == sel_) {
            g.setPen(QPen(QColor("#2e7dd7"), 1.6));
            g.setBrush(Qt::NoBrush);
            g.drawRect(r);
            QPen dash(QColor(46, 125, 215, 115), 1, Qt::DashLine);
            g.setPen(dash);
            g.drawRect(r.adjusted(-5, -5, 5, 5));
            g.setPen(QPen(Qt::white, 1));
            g.setBrush(QColor("#2e7dd7"));
            const QPointF c[8] = {
                r.topLeft(), {r.center().x(), r.top()}, r.topRight(),
                {r.left(), r.center().y()}, {r.right(), r.center().y()},
                r.bottomLeft(), {r.center().x(), r.bottom()}, r.bottomRight()};
            for (const auto& pt : c) g.drawEllipse(pt, 3.4, 3.4);
        }
    }

    // 利用率 tag
    g.setPen(Qt::NoPen);
    g.setBrush(QColor(0, 0, 0, 140));
    const QRectF ut(p.right() - 74, p.bottom() - 20, 68, 15);
    g.drawRoundedRect(ut, 3, 3);
    g.setPen(Qt::white);
    QFont uf = g.font(); uf.setPointSizeF(7.5); g.setFont(uf);
    g.drawText(ut, Qt::AlignCenter, QStringLiteral("利用率 81%"));

    // 50mm 网格（纸内，随缩放）
    if (showGrid_) {
        g.setPen(QPen(QColor(0, 120, 200, 46), 1));
        for (int mm = 50; mm < int(kPaperW); mm += 50)
            g.drawLine(QPointF(p.x() + mm * s, p.y()), QPointF(p.x() + mm * s, p.bottom()));
        for (int mm = 50; mm < int(kPaperH); mm += 50)
            g.drawLine(QPointF(p.x(), p.y() + mm * s), QPointF(p.right(), p.y() + mm * s));
    }

    // 裁切角线（每块四角，打印后裁纸用）
    if (showCropMarks_) {
        g.setPen(QPen(QColor("#23262b"), 1));
        const double L = 4.0 * s, gap = 1.2 * s;   // 4mm 线 · 1.2mm 离空
        for (const auto& b : blocks_) {
            if (b.isText) continue;
            const QRectF r = blockRectPx(b);
            const auto mark = [&](QPointF c, int dx, int dy) {
                g.drawLine(QPointF(c.x() + dx * gap, c.y()),
                           QPointF(c.x() + dx * (gap + L), c.y()));
                g.drawLine(QPointF(c.x(), c.y() + dy * gap),
                           QPointF(c.x(), c.y() + dy * (gap + L)));
            };
            mark(r.topLeft(), -1, -1);
            mark(r.topRight(), +1, -1);
            mark(r.bottomLeft(), -1, +1);
            mark(r.bottomRight(), +1, +1);
        }
    }

    // 参考线（InDesign 青色，横竖贯穿画布内容区）
    if (!guidesV_.isEmpty() || !guidesH_.isEmpty()) {
        g.setPen(QPen(QColor("#37b6f0"), 1));
        for (double gm : guidesV_) {
            const double x = p.x() + gm * s;
            g.drawLine(QPointF(x, kRuler), QPointF(x, height()));
        }
        for (double gm : guidesH_) {
            const double y = p.y() + gm * s;
            g.drawLine(QPointF(kRuler, y), QPointF(width(), y));
        }
        // 拖动中的参考线显示位置读数
        if (dragGuideKind_ != 0) {
            const auto& list = (dragGuideKind_ == 1) ? guidesV_ : guidesH_;
            const int idx = dragGuideIdx_ >= 0 ? dragGuideIdx_ : list.size() - 1;
            const double v = list[idx];
            g.setPen(Qt::white);
            const QString txt = QStringLiteral("%1 mm").arg(v, 0, 'f', 1);
            const QRectF tr(cursorPx_.x() + 10, cursorPx_.y() - 22, 74, 17);
            g.setBrush(QColor("#37b6f0"));
            g.setPen(Qt::NoPen);
            g.drawRoundedRect(tr, 3, 3);
            g.setPen(Qt::white);
            g.drawText(tr, Qt::AlignCenter, txt);
        }
    }

    // 测量工具读数（距离 + Δx/Δy）
    if (hasMeasure_) {
        const QPointF a(p.x() + measA_.x() * s, p.y() + measA_.y() * s);
        const QPointF b(p.x() + measB_.x() * s, p.y() + measB_.y() * s);
        g.setPen(QPen(QColor("#e05515"), 1.4));
        g.drawLine(a, b);
        g.setBrush(QColor("#e05515"));
        g.setPen(Qt::NoPen);
        g.drawEllipse(a, 2.5, 2.5);
        g.drawEllipse(b, 2.5, 2.5);
        const double dx = measB_.x() - measA_.x(), dy = measB_.y() - measA_.y();
        const QString txt = QStringLiteral("%1 mm  (Δx %2 · Δy %3)")
            .arg(std::hypot(dx, dy), 0, 'f', 1).arg(dx, 0, 'f', 1).arg(dy, 0, 'f', 1);
        const QRectF tr((a.x() + b.x()) / 2 + 8, (a.y() + b.y()) / 2 - 24, 172, 17);
        g.setBrush(QColor(35, 38, 43, 230));
        g.drawRoundedRect(tr, 3, 3);
        g.setPen(Qt::white);
        QFont mf = g.font(); mf.setPointSizeF(8); g.setFont(mf);
        g.drawText(tr, Qt::AlignCenter, txt);
    }

    // ── InDesign 式标尺：自适应刻度 · 单位切换 · 光标位置指示 ──
    if (!showRulers_) return;
    g.setPen(Qt::NoPen);
    g.setBrush(QColor("#eef0f2"));
    g.drawRect(QRectF(kRuler, 0, width() - kRuler, kRuler));
    g.drawRect(QRectF(0, kRuler, kRuler, height() - kRuler));
    g.drawRect(QRectF(0, 0, kRuler, kRuler));
    g.setPen(QColor("#d2d5d9"));
    g.drawLine(0, kRuler, width(), kRuler);
    g.drawLine(kRuler, 0, kRuler, height());

    // 单位与刻度：小刻度 ≥6px、标签刻度 ≥46px，随缩放加密/抽稀
    const double unitMm = (unit_ == Unit::MM) ? 1.0 : (unit_ == Unit::CM) ? 10.0 : 25.4;
    static const double kSteps[] = {0.1, 0.25, 0.5, 1, 2, 5, 10, 25, 50, 100, 200};
    double minorU = 200;
    for (double st : kSteps) if (st * unitMm * s >= 6) { minorU = st; break; }
    int labelEvery = 1;                            // 标签 = minor 的整数倍
    for (int r : {1, 2, 4, 5, 10, 20, 50})
        if (minorU * r * unitMm * s >= 46) { labelEvery = r; break; }
    const auto fmt = [this](double u) {
        return (unit_ == Unit::MM) ? QString::number(qRound(u)) : QStringLiteral("%1").arg(u, 0, 'g', 4);
    };

    QFont rf = g.font(); rf.setPointSizeF(6.5); g.setFont(rf);
    const int nX = int(kPaperW / unitMm / minorU) + 1;
    const int nY = int(kPaperH / unitMm / minorU) + 1;
    for (int i = 0; i <= nX; ++i) {
        const double u = i * minorU;
        const double x = p.x() + u * unitMm * s;
        if (x < kRuler || x > width()) continue;
        const bool isLabel = (i % labelEvery == 0);
        g.setPen(isLabel ? QColor("#7c828a") : QColor("#b4bac2"));
        g.drawLine(QPointF(x, kRuler - (isLabel ? 6 : 3)), QPointF(x, kRuler));
        if (isLabel)
            g.drawText(QRectF(x + 2, 1, 46, kRuler - 5), Qt::AlignLeft | Qt::AlignBottom, fmt(u));
    }
    for (int i = 0; i <= nY; ++i) {
        const double u = i * minorU;
        const double y = p.y() + u * unitMm * s;
        if (y < kRuler || y > height()) continue;
        const bool isLabel = (i % labelEvery == 0);
        g.setPen(isLabel ? QColor("#7c828a") : QColor("#b4bac2"));
        g.drawLine(QPointF(kRuler - (isLabel ? 6 : 3), y), QPointF(kRuler, y));
        if (isLabel) {
            g.save();
            g.translate(2, y + 2);
            g.rotate(90);
            g.drawText(0, 0, fmt(u));
            g.restore();
        }
    }

    // 光标位置指示（InDesign 的标尺细线）
    if (cursorPx_.x() > kRuler && cursorPx_.y() > kRuler) {
        g.setPen(QColor("#e05515"));
        g.drawLine(QPointF(cursorPx_.x(), 0), QPointF(cursorPx_.x(), kRuler));
        g.drawLine(QPointF(0, cursorPx_.y()), QPointF(kRuler, cursorPx_.y()));
    }

    // 左上角单位标（点击右键换单位）
    g.setPen(QColor("#7c828a"));
    g.drawText(QRectF(0, 0, kRuler, kRuler), Qt::AlignCenter,
               unit_ == Unit::MM ? "mm" : unit_ == Unit::CM ? "cm" : "in");
}

void LayoutCanvas::mousePressEvent(QMouseEvent* e) {
    const QPointF pos = e->position();
    setFocus();

    // ---- InDesign 式：从标尺拖出参考线（任意工具下都可用）----
    if (showRulers_ && e->button() == Qt::LeftButton) {
        if (pos.y() < kRuler && pos.x() >= kRuler) {          // 顶标尺 → 横参考线
            dragGuideKind_ = 2; dragGuideIdx_ = -1;
            guidesH_.push_back(pxToMm(pos).y());
            update(); return;
        }
        if (pos.x() < kRuler && pos.y() >= kRuler) {          // 左标尺 → 竖参考线
            dragGuideKind_ = 1; dragGuideIdx_ = -1;
            guidesV_.push_back(pxToMm(pos).x());
            update(); return;
        }
    }

    // ---- 抓手平移 ----
    if (tool_ == CanvasTool::Hand) {
        panning_ = true; panLast_ = pos;
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    // ---- 测量 ----
    if (tool_ == CanvasTool::Measure) {
        measuring_ = true; hasMeasure_ = true;
        measA_ = measB_ = pxToMm(pos);
        update(); return;
    }

    // ---- 拖动已有参考线（选择工具下，4px 命中）----
    if (tool_ == CanvasTool::Select) {
        const QRectF p = paperRectPx();
        const double s = p.width() / kPaperW;
        for (int i = 0; i < guidesV_.size(); ++i)
            if (qAbs(p.x() + guidesV_[i] * s - pos.x()) < 4) {
                dragGuideKind_ = 1; dragGuideIdx_ = i; return;
            }
        for (int i = 0; i < guidesH_.size(); ++i)
            if (qAbs(p.y() + guidesH_[i] * s - pos.y()) < 4) {
                dragGuideKind_ = 2; dragGuideIdx_ = i; return;
            }
    }

    // ---- insert tools: 文字 / 二维码 click-to-place ----
    if (tool_ == CanvasTool::Text || tool_ == CanvasTool::Qr) {
        const QPointF mm = pxToMm(pos);
        if (mm.x() >= 0 && mm.x() <= kPaperW && mm.y() >= 0 && mm.y() <= kPaperH) {
            CanvasBlock b;
            if (tool_ == CanvasTool::Text) {
                bool ok = false;
                const QString t = QInputDialog::getText(
                    this, QStringLiteral("加入文字"), QStringLiteral("文字内容："),
                    QLineEdit::Normal, QStringLiteral("展览标题 2026"), &ok);
                if (!ok || t.isEmpty()) { tool_ = CanvasTool::Select; emit toolFinished(); return; }
                b.isText = true; b.text = t;
                b.w = qMin(kPaperW * 0.6, 22.0 + t.size() * 14.0); b.h = 34;
                b.name = QStringLiteral("文字");
            } else {
                bool ok = false;
                const QString d = QInputDialog::getText(
                    this, QStringLiteral("加入二维码"), QStringLiteral("二维码内容（URL / 文本）："),
                    QLineEdit::Normal, QStringLiteral("https://jcolour.app/e/2026"), &ok);
                if (!ok || d.isEmpty()) { tool_ = CanvasTool::Select; emit toolFinished(); return; }
                b.isQr = true; b.qrData = d;
                b.w = 60; b.h = 60;
                b.name = QStringLiteral("二维码");
                b.modeKey = QStringLiteral("mono");   // QR 单黑打印
                b.color = Qt::white;
            }
            b.x = qBound(0.0, mm.x() - b.w / 2, kPaperW - b.w);
            b.y = qBound(0.0, mm.y() - b.h / 2, kPaperH - b.h);
            blocks_.push_back(b);
            sel_ = blocks_.size() - 1;
            tool_ = CanvasTool::Select;
            update();
            emit blockAdded(sel_);
            emit selectionChanged(sel_);
            emit toolFinished();
        }
        return;
    }

    // ---- resize via selection handles ----
    const int h = handleAt(pos);
    if (h >= 0) {
        resizing_ = h;
        const CanvasBlock& b = blocks_[sel_];
        startMm_ = QRectF(b.x, b.y, b.w, b.h);
        return;
    }

    // ---- select / drag ----
    for (int i = blocks_.size() - 1; i >= 0; --i) {
        const QRectF r = blockRectPx(blocks_[i]);
        if (r.contains(pos)) {
            select(i);
            dragging_ = true;
            const QRectF p = paperRectPx();
            const double s = p.width() / kPaperW;
            dragOffsetMm_ = QPointF((pos.x() - r.x()) / s, (pos.y() - r.y()) / s);
            return;
        }
    }
}

void LayoutCanvas::mouseMoveEvent(QMouseEvent* e) {
    const QPointF pos = e->position();
    cursorPx_ = pos;                       // 标尺光标位置指示
    if (showRulers_) update();

    // ---- 参考线拖动中 ----
    if (dragGuideKind_ != 0) {
        const QPointF mm = pxToMm(pos);
        auto& list = (dragGuideKind_ == 1) ? guidesV_ : guidesH_;
        const int idx = dragGuideIdx_ >= 0 ? dragGuideIdx_ : list.size() - 1;
        list[idx] = (dragGuideKind_ == 1) ? mm.x() : mm.y();
        update(); return;
    }

    // ---- 抓手平移中 ----
    if (panning_) {
        scrollX_ += pos.x() - panLast_.x();
        scroll_  -= pos.y() - panLast_.y();
        panLast_ = pos;
        clampScroll(); update(); return;
    }

    // ---- 测量中 ----
    if (measuring_) {
        measB_ = pxToMm(pos);
        update(); return;
    }

    // ---- handle-resize in progress ----
    if (resizing_ >= 0 && sel_ >= 0) {
        const QPointF mm = pxToMm(pos);
        CanvasBlock& b = blocks_[sel_];
        const double minMm = 10.0;
        double L = startMm_.left(), T = startMm_.top();
        double R = startMm_.right(), B = startMm_.bottom();
        // handle order: TL TM TR ML MR BL BM BR
        const bool left   = (resizing_ == 0 || resizing_ == 3 || resizing_ == 5);
        const bool right  = (resizing_ == 2 || resizing_ == 4 || resizing_ == 7);
        const bool top    = (resizing_ == 0 || resizing_ == 1 || resizing_ == 2);
        const bool bottom = (resizing_ == 5 || resizing_ == 6 || resizing_ == 7);
        if (left)   L = qBound(0.0, mm.x(), R - minMm);
        if (right)  R = qBound(L + minMm, mm.x(), kPaperW);
        if (top)    T = qBound(0.0, mm.y(), B - minMm);
        if (bottom) B = qBound(T + minMm, mm.y(), kPaperH);
        b.x = L; b.y = T; b.w = R - L; b.h = B - T;
        update();
        emit blockMoved(sel_);
        return;
    }

    // ---- drag-move in progress ----
    if (dragging_ && sel_ >= 0) {
        const QRectF p = paperRectPx();
        const double s = p.width() / kPaperW;
        CanvasBlock& b = blocks_[sel_];
        b.x = qBound(0.0, (pos.x() - p.x()) / s - dragOffsetMm_.x(), kPaperW - b.w);
        b.y = qBound(0.0, (pos.y() - p.y()) / s - dragOffsetMm_.y(), kPaperH - b.h);
        // 参考线吸附（1.5mm，左右缘 / 上下缘）
        for (double g : guidesV_) {
            if (qAbs(b.x - g) < 1.5)             b.x = qBound(0.0, g, kPaperW - b.w);
            else if (qAbs(b.x + b.w - g) < 1.5)  b.x = qBound(0.0, g - b.w, kPaperW - b.w);
        }
        for (double g : guidesH_) {
            if (qAbs(b.y - g) < 1.5)             b.y = qBound(0.0, g, kPaperH - b.h);
            else if (qAbs(b.y + b.h - g) < 1.5)  b.y = qBound(0.0, g - b.h, kPaperH - b.h);
        }
        update();
        emit blockMoved(sel_);
        return;
    }

    // ---- hover cursor feedback ----
    if (tool_ == CanvasTool::Hand)    { setCursor(Qt::OpenHandCursor); return; }
    if (tool_ == CanvasTool::Measure) { setCursor(Qt::CrossCursor); return; }
    if (tool_ != CanvasTool::Select) { setCursor(Qt::CrossCursor); return; }
    const int h = handleAt(pos);
    if (h == 0 || h == 7)      setCursor(Qt::SizeFDiagCursor);
    else if (h == 2 || h == 5) setCursor(Qt::SizeBDiagCursor);
    else if (h == 1 || h == 6) setCursor(Qt::SizeVerCursor);
    else if (h == 3 || h == 4) setCursor(Qt::SizeHorCursor);
    else {
        bool over = false;
        for (int i = blocks_.size() - 1; i >= 0; --i)
            if (blockRectPx(blocks_[i]).contains(pos)) { over = true; break; }
        setCursor(over ? Qt::SizeAllCursor : Qt::ArrowCursor);
    }
}

void LayoutCanvas::mouseReleaseEvent(QMouseEvent* e) {
    // 参考线：拖回标尺 = 删除（InDesign 行为）
    if (dragGuideKind_ != 0) {
        const QPointF pos = e->position();
        auto& list = (dragGuideKind_ == 1) ? guidesV_ : guidesH_;
        const int idx = dragGuideIdx_ >= 0 ? dragGuideIdx_ : list.size() - 1;
        const bool overRuler = (dragGuideKind_ == 1) ? pos.x() < kRuler : pos.y() < kRuler;
        const double v = list[idx];
        const bool offPaper = (dragGuideKind_ == 1) ? (v < 0 || v > kPaperW)
                                                    : (v < 0 || v > kPaperH);
        if (overRuler || offPaper) list.remove(idx);
        dragGuideKind_ = 0; dragGuideIdx_ = -1;
        update();
    }
    if (panning_) { panning_ = false; setCursor(Qt::OpenHandCursor); }
    measuring_ = false;
    dragging_ = false; resizing_ = -1;
}

void LayoutCanvas::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) { deleteSelected(); return; }
    if (e->matches(QKeySequence::StandardKey(QKeySequence::SelectAll))) { /* 保留 */ }
    if ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_D) { duplicateSelected(); return; }
    if (e->key() == Qt::Key_Escape && hasMeasure_) { hasMeasure_ = false; update(); return; }
    QWidget::keyPressEvent(e);
}

void LayoutCanvas::contextMenuEvent(QContextMenuEvent* e) {
    // 标尺区右键：单位切换 + 清除参考线（InDesign 行为）
    if (!(e->pos().x() < kRuler || e->pos().y() < kRuler)) { QWidget::contextMenuEvent(e); return; }
    QMenu menu(this);
    const struct { const char* label; Unit u; } units[] = {
        {"毫米 (mm)", Unit::MM}, {"厘米 (cm)", Unit::CM}, {"英寸 (inch)", Unit::Inch},
    };
    for (const auto& it : units) {
        auto* a = menu.addAction(QString::fromUtf8(it.label), this,
                                 [this, u = it.u] { setUnit(u); });
        a->setCheckable(true);
        a->setChecked(unit_ == it.u);
    }
    menu.addSeparator();
    menu.addAction(QStringLiteral("清除所有参考线"), this, [this] { clearGuides(); });
    menu.exec(e->globalPos());
}

void LayoutCanvas::wheelEvent(QWheelEvent* e) {
    if (e->modifiers() & Qt::ControlModifier) {
        zoomBy(e->angleDelta().y() > 0 ? 10 : -10);
    } else {
        scroll_  -= e->angleDelta().y() * 0.6;
        scrollX_ += e->angleDelta().x() * 0.6;   // 触控板横向双指
        clampScroll();
        update();
    }
}

void LayoutCanvas::resizeEvent(QResizeEvent*) { clampScroll(); }

void LayoutCanvas::duplicateSelected() {
    if (sel_ < 0 || sel_ >= blocks_.size()) return;
    CanvasBlock b = blocks_[sel_];
    b.x = qMin(b.x + 12.0, kPaperW - b.w);   // 错位 12mm，不越纸
    b.y = qMin(b.y + 12.0, kPaperH - b.h);
    blocks_.push_back(b);
    sel_ = blocks_.size() - 1;
    update();
    emit blockAdded(sel_);
    emit selectionChanged(sel_);
}

void LayoutCanvas::duplicateSelectedN(int copies) {
    if (sel_ < 0 || sel_ >= blocks_.size() || copies < 1) return;
    const CanvasBlock src = blocks_[sel_];
    for (int i = 0; i < copies; ++i) {
        CanvasBlock b = src;
        const double d = 12.0 * (i + 1);
        b.x = qMin(src.x + d, kPaperW - b.w);   // 阶梯错位，之后一键自动拼版归位
        b.y = qMin(src.y + d, kPaperH - b.h);
        blocks_.push_back(b);
    }
    sel_ = blocks_.size() - 1;
    update();
    emit blockAdded(sel_);
    emit selectionChanged(sel_);
}

void LayoutCanvas::rotateSelected90() {
    if (sel_ < 0 || sel_ >= blocks_.size()) return;
    auto& b = blocks_[sel_];
    std::swap(b.w, b.h);
    b.x = qBound(0.0, b.x, kPaperW - b.w);   // 旋转后拉回纸内
    b.y = qBound(0.0, b.y, kPaperH - b.h);
    update();
    emit blockMoved(sel_);
}

void LayoutCanvas::deleteSelected() {
    if (sel_ < 0 || sel_ >= blocks_.size()) return;
    blocks_.remove(sel_);
    sel_ = -1;
    update();
    emit selectionChanged(-1);
}

void LayoutCanvas::alignSelected(Align a) {
    if (sel_ < 0 || sel_ >= blocks_.size()) return;
    auto& b = blocks_[sel_];
    switch (a) {
        case Align::Left:    b.x = 0; break;
        case Align::HCenter: b.x = (kPaperW - b.w) / 2; break;
        case Align::Right:   b.x = kPaperW - b.w; break;
        case Align::Top:     b.y = 0; break;
        case Align::VCenter: b.y = (kPaperH - b.h) / 2; break;
        case Align::Bottom:  b.y = kPaperH - b.h; break;
    }
    update();
    emit blockMoved(sel_);
}

bool LayoutCanvas::event(QEvent* e) {
    // macOS 触控板捏合缩放；标尺/纸张同一 paint 通道，天然跟随
    if (e->type() == QEvent::NativeGesture) {
        auto* g = static_cast<QNativeGestureEvent*>(e);
        if (g->gestureType() == Qt::ZoomNativeGesture) {
            zoomBy(int(g->value() * 100));
            return true;
        }
    }
    return QWidget::event(e);
}

// ============================================================================
// LayoutWindow
// ============================================================================

LayoutWindow::LayoutWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("jcolour — exhibition_2026.jcp"));
    resize(1360, 880);

    auto* central = new QWidget;
    auto* col = new QVBoxLayout(central);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    buildMenus();
    col->addWidget(buildToolbar());
    col->addWidget(buildPropBars());

    auto* mid = new QHBoxLayout;
    mid->setContentsMargins(0, 0, 0, 0);
    mid->setSpacing(0);
    canvas_ = new LayoutCanvas;          // before buildToolbox(): tool wiring connects to it
    mid->addWidget(buildToolbox());

    // canvas column (canvas + slim zoom bar)
    auto* canvasCol = new QVBoxLayout;
    canvasCol->setContentsMargins(0, 0, 0, 0);
    canvasCol->setSpacing(0);
    canvasCol->addWidget(canvas_, 1);
    {
        auto* zrow = new QWidget;
        zrow->setStyleSheet("background:#eef0f2;border-top:1px solid #d2d5d9;");
        auto* zl = new QHBoxLayout(zrow);
        zl->setContentsMargins(8, 2, 8, 2);
        zl->addStretch();
        auto* zm = iconBtn(ic::Icon::Minus, QStringLiteral("缩小"));
        auto* zv = new QLabel("100%");
        zv->setStyleSheet("color:#3b3f46;font-size:10px;min-width:34px;qproperty-alignment:AlignCenter;");
        auto* zp = iconBtn(ic::Icon::Plus, QStringLiteral("放大"));
        auto* zf = new QPushButton(QStringLiteral("适合"));
        zf->setToolTip(QStringLiteral("整张纸适合窗口"));
        zf->setFixedHeight(20);
        zf->setStyleSheet("font-size:10px;padding:0 8px;");
        zl->addWidget(zm); zl->addWidget(zv); zl->addWidget(zp); zl->addWidget(zf);
        zl->addStretch();
        connect(zm, &QToolButton::clicked, this, [this] { canvas_->zoomBy(-10); });
        connect(zp, &QToolButton::clicked, this, [this] { canvas_->zoomBy(+10); });
        connect(zf, &QPushButton::clicked, this, [this] { canvas_->zoomFit(); });
        connect(canvas_, &LayoutCanvas::zoomChanged, zv,
                [zv](int p) { zv->setText(QString::number(p) + "%"); });
        canvasCol->addWidget(zrow);
    }
    auto* canvasWrap = new QWidget;
    canvasWrap->setLayout(canvasCol);
    mid->addWidget(canvasWrap, 1);
    mid->addWidget(buildRightPanel());

    auto* midW = new QWidget;
    midW->setLayout(mid);
    col->addWidget(midW, 1);
    col->addWidget(buildJobStrip());

    setCentralWidget(central);
    buildStatusBar();

    connect(canvas_, &LayoutCanvas::selectionChanged, this, &LayoutWindow::onSelectionChanged);
    connect(canvas_, &LayoutCanvas::blockMoved, this, &LayoutWindow::syncPropBarFromBlock);

    onSelectionChanged(0);
}

// ---- menus -----------------------------------------------------------------

void LayoutWindow::buildMenus() {
    auto* mb = menuBar();
    QMenu* file = mb->addMenu(QStringLiteral("文件"));
    file->addAction(QStringLiteral("新建版面"), QKeySequence::New, [] {});
    file->addAction(QStringLiteral("打开…"), QKeySequence::Open, [] {});
    file->addAction(QStringLiteral("保存"), QKeySequence::Save, [] {});
    file->addSeparator();
    file->addAction(QStringLiteral("导入图片…"), QKeySequence(QStringLiteral("Ctrl+I")), [this] {
        QFileDialog::getOpenFileNames(this, QStringLiteral("导入图片"), {},
                                      QStringLiteral("图像 (*.tif *.tiff *.psd *.png *.jpg)"));
    });
    file->addSeparator();
    file->addAction(QStringLiteral("打印整版…"), QKeySequence::Print,
                    [this] { submitLayoutToQueue(); });

    QMenu* edit = mb->addMenu(QStringLiteral("编辑"));
    edit->addAction(QStringLiteral("撤销"), QKeySequence::Undo, [] {});
    edit->addAction(QStringLiteral("重做"), QKeySequence::Redo, [] {});

    QMenu* view = mb->addMenu(QStringLiteral("视图"));
    view->addAction(QStringLiteral("放大"), QKeySequence::ZoomIn, [this] { canvas_->zoomBy(+10); });
    view->addAction(QStringLiteral("缩小"), QKeySequence::ZoomOut, [this] { canvas_->zoomBy(-10); });
    view->addAction(QStringLiteral("适合窗口"), QKeySequence(QStringLiteral("Ctrl+0")),
                    [this] { canvas_->zoomFit(); });
    view->addSeparator();
    view->addAction(QStringLiteral("软打样"))->setCheckable(true);
    view->addAction(QStringLiteral("色域警告"))->setCheckable(true);

    mb->addMenu(QStringLiteral("对象"));
    QMenu* arrange = mb->addMenu(QStringLiteral("排列"));
    arrange->addAction(QStringLiteral("自动拼版 / 嵌套"), QKeySequence(QStringLiteral("Ctrl+Shift+N")),
                       [this] { runAutoNest(); });
    QMenu* colour = mb->addMenu(QStringLiteral("色彩"));
    colour->addAction(QStringLiteral("打印环境 / 校准…"), [this] { openEnvManager(); });
    QMenu* tools = mb->addMenu(QStringLiteral("工具"));
    tools->addAction(QStringLiteral("米尺走纸校正…"), [this] { openEnvManager(); });
    tools->addAction(QStringLiteral("检查线性…"), [this] { openEnvManager(); });
    mb->addMenu(QStringLiteral("帮助"));
}

// ---- toolbar ----------------------------------------------------------------

QWidget* LayoutWindow::buildToolbar() {
    auto* bar = new QWidget;
    bar->setFixedHeight(34);
    bar->setStyleSheet("background:#e7e8ea;border-bottom:1px solid #d2d5d9;");
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(6, 0, 6, 0);
    h->setSpacing(2);

    h->addWidget(iconBtn(ic::Icon::FilePlus,   QStringLiteral("新建")));
    h->addWidget(iconBtn(ic::Icon::FolderOpen, QStringLiteral("打开")));
    h->addWidget(iconBtn(ic::Icon::Save,       QStringLiteral("保存")));
    h->addWidget(vsep());
    h->addWidget(iconBtn(ic::Icon::Undo, QStringLiteral("撤销")));
    h->addWidget(iconBtn(ic::Icon::Redo, QStringLiteral("重做")));
    h->addWidget(vsep());
    h->addWidget(iconBtn(ic::Icon::ImagePlus, QStringLiteral("导入图片")));
    h->addWidget(vsep());
    auto* eye = iconBtn(ic::Icon::Eye, QStringLiteral("软打样"));
    eye->setCheckable(true); eye->setChecked(true);
    h->addWidget(eye);
    auto* gam = iconBtn(ic::Icon::AlertTriangle, QStringLiteral("色域警告"));
    gam->setCheckable(true);
    h->addWidget(gam);
    h->addWidget(vsep());
    {
        auto* nest = iconBtn(ic::Icon::Grid, QStringLiteral("自动拼版"));
        connect(nest, &QToolButton::clicked, this, [this] { runAutoNest(); });
        h->addWidget(nest);
    }
    h->addWidget(vsep());

    // ① printer → ② paper → ③ size — the env selectors (user-specified order)
    auto addLbl = [&](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet("color:#6b7079;font-size:10px;margin:0 2px;");
        h->addWidget(l);
    };
    addLbl(QStringLiteral("打印机"));
    printerCombo_ = new QComboBox;
    printerCombo_->setMinimumWidth(140);
    paperCombo_ = new QComboBox;
    paperCombo_->setMinimumWidth(170);
    sizeCombo_ = new QComboBox;
    sizeCombo_->addItems({QStringLiteral("24″ × 36″"), QStringLiteral("17″ × 22″"),
                          QStringLiteral("13″ × 19″"), QStringLiteral("24″ 卷宽"),
                          QStringLiteral("44″ 卷宽")});
    // real data when the ParamDb has configs; demo fallback otherwise
    {
        QStringList printers, papers;
        const auto cfgs = PaperState::instance().listConfigs();
        for (const auto& c : cfgs) {
            if (!printers.contains(c.printer)) printers << c.printer;
            if (!papers.contains(c.paperName)) papers << c.paperName;
        }
        if (printers.isEmpty()) printers = {QStringLiteral("Epson SC-P9500"),
                                            QStringLiteral("Epson SC-P7500")};
        if (papers.isEmpty())
            papers = {QStringLiteral("Hahnemühle Photo Rag 308"),
                      QStringLiteral("Canson Baryta 310"),
                      QStringLiteral("Epson Legacy Fibre")};
        printerCombo_->addItems(printers);
        paperCombo_->addItems(papers);
    }
    h->addWidget(printerCombo_);
    addLbl(QStringLiteral("纸张"));
    h->addWidget(paperCombo_);
    addLbl(QStringLiteral("尺寸"));
    h->addWidget(sizeCombo_);
    h->addWidget(vsep());
    auto* envBtn = new QPushButton(QStringLiteral(" 打印环境 / 校准…"));
    envBtn->setIcon(ic::icon(ic::Icon::Settings, QColor(0x1f, 0x5f, 0xa8), 14));
    envBtn->setIconSize(QSize(14, 14));
    envBtn->setStyleSheet("color:#1f5fa8;background:#eaf2fc;border:1px solid #bcd4f2;"
                          "border-radius:4px;padding:2px 8px;font-size:10px;");
    connect(envBtn, &QPushButton::clicked, this, &LayoutWindow::openEnvManager);
    h->addWidget(envBtn);

    h->addStretch();
    {
        auto* cfg = iconBtn(ic::Icon::Settings, QStringLiteral("设定（量测设备 / i1 接入方式）"));
        connect(cfg, &QToolButton::clicked, this, [this] {
            QDialog dlg(this);
            dlg.setWindowTitle(QStringLiteral("设定 — 量测设备"));
            dlg.resize(560, 520);
            auto* v = new QVBoxLayout(&dlg);
            v->setContentsMargins(0, 0, 0, 0);
            v->addWidget(new PageMeasurementDevices(&dlg));
            dlg.exec();
        });
        h->addWidget(cfg);
    }
    auto* print = new QPushButton(QStringLiteral(" 打印"));
    print->setIcon(ic::icon(ic::Icon::Printer, QColor(Qt::white), 15));
    print->setIconSize(QSize(15, 15));
    print->setToolTip(QStringLiteral("打印整版 → 送 RIP 队列"));
    print->setStyleSheet("background:#2e7dd7;color:#fff;border:none;border-radius:4px;"
                         "padding:4px 14px;font-weight:600;");
    connect(print, &QPushButton::clicked, this, [this] { submitLayoutToQueue(); });
    h->addWidget(print);
    return bar;
}

// ---- property bars (object / text) ------------------------------------------

QWidget* LayoutWindow::buildPropBars() {
    propStack_ = new QStackedWidget;
    propStack_->setFixedHeight(30);
    propStack_->setStyleSheet(
        "QStackedWidget{background:#e7e8ea;border-bottom:1px solid #d2d5d9;}"
        "QDoubleSpinBox,QComboBox{font-size:10px;}"
        "QLabel{color:#6b7079;font-size:10px;}");

    auto mmSpin = [](double v, double maxV) {
        auto* s = new QDoubleSpinBox;
        s->setRange(0, maxV);
        s->setDecimals(1);
        s->setValue(v);
        s->setSuffix(QStringLiteral(" 毫米"));
        s->setFixedWidth(86);
        return s;
    };

    // -- object row
    {
        auto* w = new QWidget;
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(6, 2, 6, 2);
        h->setSpacing(4);
        h->addWidget(new QLabel("X:"));
        h->addWidget(px_ = mmSpin(16, kPaperW));
        h->addWidget(new QLabel("Y:"));
        h->addWidget(py_ = mmSpin(17, kPaperH));
        h->addWidget(new QLabel(QStringLiteral("宽:")));
        h->addWidget(pw_ = mmSpin(286, kPaperW));
        h->addWidget(new QLabel(QStringLiteral("高:")));
        h->addWidget(ph_ = mmSpin(380, kPaperH));
        auto* link = glyphBtn("🔗", QStringLiteral("等比锁定"));
        link->setCheckable(true); link->setChecked(true);
        h->addWidget(link);
        h->addWidget(vsep());
        auto* rot = new QDoubleSpinBox;
        rot->setRange(-360, 360); rot->setDecimals(0); rot->setSuffix("°");
        rot->setFixedWidth(56);
        h->addWidget(rot);
        h->addWidget(glyphBtn("⟳", QStringLiteral("顺时针 90°")));
        h->addWidget(glyphBtn("⇋", QStringLiteral("水平镜像")));
        h->addWidget(glyphBtn("⇵", QStringLiteral("垂直镜像")));
        h->addWidget(vsep());
        for (auto g : {"⫷", "⊜", "⫸", "⊤", "⊟", "⊥"})
            h->addWidget(glyphBtn(QString::fromUtf8(g), QStringLiteral("对齐")));
        h->addWidget(vsep());
        h->addWidget(new QLabel(QStringLiteral("不透明")));
        auto* op = new QDoubleSpinBox;
        op->setRange(0, 100); op->setDecimals(0); op->setValue(100); op->setSuffix("%");
        op->setFixedWidth(58);
        h->addWidget(op);
        auto* aa = new QCheckBox(QStringLiteral("自动调整"));
        aa->setStyleSheet("font-size:10px;color:#6b7079;");
        h->addWidget(aa);
        h->addStretch();
        warnChip_ = new QPushButton(QStringLiteral("⚠ 2 警告"));
        warnChip_->setToolTip(QStringLiteral("点击查看预检清单"));
        warnChip_->setStyleSheet("background:#fbf2dd;border:1px solid #e6c878;color:#8a6310;"
                                 "border-radius:3px;font-size:9px;font-weight:600;padding:1px 6px;");
        connect(warnChip_, &QPushButton::clicked, this, [this] { rightTabs_->setCurrentIndex(2); });
        h->addWidget(warnChip_);
        propStack_->addWidget(w);
    }

    // -- text row
    {
        auto* w = new QWidget;
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(6, 2, 6, 2);
        h->setSpacing(4);
        auto* tag = new QLabel(QStringLiteral("字"));
        tag->setStyleSheet("font-weight:700;color:#4a4e55;");
        h->addWidget(tag);
        auto* fonts = new QFontComboBox;
        fonts->setFixedWidth(150);
        h->addWidget(fonts);
        auto* weight = new QComboBox;
        weight->addItems({"Light", "Regular", "Medium", "Bold"});
        h->addWidget(weight);
        h->addWidget(vsep());
        h->addWidget(new QLabel(QStringLiteral("字号")));
        auto* size = new QDoubleSpinBox;
        size->setRange(1, 720); size->setValue(36); size->setSuffix(QStringLiteral(" 点"));
        size->setFixedWidth(70);
        h->addWidget(size);
        h->addWidget(new QLabel(QStringLiteral("行距")));
        auto* lead = new QDoubleSpinBox;
        lead->setRange(1, 999); lead->setValue(43); lead->setSuffix(QStringLiteral(" 点"));
        lead->setFixedWidth(70);
        h->addWidget(lead);
        h->addWidget(new QLabel(QStringLiteral("字距")));
        auto* trk = new QDoubleSpinBox;
        trk->setRange(-50, 200); trk->setValue(0); trk->setSuffix("%");
        trk->setFixedWidth(60);
        h->addWidget(trk);
        h->addWidget(vsep());
        for (auto g : {"⟸", "≡", "⟹", "☰"})
            h->addWidget(glyphBtn(QString::fromUtf8(g), QStringLiteral("段落对齐")));
        auto* tate = new QCheckBox(QStringLiteral("直排内横排"));
        tate->setStyleSheet("font-size:10px;color:#6b7079;");
        h->addWidget(tate);
        h->addStretch();
        propStack_->addWidget(w);
    }

    auto syncBack = [this] {
        if (syncing_ || canvas_->selected() < 0) return;
        CanvasBlock& b = canvas_->blocks()[canvas_->selected()];
        b.x = px_->value(); b.y = py_->value();
        b.w = qMax(5.0, pw_->value()); b.h = qMax(5.0, ph_->value());
        canvas_->update();
    };
    for (auto* s : {px_, py_, pw_, ph_})
        connect(s, &QDoubleSpinBox::valueChanged, this, syncBack);

    return propStack_;
}

// ---- toolbox -----------------------------------------------------------------

QWidget* LayoutWindow::buildToolbox() {
    auto* box = new QWidget;
    box->setFixedWidth(36);
    box->setStyleSheet("background:#eef0f2;border-right:1px solid #d2d5d9;"
                       "QToolButton{font-size:13px;}");
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(4, 6, 4, 6);
    v->setSpacing(2);
    // 工具栏分组对标 Mirage / ImagePrint / Caldera 排版编辑器：
    //   工具（互斥）→ 块操作 → 拼版 → 显示开关 → 视图缩放
    const auto sep = [&v] {
        auto* s = new QFrame;
        s->setFixedSize(18, 1);
        s->setStyleSheet("background:#d2d5d9;border:none;");
        v->addSpacing(3);
        v->addWidget(s, 0, Qt::AlignHCenter);
        v->addSpacing(3);
    };
    const auto add = [&](const char* glyph, const QString& tip, bool exclusiveTool,
                         std::function<void()> fn) {
        auto* b = glyphBtn(QString::fromUtf8(glyph), tip);
        b->setFixedSize(26, 26);
        if (exclusiveTool) { b->setCheckable(true); b->setAutoExclusive(true); }
        if (fn) connect(b, &QToolButton::clicked, this, std::move(fn));
        v->addWidget(b);
        return b;
    };

    // ── 工具（互斥选中）──
    selectToolBtn_ = add("➤", QStringLiteral("选择 / 移动"), true,
        [this] { canvas_->setTool(CanvasTool::Select); });
    selectToolBtn_->setChecked(true);
    add("A", QStringLiteral("文字"), true, [this] {
        canvas_->setTool(CanvasTool::Text);
        statusBar()->showMessage(QStringLiteral("文字工具：在纸张上点击放置文字"), 4000); });
    add("▦", QStringLiteral("二维码 / 条码"), true, [this] {
        canvas_->setTool(CanvasTool::Qr);
        statusBar()->showMessage(QStringLiteral("二维码工具：在纸张上点击放置二维码"), 4000); });
    add("✥", QStringLiteral("抓手平移（缩放后拖动画布）"), true, [this] {
        canvas_->setTool(CanvasTool::Hand); });
    add("⊿", QStringLiteral("测量（拖一条线读距离，Esc 清除）"), true, [this] {
        canvas_->setTool(CanvasTool::Measure);
        statusBar()->showMessage(QStringLiteral("测量工具：在纸上拖动读取 mm 距离与 Δx/Δy"), 4000); });

    // ── 块操作 ──
    sep();
    add("⧉", QStringLiteral("复制选中块 (Cmd+D)"), false, [this] { canvas_->duplicateSelected(); });
    add("份", QStringLiteral("份数… 同图打 N 份（配合自动拼版）"), false, [this] {
        bool ok = false;
        const int n = QInputDialog::getInt(this, QStringLiteral("份数"),
            QStringLiteral("再复制多少份？（之后可点「自动拼版」归位）"), 3, 1, 99, 1, &ok);
        if (ok) canvas_->duplicateSelectedN(n); });
    add("⟳", QStringLiteral("旋转 90°（省纸拼版常用）"), false, [this] { canvas_->rotateSelected90(); });
    add("⌫", QStringLiteral("删除选中块"), false, [this] { canvas_->deleteSelected(); });
    {
        auto* b = add("≡", QStringLiteral("对齐（相对纸张）"), false, nullptr);
        auto* menu = new QMenu(b);
        using A = LayoutCanvas::Align;
        const std::pair<QString, A> items[] = {
            {QStringLiteral("左对齐"), A::Left},   {QStringLiteral("水平居中"), A::HCenter},
            {QStringLiteral("右对齐"), A::Right},  {QStringLiteral("顶对齐"), A::Top},
            {QStringLiteral("垂直居中"), A::VCenter}, {QStringLiteral("底对齐"), A::Bottom},
        };
        for (const auto& [txt, mode] : items)
            menu->addAction(txt, this, [this, mode] { canvas_->alignSelected(mode); });
        b->setMenu(menu);
        b->setPopupMode(QToolButton::InstantPopup);
    }

    // ── 拼版 ──
    sep();
    add("▥", QStringLiteral("自动拼版 / 嵌套"), false, [this] { runAutoNest(); });
    add("⊞", QStringLiteral("步进重复 Step && Repeat…"), false, [this] {
        auto* dlg = new StepRepeatDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show(); });
    add("▤", QStringLiteral("联系表 Contact Sheet…"), false, [this] {
        auto* w = new ContactSheetDialog(nullptr);
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->setWindowTitle(QStringLiteral("联系表"));
        w->show(); });

    // ── 显示开关 ──
    sep();
    add("✂", QStringLiteral("裁切角线 开/关（裁纸标记）"), false, [this] { canvas_->toggleCropMarks(); });
    add("#", QStringLiteral("50mm 网格 开/关"), false, [this] { canvas_->toggleGrid(); });
    add("⟙", QStringLiteral("标尺 开/关"), false, [this] { canvas_->toggleRulers(); });

    // ── 视图缩放 ──
    sep();
    add("⊕", QStringLiteral("放大 (Cmd+滚轮↑ / Cmd+=)"), false, [this] { canvas_->zoomBy(+10); });
    add("⊖", QStringLiteral("缩小 (Cmd+滚轮↓ / Cmd+-)"), false, [this] { canvas_->zoomBy(-10); });
    add("⤢", QStringLiteral("缩放适配窗口"), false, [this] { canvas_->zoomFit(); });
    v->addStretch();
    // insert tool done (or cancelled) → revert to 选择
    connect(canvas_, &LayoutCanvas::toolFinished, this, [this] {
        if (selectToolBtn_) selectToolBtn_->setChecked(true);
    });
    return box;
}

// ---- right panel ---------------------------------------------------------------

QWidget* LayoutWindow::buildRightPanel() {
    rightTabs_ = new QTabWidget;
    rightTabs_->setFixedWidth(252);
    rightTabs_->setDocumentMode(true);
    rightTabs_->setStyleSheet(
        "QTabWidget::pane{border:none;border-left:1px solid #d2d5d9;background:#f5f6f7;}"
        "QTabBar::tab{font-size:10px;padding:5px 10px;}"
        "QLabel{font-size:10px;}");

    auto hdLbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet("font-size:9px;font-weight:600;color:#868c95;background:#eef0f2;"
                         "padding:4px 8px;border-bottom:1px solid #e6e8eb;");
        return l;
    };
    auto kvRow = [](const QString& k, QLabel*& vOut, bool warn = false) {
        auto* w = new QWidget;
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(10, 2, 10, 2);
        auto* kl = new QLabel(k);
        kl->setStyleSheet("color:#6b7079;");
        vOut = new QLabel;
        vOut->setStyleSheet(warn ? "color:#c2861a;font-weight:600;" : "color:#2b2e33;font-weight:600;");
        h->addWidget(kl);
        h->addStretch();
        h->addWidget(vOut);
        return w;
    };

    // ---- 图片 tab
    {
        auto* page = new QWidget;
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);
        imgHd_ = hdLbl("portrait_01.tif");
        v->addWidget(imgHd_);
        v->addWidget(kvRow(QStringLiteral("原始尺寸"), imgSize_));
        v->addWidget(kvRow(QStringLiteral("嵌入 ICC"), imgIcc_, true));
        v->addWidget(kvRow(QStringLiteral("色彩深度"), imgDepth_));
        v->addWidget(kvRow(QStringLiteral("分辨率"), imgDpi_));
        v->addWidget(hdLbl(QStringLiteral("打印模式（5 选 1 · 用已校准的线性/色彩管理）")));

        auto* grid = new QGridLayout;
        grid->setContentsMargins(10, 4, 10, 2);
        grid->setSpacing(4);
        int i = 0;
        for (const auto& m : kModes) {
            auto* b = new QPushButton(QString::fromUtf8(m.name));
            b->setCheckable(true);
            b->setFixedHeight(22);
            const bool stale = modeState(m.key) != cal::CalibState::FRESH;
            b->setStyleSheet(QStringLiteral(
                "QPushButton{font-size:10px;border:1px solid %1;border-radius:4px;background:#fff;}"
                "QPushButton:checked{background:#eaf2fc;border-color:#2e7dd7;color:#1f5fa8;font-weight:600;}")
                .arg(stale ? "#c0392b" : "#d2d5d9"));
            if (stale)
                b->setToolTip(QStringLiteral("线性曲线 %1 天 · 超 1 个月需检查").arg(m.days));
            const QString key = m.key;
            connect(b, &QPushButton::clicked, this, [this, key] { onModeChosen(key); });
            modeBtns_ << b;
            grid->addWidget(b, i / 2, i % 2);
            ++i;
        }
        // span the last (16位RGB) across both columns
        grid->addWidget(modeBtns_.last(), 2, 0, 1, 2);
        v->addLayout(grid);

        modeHint_ = new QLabel;
        modeHint_->setWordWrap(true);
        modeHint_->setStyleSheet("color:#1f5fa8;background:#eaf2fc;border:1px solid #cfe0f4;"
                                 "border-radius:4px;padding:5px 7px;font-size:9px;margin:2px 10px;");
        v->addWidget(modeHint_);

        // per-mode adjust stack
        adjustStack_ = new QStackedWidget;
        auto slider = [](const QString& name, int val) {
            auto* w = new QWidget;
            auto* h = new QHBoxLayout(w);
            h->setContentsMargins(10, 1, 10, 1);
            auto* l = new QLabel(name);
            l->setFixedWidth(52);
            l->setStyleSheet("color:#6b7079;");
            auto* s = new QSlider(Qt::Horizontal);
            s->setRange(0, 100);
            s->setValue(val);
            h->addWidget(l);
            h->addWidget(s);
            return w;
        };
        auto seg = [](const QString& name, const QStringList& opts, int on) {
            auto* w = new QWidget;
            auto* h = new QHBoxLayout(w);
            h->setContentsMargins(10, 1, 10, 1);
            auto* l = new QLabel(name);
            l->setFixedWidth(52);
            l->setStyleSheet("color:#6b7079;");
            h->addWidget(l);
            h->addStretch();
            for (int i = 0; i < opts.size(); ++i) {
                auto* b = new QPushButton(opts[i]);
                b->setCheckable(true);
                b->setAutoExclusive(true);
                b->setChecked(i == on);
                b->setFixedHeight(18);
                b->setStyleSheet("QPushButton{font-size:9px;padding:0 7px;border:1px solid #c4c9d0;background:#fff;}"
                                 "QPushButton:checked{background:#eaf2fc;color:#1f5fa8;font-weight:600;}");
                h->addWidget(b);
            }
            return w;
        };
        auto hint = [](const QString& t) {
            auto* l = new QLabel(t);
            l->setWordWrap(true);
            l->setStyleSheet("color:#1f5fa8;background:#eaf2fc;border:1px solid #cfe0f4;"
                             "border-radius:4px;padding:4px 7px;font-size:9px;margin:2px 10px;");
            return l;
        };
        auto pageOf = [&](std::initializer_list<QWidget*> ws) {
            auto* p = new QWidget;
            auto* pv = new QVBoxLayout(p);
            pv->setContentsMargins(0, 2, 0, 2);
            pv->setSpacing(2);
            for (auto* x : ws) pv->addWidget(x);
            pv->addStretch();
            return p;
        };
        // mono / gray / tone / rgb / rgb16 — order matches kModes
        adjustStack_->addWidget(pageOf({hdLbl(QStringLiteral("单黑 · 黑白调整（纯黑单墨）")),
            slider(QStringLiteral("亮度"), 50), slider(QStringLiteral("对比"), 55),
            hint(QStringLiteral("单黑只用 K 一条墨：线性好即可打印，无色彩管理。"))}));
        adjustStack_->addWidget(pageOf({hdLbl(QStringLiteral("黑+灰 · 黑白调整（黑 + 浅灰墨）")),
            slider(QStringLiteral("亮度"), 50), slider(QStringLiteral("对比"), 55),
            seg(QStringLiteral("高光"), {QStringLiteral("中性"), QStringLiteral("偏暖"), QStringLiteral("偏冷")}, 0),
            seg(QStringLiteral("阴影"), {QStringLiteral("中性"), QStringLiteral("偏暖"), QStringLiteral("偏冷")}, 0),
            hint(QStringLiteral("黑 + 浅灰多墨过渡细腻；线性做好即可，分区微调保中性。"))}));
        adjustStack_->addWidget(pageOf({hdLbl(QStringLiteral("黑+灰+彩 · 分区色调（split-tone）")),
            seg(QStringLiteral("高光"), {QStringLiteral("冷"), QStringLiteral("中"), QStringLiteral("暖")}, 1),
            seg(QStringLiteral("中间调"), {QStringLiteral("冷"), QStringLiteral("中"), QStringLiteral("暖")}, 1),
            seg(QStringLiteral("阴影"), {QStringLiteral("冷"), QStringLiteral("中"), QStringLiteral("暖")}, 0),
            slider(QStringLiteral("色调强度"), 35),
            hint(QStringLiteral("黑白基底 + 彩墨分区调色；用 G7 修正线性、保中性轴。"))}));
        auto rgbPage = [&](bool sixteen) {
            auto* iccRow = new QWidget;
            auto* ih = new QHBoxLayout(iccRow);
            ih->setContentsMargins(10, 1, 10, 1);
            auto* il = new QLabel(QStringLiteral("输出 ICC"));
            il->setFixedWidth(52);
            il->setStyleSheet("color:#6b7079;");
            auto* ic = new QComboBox;
            ic->addItem(QStringLiteral("PhotoRag_P9500_RGB.icc"));
            ih->addWidget(il);
            ih->addWidget(ic, 1);
            auto* intRow = new QWidget;
            auto* nh = new QHBoxLayout(intRow);
            nh->setContentsMargins(10, 1, 10, 1);
            auto* nl = new QLabel(QStringLiteral("渲染意图"));
            nl->setFixedWidth(52);
            nl->setStyleSheet("color:#6b7079;");
            auto* nc = new QComboBox;
            nc->addItems({QStringLiteral("可感知 Perceptual"), QStringLiteral("相对色度 Rel.Col"),
                          QStringLiteral("饱和度"), QStringLiteral("绝对色度")});
            nh->addWidget(nl);
            nh->addWidget(nc, 1);
            auto* bpc = new QCheckBox(QStringLiteral("黑点补偿 BPC"));
            bpc->setChecked(true);
            bpc->setStyleSheet("font-size:10px;margin-left:10px;");
            auto* btns = new QWidget;
            auto* bh = new QHBoxLayout(btns);
            bh->setContentsMargins(10, 1, 10, 1);
            auto* ab = new QPushButton(QStringLiteral("A/B 软打样"));
            auto* pwt = new QPushButton(QStringLiteral("纸白模拟"));
            for (auto* b : {ab, pwt}) {
                b->setFixedHeight(20);
                b->setStyleSheet("font-size:9px;");
                bh->addWidget(b);
            }
            bh->addStretch();
            // Or/Gr/V meters
            auto meter = [](const QString& n, int pct) {
                auto* w = new QWidget;
                auto* h = new QHBoxLayout(w);
                h->setContentsMargins(10, 0, 10, 0);
                auto* l = new QLabel(n);
                l->setFixedWidth(18);
                l->setStyleSheet("color:#6b7079;");
                auto* p = new QProgressBar;
                p->setRange(0, 100);
                p->setValue(pct);
                p->setTextVisible(false);
                p->setFixedHeight(5);
                auto* vv = new QLabel(QString::number(pct) + "%");
                vv->setFixedWidth(28);
                h->addWidget(l);
                h->addWidget(p, 1);
                h->addWidget(vv);
                return w;
            };
            std::initializer_list<QWidget*> base = {
                hdLbl(sixteen ? QStringLiteral("16 位 RGB · 全面色彩管理（高精度）")
                              : QStringLiteral("RGB · 全面色彩管理")),
                iccRow, intRow, bpc, hint(QStringLiteral("GamutAdvisor：此图饱和度高 → 建议可感知。")), btns,
                hdLbl(QStringLiteral("Or / Gr / V 扩展色调用率")),
                meter("Or", 42), meter("Gr", 18), meter("V", 9)};
            auto* p = pageOf(base);
            if (sixteen) {
                auto* pv = qobject_cast<QVBoxLayout*>(p->layout());
                pv->insertWidget(pv->count() - 1,
                                 seg(QStringLiteral("光源"), {"D50", "D65", "A", "F11"}, 0));
            }
            return p;
        };
        adjustStack_->addWidget(rgbPage(false));
        adjustStack_->addWidget(rgbPage(true));

        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(adjustStack_);
        v->addWidget(scroll, 1);
        rightTabs_->addTab(page, QStringLiteral("图片"));
    }

    // ---- 环境 tab — list = PaperState configs (demo fallback); click → manager
    {
        auto* page = new QWidget;
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);
        v->addWidget(hdLbl(QStringLiteral("打印环境 · 机 + 纸 · 点击打开管理器")));
        struct Row { QString name, sub, badge, colour; };
        QVector<Row> rows;
        const auto cfgs = PaperState::instance().listConfigs();
        for (const auto& c : cfgs)
            rows << Row{c.printer + QStringLiteral(" · ") + c.paperName,
                        QStringLiteral("墨批 %1 · v%2").arg(c.inkBatch).arg(c.version),
                        c.enabled ? QStringLiteral("已启用") : QStringLiteral("停用"),
                        c.enabled ? "#1d6b39" : "#9aa0a8"};
        if (rows.isEmpty())
            rows = {{QStringLiteral("P9500 · Photo Rag 308"),
                     QStringLiteral("总墨 260% · 4 模式 9 天 ✓"),
                     QStringLiteral("RGB 需检查"), "#8a6310"},
                    {QStringLiteral("P9500 · Canson Baryta 310"),
                     QStringLiteral("总墨 280% · 全部模式"),
                     QStringLiteral("过期 18天"), "#8a6310"},
                    {QStringLiteral("P9500 · Legacy Fibre"),
                     QStringLiteral("墨批 #A6 旧批"),
                     QStringLiteral("墨批已变"), "#9a2418"}};
        for (const auto& r : rows) {
            auto* b = new QPushButton;
            b->setFixedHeight(44);
            b->setStyleSheet("QPushButton{background:#fff;border:none;border-bottom:1px solid #e6e8eb;text-align:left;}"
                             "QPushButton:hover{background:#eef0f2;}");
            auto* bl = new QHBoxLayout(b);
            bl->setContentsMargins(10, 4, 10, 4);
            auto* tcol = new QVBoxLayout;
            tcol->setSpacing(1);
            auto* nm = new QLabel(r.name);
            nm->setStyleSheet("font-weight:600;color:#2b2e33;");
            auto* sb = new QLabel(r.sub);
            sb->setStyleSheet("color:#868c95;font-size:8px;");
            tcol->addWidget(nm);
            tcol->addWidget(sb);
            bl->addLayout(tcol);
            bl->addStretch();
            auto* bd = new QLabel(r.badge);
            bd->setStyleSheet(QStringLiteral("color:%1;font-size:8px;font-weight:600;").arg(r.colour));
            bl->addWidget(bd);
            connect(b, &QPushButton::clicked, this, &LayoutWindow::openEnvManager);
            v->addWidget(b);
        }
        auto* manage = new QPushButton(QStringLiteral("⛭ 打开打印环境管理器…"));
        manage->setStyleSheet("margin:8px 10px;font-size:10px;");
        connect(manage, &QPushButton::clicked, this, &LayoutWindow::openEnvManager);
        v->addWidget(manage);
        v->addStretch();
        rightTabs_->addTab(page, QStringLiteral("环境"));
    }

    // ---- 预检 tab
    {
        auto* page = new QWidget;
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);
        v->addWidget(hdLbl(QStringLiteral("预检 — 选中图")));
        pfBanner_ = new QLabel;
        pfBanner_->setWordWrap(true);
        pfBanner_->setStyleSheet("background:#fbf2dd;border:1px solid #e6c878;border-radius:4px;"
                                 "color:#8a6310;font-size:9px;padding:4px 7px;margin:4px 10px;");
        v->addWidget(pfBanner_);
        auto pfRow = [&](const QString& t, bool ok) {
            auto* l = new QLabel((ok ? QStringLiteral("✓ ") : QStringLiteral("⚠ ")) + t);
            l->setStyleSheet(QStringLiteral("color:%1;font-size:10px;margin:0 12px;")
                                 .arg(ok ? "#2f8c4f" : "#c2861a"));
            v->addWidget(l);
            return l;
        };
        pfRow(QStringLiteral("分辨率 350dpi"), true);
        pfRow(QStringLiteral("ICC: AdobeRGB ≠ 预设 → 转换"), false);
        pfRow(QStringLiteral("16-bit"), true);
        pfRow(QStringLiteral("色域超出 3.2%"), true);
        pfRow(QStringLiteral("总墨量 245% ≤ 260%"), true);
        {
            auto* w = new QWidget;
            auto* h = new QHBoxLayout(w);
            h->setContentsMargins(12, 0, 10, 0);
            pfCal_ = new QLabel;
            pfCal_->setStyleSheet("font-size:10px;");
            pfCalFix_ = new QPushButton(QStringLiteral("检查"));
            pfCalFix_->setFixedHeight(18);
            pfCalFix_->setStyleSheet("font-size:9px;padding:0 8px;");
            connect(pfCalFix_, &QPushButton::clicked, this, &LayoutWindow::openEnvManager);
            h->addWidget(pfCal_);
            h->addStretch();
            h->addWidget(pfCalFix_);
            v->addWidget(w);
        }
        pfRow(QStringLiteral("打印机已连线"), true);
        auto* btns = new QWidget;
        auto* bh = new QHBoxLayout(btns);
        bh->setContentsMargins(10, 6, 10, 6);
        auto* ignore = new QPushButton(QStringLiteral("忽略并打印"));
        auto* fix = new QPushButton(QStringLiteral("✦ 自动修正"));
        fix->setStyleSheet("background:#2e7dd7;color:#fff;border:none;border-radius:4px;padding:3px 10px;");
        bh->addWidget(ignore);
        bh->addWidget(fix);
        v->addWidget(btns);
        v->addStretch();
        rightTabs_->addTab(page, QStringLiteral("预检"));
    }

    // ---- 队列 tab (RIP queue + 出墨控制)
    {
        auto* page = new QWidget;
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);
        v->addWidget(hdLbl(QStringLiteral("RIP 队列 · P9500 · PJL 直送")));
        auto* tlRow = new QWidget;
        auto* th = new QHBoxLayout(tlRow);
        th->setContentsMargins(10, 4, 10, 4);
        th->addWidget(buildTrafficLight());
        th->addStretch();
        v->addWidget(tlRow);
        // The real queue module (PK/MK batching, retry, reorder) embedded here.
        queueWidget_ = new PrintQueueWidget;
        v->addWidget(queueWidget_, 1);
        rightTabs_->addTab(page, QStringLiteral("队列"));
    }

    return rightTabs_;
}

QWidget* LayoutWindow::buildTrafficLight() {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(4, 2, 6, 2);
    h->setSpacing(4);
    w->setStyleSheet("background:#fff;border:1px solid #d2d5d9;border-radius:5px;");
    struct D { const char* c; const char* tip; };
    const D dots[] = {{"#ff5f57", "离线"}, {"#febc2e", "手动开始"}, {"#28c840", "连线（自动）"}};
    auto* lbl = new QLabel(QStringLiteral("手动开始"));
    lbl->setObjectName("tlLbl");
    lbl->setStyleSheet("font-size:8px;color:#6b7079;border:none;");
    int i = 0;
    for (const auto& d : dots) {
        auto* b = new QToolButton;
        b->setFixedSize(13, 13);
        b->setToolTip(QString::fromUtf8(d.tip));
        b->setCheckable(true);
        b->setAutoExclusive(true);
        b->setChecked(i == 1);
        b->setStyleSheet(QStringLiteral(
            "QToolButton{border-radius:6px;background:%1;border:none;}"
            "QToolButton:!checked{background:%1;}"
            "QToolButton:checked{border:2px solid #2b2e33;}").arg(d.c));
        const QString tip = QString::fromUtf8(d.tip);
        connect(b, &QToolButton::clicked, this, [lbl, tip] { lbl->setText(tip); });
        h->addWidget(b);
        ++i;
    }
    h->addWidget(lbl);
    return w;
}

// ---- job strip -----------------------------------------------------------------

QWidget* LayoutWindow::buildJobStrip() {
    auto* bar = new QWidget;
    bar->setFixedHeight(62);
    bar->setStyleSheet("background:#eef0f2;border-top:1px solid #d2d5d9;");
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(8, 5, 8, 5);
    h->setSpacing(6);
    const struct { const char* name; const char* colour; } thumbs[] = {
        {"portrait_01", "#3a5468"}, {"landscape_02", "#3a5040"}, {"detail_03", "#4a3830"},
        {"abstract_04", "#3c3050"}, {"seascape_05", "#2a4840"}};
    for (int i = 0; i < 5; ++i) {
        auto* b = new QToolButton;
        b->setFixedSize(42, 50);
        b->setCheckable(true);
        b->setAutoExclusive(true);
        b->setChecked(i == 0);
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setText(QString::fromUtf8(thumbs[i].name).left(10));
        b->setStyleSheet(QStringLiteral(
            "QToolButton{background:%1;color:#fff;font-size:7px;border-radius:4px;border:2px solid transparent;}"
            "QToolButton:checked{border:2px solid #2e7dd7;}").arg(thumbs[i].colour));
        connect(b, &QToolButton::clicked, this, [this, i] { canvas_->select(i); });
        connect(canvas_, &LayoutCanvas::selectionChanged, b,
                [b, i](int s) { b->setChecked(s == i); });
        h->addWidget(b);
    }
    auto* add = new QToolButton;
    add->setFixedSize(42, 50);
    add->setText(QStringLiteral("＋\n导入"));
    add->setStyleSheet("QToolButton{border:1px dashed #b9c2cc;border-radius:4px;color:#2e7dd7;font-size:8px;}");
    h->addWidget(add);
    h->addStretch();
    auto* stat = new QLabel(QStringLiteral("5 张 · Photo Rag　|　利用率 <b style='color:#2f8c4f'>81%</b>"));
    stat->setStyleSheet("font-size:9px;color:#6b7079;");
    h->addWidget(stat);
    h->addWidget(buildTrafficLight());
    auto* queueN = new QLabel(QStringLiteral("队列 4"));
    queueN->setStyleSheet("font-size:9px;color:#6b7079;");
    h->addWidget(queueN);
    auto* print = new QPushButton(QStringLiteral("🖶 打印整版"));
    print->setToolTip(QStringLiteral("打印整版（5 张 · 当前纸张）"));
    print->setStyleSheet("background:#2e7dd7;color:#fff;border:none;border-radius:4px;"
                         "padding:6px 16px;font-weight:600;");
    connect(print, &QPushButton::clicked, this, [this] { submitLayoutToQueue(); });
    h->addWidget(print);
    return bar;
}

// ---- status bar -----------------------------------------------------------------

void LayoutWindow::buildStatusBar() {
    auto* sb = statusBar();
    sb->setStyleSheet("QStatusBar{background:#eef0f2;border-top:1px solid #d2d5d9;font-size:9px;color:#6b7079;}"
                      "QLabel{font-size:9px;color:#6b7079;}");
    auto mk = [&](const QString& t, const QString& dot) {
        auto* l = new QLabel(QStringLiteral("<span style='color:%1'>●</span> %2").arg(dot, t));
        sb->addWidget(l);
        return l;
    };
    mk(QStringLiteral("P9500 · 已连线"), "#2f8c4f");
    mk(QStringLiteral("软打样 · 启用"), "#2f8c4f");
    // calibration summary — engine-driven worst-of-modes
    QString worst;
    for (const auto& m : kModes)
        if (modeState(m.key) != cal::CalibState::FRESH)
            worst = QStringLiteral("%1 %2 天需检查").arg(QString::fromUtf8(m.name)).arg(m.days);
    sbCal_ = mk(worst.isEmpty() ? QStringLiteral("校准 9 天 ✓")
                                : QStringLiteral("校准 9 天 · ") + worst, "#d97706");
    mk(QStringLiteral("LLK 31%"), "#d97706");
    sbSel_ = new QLabel;
    sb->addWidget(sbSel_, 1);
    auto* right = new QLabel(QStringLiteral("24″ × 36″ · 5 张"));
    sb->addPermanentWidget(right);
}

// ---- behaviour --------------------------------------------------------------------

void LayoutWindow::onSelectionChanged(int idx) {
    if (idx < 0) return;
    const CanvasBlock& b = canvas_->blocks()[idx];
    propStack_->setCurrentIndex(b.isText ? 1 : 0);
    if (!b.isText) {
        syncPropBarFromBlock(idx);
        const ImgInfo info = imgInfo(b.name);
        imgHd_->setText(b.name + ".tif");
        imgSize_->setText(QString::fromUtf8(info.size));
        imgIcc_->setText(QString::fromUtf8(info.icc));
        imgIcc_->setStyleSheet(info.warn ? "color:#c2861a;font-weight:600;" : "color:#2b2e33;font-weight:600;");
        imgDepth_->setText(QString::fromUtf8(info.depth));
        imgDpi_->setText(QString::fromUtf8(info.dpi));
        // reflect this image's mode in the 5-button group
        for (int i = 0; i < modeBtns_.size(); ++i)
            modeBtns_[i]->setChecked(QString(kModes[i].key) == b.modeKey);
        onModeChosen(b.modeKey);
    } else {
        sbSel_->setText(QStringLiteral("文字物件 · “%1”").arg(b.text));
    }
}

void LayoutWindow::syncPropBarFromBlock(int idx) {
    if (idx < 0 || canvas_->blocks()[idx].isText) return;
    const CanvasBlock& b = canvas_->blocks()[idx];
    syncing_ = true;
    px_->setValue(b.x);
    py_->setValue(b.y);
    pw_->setValue(b.w);
    ph_->setValue(b.h);
    syncing_ = false;
    sbSel_->setText(QStringLiteral("%1 · X %2  Y %3  宽 %4  高 %5 毫米")
                        .arg(b.name).arg(b.x, 0, 'f', 0).arg(b.y, 0, 'f', 0)
                        .arg(b.w, 0, 'f', 0).arg(b.h, 0, 'f', 0));
}

void LayoutWindow::onModeChosen(const QString& key) {
    const ModeDef* m = modeDef(key);
    if (canvas_->selected() >= 0 && !canvas_->blocks()[canvas_->selected()].isText)
        canvas_->blocks()[canvas_->selected()].modeKey = key;
    for (int i = 0; i < modeBtns_.size(); ++i)
        modeBtns_[i]->setChecked(QString(kModes[i].key) == key);
    adjustStack_->setCurrentIndex(int(m - kModes));
    const bool stale = modeState(key) != cal::CalibState::FRESH;
    QString hint = QStringLiteral("ⓘ 当前 <b>%1</b> · 线性曲线 <b>%2</b>：用 <b>%3</b>。")
                       .arg(QString::fromUtf8(m->name), QString::fromUtf8(m->lin),
                            QString::fromUtf8(m->depth));
    if (stale)
        hint += QStringLiteral("<br><span style='color:#9a2418'>⚠ <b>线性已 %1 天</b>（超 1 个月）— 先检查，不行就重做线性。</span>")
                    .arg(m->days);
    modeHint_->setText(hint);
    updatePreflight();
    sbSel_->setText(QStringLiteral("本图 · %1（%2 · %3）· P9500 · Photo Rag 308")
                        .arg(QString::fromUtf8(m->name), QString::fromUtf8(m->depth),
                             QString::fromUtf8(m->lin)));
}

void LayoutWindow::updatePreflight() {
    QString key = "tone";
    if (canvas_->selected() >= 0 && !canvas_->blocks()[canvas_->selected()].isText)
        key = canvas_->blocks()[canvas_->selected()].modeKey;
    const ModeDef* m = modeDef(key);
    const bool stale = modeState(key) != cal::CalibState::FRESH;
    const int n = 1 + (stale ? 1 : 0);
    pfBanner_->setText(QStringLiteral("⚠ %1 项警告，建议处理后打印").arg(n));
    if (stale) {
        pfCal_->setText(QStringLiteral("⚠ %1 线性 %2 天 · 超 1 个月需检查")
                            .arg(QString::fromUtf8(m->name)).arg(m->days));
        pfCal_->setStyleSheet("color:#c2861a;font-size:10px;");
    } else {
        pfCal_->setText(QStringLiteral("✓ 校准有效 · %1 天").arg(m->days));
        pfCal_->setStyleSheet("color:#2f8c4f;font-size:10px;");
    }
    pfCalFix_->setVisible(stale);
    if (warnChip_) warnChip_->setText(QStringLiteral("⚠ %1 警告").arg(n));
}

void LayoutWindow::openEnvManager() {
    PrintEnvironmentManager dlg(this);
    dlg.exec();
}

int  LayoutWindow::modeDays(const QString& key) const { return modeDef(key)->days; }
bool LayoutWindow::modeStale(const QString& key) const {
    return modeState(key) != cal::CalibState::FRESH;
}

void LayoutWindow::selectTabForShot(int tab) {
    if (rightTabs_ && tab >= 0 && tab < rightTabs_->count())
        rightTabs_->setCurrentIndex(tab);
}

void LayoutWindow::runActionForShot(const QString& action) {
    if (action.startsWith(QLatin1String("zoom:")))   // zoom:+30 / zoom:-40（复现缩放问题用）
        canvas_->zoomBy(action.mid(5).toInt());
    else if (action == QLatin1String("marks")) canvas_->toggleCropMarks();
    else if (action == QLatin1String("grid"))  canvas_->toggleGrid();
    else if (action == QLatin1String("guides")) {   // 演示参考线（截图验证）
        canvas_->addGuideV(150); canvas_->addGuideV(305);
        canvas_->addGuideH(400);
    }
    else if (action == QLatin1String("submit")) submitLayoutToQueue();
    else if (action == QLatin1String("nest")) runAutoNest();
    else if (action == QLatin1String("settings")) {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle(QStringLiteral("设定 — 量测设备"));
        dlg->resize(560, 520);
        auto* v = new QVBoxLayout(dlg);
        v->setContentsMargins(0, 0, 0, 0);
        v->addWidget(new PageMeasurementDevices(dlg));
        dlg->show();
    }
    else if (action == QLatin1String("qr")) {
        CanvasBlock b;
        b.isQr = true;
        b.qrData = QStringLiteral("https://jcolour.app/e/2026");
        b.name = QStringLiteral("二维码");
        b.modeKey = QStringLiteral("mono");
        b.color = Qt::white;
        b.x = 470; b.y = 700; b.w = 90; b.h = 90;
        canvas_->blocks().push_back(b);
        canvas_->select(int(canvas_->blocks().size()) - 1);
        canvas_->update();
    }
}

// ---- real-module wiring -----------------------------------------------------

void LayoutWindow::submitLayoutToQueue() {
    // 打印整版：版面上每个图像块 → 一个 queue::PrintJob。
    // 黑墨类型按模式归类（单黑/黑+灰/黑+灰+彩 在 Photo Rag 上走 PK；RGB 类 Either），
    // 让 PrintQueueWidget 的 PK/MK 批次优化真正参与排序。
    if (!queueWidget_) return;
    const QString printer = printerCombo_ ? printerCombo_->currentText() : QStringLiteral("P9500");
    const QString paper   = paperCombo_   ? paperCombo_->currentText()   : QStringLiteral("Photo Rag");
    int submitted = 0;
    for (const auto& b : canvas_->blocks()) {
        if (b.isText) continue;   // 文字随版面光栅，不单独成 RIP 工作
        const ModeDef* m = modeDef(b.modeKey);
        jcolour::queue::PrintJob job;
        job.id          = b.name + "-" + QDateTime::currentDateTime().toString("hhmmsszzz");
        job.imagePath   = b.name + ".tif";
        job.presetName  = printer + QStringLiteral(" · ") + paper + QStringLiteral(" · ") +
                          QString::fromUtf8(m->name) + QStringLiteral("（") +
                          QString::fromUtf8(m->lin) + QStringLiteral("）");
        job.copies      = 1;
        job.status      = QStringLiteral("pending");
        job.submittedAt = QDateTime::currentDateTime();
        job.blackInk    = (b.modeKey == QLatin1String("rgb") || b.modeKey == QLatin1String("rgb16"))
                              ? jcolour::queue::BlackInk::Either
                              : jcolour::queue::BlackInk::PK;
        queueWidget_->addJob(job);
        ++submitted;
    }
    queueWidget_->startProcessing();
    if (rightTabs_) rightTabs_->setCurrentIndex(3);   // 跳到队列
    statusBar()->showMessage(
        QStringLiteral("已送 %1 个工作到 RIP 队列（%2 · %3）").arg(submitted).arg(printer, paper), 5000);
}

void LayoutWindow::runAutoNest() {
    // 自动拼版：现有图像块喂给 NestingWidget（真实引擎模块），取回排好的位置。
    NestingWidget nester;
    nester.setPaperSize(kPaperW, kPaperH);
    nester.setMargins(10, 10, 10, 10);
    nester.setGutter(6);
    auto& blocks = canvas_->blocks();
    for (const auto& b : blocks) {
        if (b.isText) continue;
        jcolour::nesting::PrintItem it;
        it.id        = b.name;
        it.imagePath = b.name + ".tif";
        it.bounds    = QRectF(b.x, b.y, b.w, b.h);
        it.rotation  = 0;
        it.copies    = 1;
        nester.addItem(it);
    }
    const auto res = nester.nest();
    for (const auto& placed : res.placedItems)
        for (auto& b : blocks)
            if (b.name == placed.id && !b.isText) {
                b.x = placed.bounds.x();
                b.y = placed.bounds.y();
                b.w = placed.bounds.width();
                b.h = placed.bounds.height();
            }
    canvas_->update();
    syncPropBarFromBlock(canvas_->selected());
    statusBar()->showMessage(
        QStringLiteral("自动拼版完成 · 利用率 %1% · %2 页")
            .arg(int(res.utilization * 100)).arg(qMax(1, res.pagesRequired)), 5000);
}

}  // namespace jcolour::ui
