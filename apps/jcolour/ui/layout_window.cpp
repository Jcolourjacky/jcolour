#include "layout_window.h"
#include "print_env_manager.h"
#include "page_measurement_devices.h"
#include "icons.h"
#include "spot_tuner_dialog.h"
#include "til_wizard.h"
#include "../paper_state.h"
#include "../queue.h"
#include "../nesting.h"
#include "../step_repeat.h"
#include "../contact_sheet.h"
#include "state_machine.h"

#include <QInputDialog>
#include <QDialog>
#include <QTextEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QActionGroup>
#include <QTimer>
#include <QSettings>
#include <QTableWidget>
#include <QHeaderView>

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
    static const QColor kOff(0x4a, 0x47, 0x40);
    static const QColor kOn(0xfa, 0xfa, 0xf7);
    auto* b = new QToolButton(parent);
    b->setIcon(ic::toggleIcon(glyph, kOff, kOn, 17));
    b->setIconSize(QSize(17, 17));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setFixedSize(26, 24);
    // 1a 纸感：选中工具 = 靛蓝底 + 纸白图标（设计稿工具列样式）
    b->setStyleSheet(
        "QToolButton{border:none;border-radius:6px;}"
        "QToolButton:hover{background:rgba(16,10,54,.08);}"
        "QToolButton:checked{background:#100A36;}");
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
    if (sel_ < 0 || sel_ >= blocks_.size() || multiSel_.size() > 1) return -1;
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
    if (idx == sel_ && multiSel_.size() <= 1) return;
    sel_ = idx;
    multiSel_.clear();
    if (idx >= 0) multiSel_.push_back(idx);
    update();
    emit selectionChanged(sel_);
}

int LayoutCanvas::zoomPercent() const {
    return int((zoom_ > 0 ? zoom_ : 0.8) / 0.8 * 100 + 0.5);
}

void LayoutCanvas::zoomBy(int dp) {
    finishTextEdit(true);
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
    g.fillRect(rect(), QColor("#ECEAE3"));
    for (int yy = 0; yy < height(); yy += 22)
        for (int xx = ((yy / 22) % 2) * 22; xx < width(); xx += 44)
            g.fillRect(xx, yy, 22, 22, QColor("#E3E0D7"));

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
            tf.setFamilies({QStringLiteral("Source Serif 4"), QStringLiteral("Georgia"),
                            QStringLiteral("Songti SC")});   // 1a：标题衬线
            tf.setPointSizeF(qMax(7.0, r.height() * 0.42));
            tf.setBold(true);
            g.setFont(tf);
            g.setPen(QColor("#100A36"));
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

        if (i != sel_ && multiSel_.contains(i)) {   // 多选中的次级块：细蓝框
            g.setPen(QPen(QColor("#0BBAEE"), 1.1));
            g.setBrush(Qt::NoBrush);
            g.drawRect(r);
        }
        if (i == sel_) {
            g.setPen(QPen(QColor("#0BBAEE"), 1.6));
            g.setBrush(Qt::NoBrush);
            g.drawRect(r);
            QPen dash(QColor(46, 125, 215, 115), 1, Qt::DashLine);
            g.setPen(dash);
            g.drawRect(r.adjusted(-5, -5, 5, 5));
            if (multiSel_.size() > 1) { /* 群选时不画缩放手柄（InDesign 语义） */ }
            else {
            g.setPen(QPen(Qt::white, 1));
            g.setBrush(QColor("#0BBAEE"));
            const QPointF c[8] = {
                r.topLeft(), {r.center().x(), r.top()}, r.topRight(),
                {r.left(), r.center().y()}, {r.right(), r.center().y()},
                r.bottomLeft(), {r.center().x(), r.bottom()}, r.bottomRight()};
            for (const auto& pt : c) g.drawEllipse(pt, 3.4, 3.4);
            }
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

    // 分幅 Tiling 叠加：虚线幅界 + 重叠搭接带 + 幅号（交替旋转幅标 ↻）
    for (const auto& b : blocks_) {
        if (!b.tiled() || b.isText) continue;
        const QRectF r = blockRectPx(b);
        const double ovPx = b.tileOverlapMm * s;
        const double tw = r.width() / b.tileCols, th = r.height() / b.tileRows;
        g.setPen(QPen(QColor("#100A36"), 1, Qt::DashLine));
        for (int c = 1; c < b.tileCols; ++c)
            g.drawLine(QPointF(r.x() + c * tw, r.y()), QPointF(r.x() + c * tw, r.bottom()));
        for (int rr = 1; rr < b.tileRows; ++rr)
            g.drawLine(QPointF(r.x(), r.y() + rr * th), QPointF(r.right(), r.y() + rr * th));
        // 重叠带（沿内部幅界两侧各 overlap/2）
        if (ovPx > 0.5) {
            g.setPen(Qt::NoPen);
            g.setBrush(QColor(11, 186, 238, 46));
            for (int c = 1; c < b.tileCols; ++c)
                g.drawRect(QRectF(r.x() + c * tw - ovPx / 2, r.y(), ovPx, r.height()));
            for (int rr = 1; rr < b.tileRows; ++rr)
                g.drawRect(QRectF(r.x(), r.y() + rr * th - ovPx / 2, r.width(), ovPx));
        }
        // 幅号（行主序）+ 交替旋转标记
        QFont tf2 = g.font(); tf2.setPointSizeF(qMax(7.0, th * 0.16)); tf2.setBold(true);
        g.setFont(tf2);
        for (int rr = 0; rr < b.tileRows; ++rr)
            for (int c = 0; c < b.tileCols; ++c) {
                const int idx = rr * b.tileCols + c;
                const bool rot = b.tileAlternate && (idx % 2 == 1);
                const QRectF cell(r.x() + c * tw, r.y() + rr * th, tw, th);
                g.setPen(QColor(255, 255, 255, 200));
                g.drawText(cell, Qt::AlignCenter,
                           QStringLiteral("%1%2").arg(idx + 1).arg(rot ? QStringLiteral(" ↻") : QString()));
            }
    }

    // 裁切角线（每块四角，打印后裁纸用）
    if (showCropMarks_) {
        g.setPen(QPen(QColor("#100A36"), 1));
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

    // 框选橡皮筋
    if (marquee_) {
        const QRectF mr = QRectF(marqueeA_, marqueeB_).normalized();
        g.setPen(QPen(QColor(46, 125, 215, 160), 1, Qt::DashLine));
        g.setBrush(QColor(46, 125, 215, 24));
        g.drawRect(mr);
    }

    // 文字框拖拽预览（虚线框）
    if (creatingText_ && textDragMm_.isValid()) {
        g.setPen(QPen(QColor("#0BBAEE"), 1, Qt::DashLine));
        g.setBrush(Qt::NoBrush);
        g.drawRect(QRectF(p.x() + textDragMm_.x() * s, p.y() + textDragMm_.y() * s,
                          textDragMm_.width() * s, textDragMm_.height() * s));
    }

    // 参考线（InDesign 青色，横竖贯穿画布内容区；Cmd+; 显隐）
    if (showGuides_ && (!guidesV_.isEmpty() || !guidesH_.isEmpty())) {
        g.setPen(QPen(QColor("#0BBAEE"), 1));
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
            const double v = list[idx] - (dragGuideKind_ == 1 ? rulerOrigin_.x() : rulerOrigin_.y());
            g.setPen(Qt::white);
            const QString txt = QStringLiteral("%1 mm").arg(v, 0, 'f', 1);
            const QRectF tr(cursorPx_.x() + 10, cursorPx_.y() - 22, 74, 17);
            g.setBrush(QColor("#0BBAEE"));
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
        g.setPen(QPen(QColor("#E71655"), 1.4));
        g.drawLine(a, b);
        g.setBrush(QColor("#E71655"));
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
    g.setBrush(QColor("#F1EFE8"));
    g.drawRect(QRectF(kRuler, 0, width() - kRuler, kRuler));
    g.drawRect(QRectF(0, kRuler, kRuler, height() - kRuler));
    g.drawRect(QRectF(0, 0, kRuler, kRuler));
    g.setPen(QColor("#DDD9CF"));
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
    // 刻度以零位（rulerOrigin_，角落方块可拖动调整）为基准；零点左/上侧为负值
    const double stepMm = minorU * unitMm;
    const auto isLbl = [labelEvery](int k) { return ((k % labelEvery) + labelEvery) % labelEvery == 0; };
    {
        const double o = rulerOrigin_.x();
        const int k0 = int(std::floor((0 - o) / stepMm)), k1 = int(std::ceil((kPaperW - o) / stepMm));
        for (int k = k0; k <= k1; ++k) {
            const double mmPos = o + k * stepMm;
            if (mmPos < -1e-9 || mmPos > kPaperW + 1e-9) continue;
            const double x = p.x() + mmPos * s;
            if (x < kRuler || x > width()) continue;
            const bool lab = isLbl(k);
            g.setPen(lab ? QColor("#8A8779") : QColor("#C9C5B9"));
            g.drawLine(QPointF(x, kRuler - (lab ? 6 : 3)), QPointF(x, kRuler));
            if (lab)
                g.drawText(QRectF(x + 2, 1, 46, kRuler - 5), Qt::AlignLeft | Qt::AlignBottom,
                           fmt(k * minorU));
        }
    }
    {
        const double o = rulerOrigin_.y();
        const int k0 = int(std::floor((0 - o) / stepMm)), k1 = int(std::ceil((kPaperH - o) / stepMm));
        for (int k = k0; k <= k1; ++k) {
            const double mmPos = o + k * stepMm;
            if (mmPos < -1e-9 || mmPos > kPaperH + 1e-9) continue;
            const double y = p.y() + mmPos * s;
            if (y < kRuler || y > height()) continue;
            const bool lab = isLbl(k);
            g.setPen(lab ? QColor("#8A8779") : QColor("#C9C5B9"));
            g.drawLine(QPointF(kRuler - (lab ? 6 : 3), y), QPointF(kRuler, y));
            if (lab) {
                g.save();
                g.translate(2, y + 2);
                g.rotate(90);
                g.drawText(0, 0, fmt(k * minorU));
                g.restore();
            }
        }
    }

    // 零位拖动中：十字线跟随光标（InDesign 拖零点的视觉）
    if (draggingOrigin_ && cursorPx_.x() > 0) {
        g.setPen(QPen(QColor("#8A8779"), 1, Qt::DashLine));
        g.drawLine(QPointF(cursorPx_.x(), 0), QPointF(cursorPx_.x(), height()));
        g.drawLine(QPointF(0, cursorPx_.y()), QPointF(width(), cursorPx_.y()));
    }

    // 光标位置指示（InDesign 的标尺细线）
    if (cursorPx_.x() > kRuler && cursorPx_.y() > kRuler) {
        g.setPen(QColor("#E71655"));
        g.drawLine(QPointF(cursorPx_.x(), 0), QPointF(cursorPx_.x(), kRuler));
        g.drawLine(QPointF(0, cursorPx_.y()), QPointF(kRuler, cursorPx_.y()));
    }

    // 左上角单位标（点击右键换单位）
    g.setPen(QColor("#8A8779"));
    g.drawText(QRectF(0, 0, kRuler, kRuler), Qt::AlignCenter,
               unit_ == Unit::MM ? "mm" : unit_ == Unit::CM ? "cm" : "in");
}

void LayoutCanvas::mousePressEvent(QMouseEvent* e) {
    const QPointF pos = e->position();
    setFocus();

    // ---- InDesign 式：角落方块拖出 = 调整标尺零位 ----
    if (showRulers_ && e->button() == Qt::LeftButton &&
        pos.x() < kRuler && pos.y() < kRuler) {
        draggingOrigin_ = true;
        update();
        return;
    }

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
        finishTextEdit(true);
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

    // ---- 文字工具：InDesign 式拖拽建文本框（松手后框内直接输入）----
    if (tool_ == CanvasTool::Text) {
        const QPointF mm = pxToMm(pos);
        if (mm.x() >= 0 && mm.x() <= kPaperW && mm.y() >= 0 && mm.y() <= kPaperH) {
            creatingText_ = true;
            textStartMm_ = mm;
            textDragMm_ = QRectF(mm, mm);
        }
        return;
    }

    // ---- insert tools: 二维码 click-to-place ----
    if (tool_ == CanvasTool::Qr) {
        const QPointF mm = pxToMm(pos);
        if (mm.x() >= 0 && mm.x() <= kPaperW && mm.y() >= 0 && mm.y() <= kPaperH) {
            CanvasBlock b;
            {
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

    // ---- select / drag（InDesign 选择模型）----
    for (int i = blocks_.size() - 1; i >= 0; --i) {
        const QRectF r = blockRectPx(blocks_[i]);
        if (!r.contains(pos)) continue;
        if (e->modifiers() & Qt::ShiftModifier) {          // Shift 加选/减选
            const int at = multiSel_.indexOf(i);
            if (at >= 0) { if (multiSel_.size() > 1) multiSel_.remove(at); }
            else multiSel_.push_back(i);
            sel_ = multiSel_.isEmpty() ? -1 : multiSel_.last();
            update();
            emit selectionChanged(sel_);
            return;
        }
        if (!multiSel_.contains(i)) select(i);             // 点中未选块 → 单选
        else { sel_ = i; emit selectionChanged(sel_); }    // 点中已选块 → 保留群选拖动
        dragging_ = true;
        const QRectF p = paperRectPx();
        const double s = p.width() / kPaperW;
        dragOffsetMm_ = QPointF((pos.x() - r.x()) / s, (pos.y() - r.y()) / s);
        dragStart_.clear();
        for (int k : multiSel_) dragStart_.push_back({blocks_[k].x, blocks_[k].y});
        return;
    }

    // ---- 空白处：清选择 + 框选（InDesign 橡皮筋）----
    if (tool_ == CanvasTool::Select) {
        select(-1);
        marquee_ = true;
        marqueeA_ = marqueeB_ = pos;
    }
}

void LayoutCanvas::mouseMoveEvent(QMouseEvent* e) {
    const QPointF pos = e->position();
    cursorPx_ = pos;                       // 标尺光标位置指示
    if (showRulers_) update();

    // ---- 文字框拖拽中（虚线预览）----
    if (creatingText_) {
        const QPointF mm = pxToMm(pos);
        textDragMm_ = QRectF(textStartMm_, mm).normalized()
                          .intersected(QRectF(0, 0, kPaperW, kPaperH));
        update(); return;
    }

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

    // ---- 测量中（Shift 锁 0°/45°/90°，InDesign 同款）----
    if (measuring_) {
        measB_ = pxToMm(pos);
        if (e->modifiers() & Qt::ShiftModifier) {
            const double dx = measB_.x() - measA_.x(), dy = measB_.y() - measA_.y();
            const double ang = std::atan2(dy, dx);
            const double snap = std::round(ang / (M_PI / 4)) * (M_PI / 4);
            const double len = std::hypot(dx, dy);
            measB_ = measA_ + QPointF(len * std::cos(snap), len * std::sin(snap));
        }
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

    // ---- 零位拖动中（十字线跟随，见 paint）----
    if (draggingOrigin_) { update(); return; }

    // ---- marquee 框选中 ----
    if (marquee_) {
        marqueeB_ = pos;
        update(); return;
    }

    // ---- drag-move in progress（群体移动，主块吸附参考线）----
    if (dragging_ && sel_ >= 0) {
        const QRectF p = paperRectPx();
        const double s = p.width() / kPaperW;
        const int primaryAt = multiSel_.indexOf(sel_);
        CanvasBlock& b = blocks_[sel_];
        const QPointF want((pos.x() - p.x()) / s - dragOffsetMm_.x(),
                           (pos.y() - p.y()) / s - dragOffsetMm_.y());
        double nx = qBound(0.0, want.x(), kPaperW - b.w);
        double ny = qBound(0.0, want.y(), kPaperH - b.h);
        for (double g : guidesV_) {
            if (qAbs(nx - g) < 1.5)             nx = qBound(0.0, g, kPaperW - b.w);
            else if (qAbs(nx + b.w - g) < 1.5)  nx = qBound(0.0, g - b.w, kPaperW - b.w);
        }
        for (double g : guidesH_) {
            if (qAbs(ny - g) < 1.5)             ny = qBound(0.0, g, kPaperH - b.h);
            else if (qAbs(ny + b.h - g) < 1.5)  ny = qBound(0.0, g - b.h, kPaperH - b.h);
        }
        // 群移：主块相对起始位移套用到所有选中块
        const QPointF delta(nx - dragStart_[primaryAt].x(), ny - dragStart_[primaryAt].y());
        for (int k = 0; k < multiSel_.size(); ++k) {
            CanvasBlock& bb = blocks_[multiSel_[k]];
            bb.x = qBound(0.0, dragStart_[k].x() + delta.x(), kPaperW - bb.w);
            bb.y = qBound(0.0, dragStart_[k].y() + delta.y(), kPaperH - bb.h);
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
    // 文字框建立：拖出的矩形即文本框；几乎没拖动 = 默认 90×22mm（InDesign 单击习惯）
    if (creatingText_) {
        creatingText_ = false;
        QRectF r = textDragMm_;
        textDragMm_ = QRectF();
        if (r.width() < 8 || r.height() < 6)
            r = QRectF(qBound(0.0, textStartMm_.x(), kPaperW - 90.0),
                       qBound(0.0, textStartMm_.y(), kPaperH - 22.0), 90, 22);
        CanvasBlock b;
        b.isText = true;
        b.text.clear();
        b.name = QStringLiteral("文字");
        b.x = r.x(); b.y = r.y(); b.w = r.width(); b.h = r.height();
        blocks_.push_back(b);
        sel_ = blocks_.size() - 1;
        update();
        emit blockAdded(sel_);
        emit selectionChanged(sel_);
        startTextEdit(sel_);          // InDesign：建框即进入输入状态
        return;
    }
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
    // 零位落定（InDesign：松手处即新零点）
    if (draggingOrigin_) {
        draggingOrigin_ = false;
        rulerOrigin_ = pxToMm(e->position());
        update();
    }
    // 框选落定：选中与框相交的块（InDesign 橡皮筋语义）
    if (marquee_) {
        marquee_ = false;
        const QRectF mr = QRectF(marqueeA_, marqueeB_).normalized();
        if (mr.width() > 4 || mr.height() > 4) {
            multiSel_.clear();
            for (int i = 0; i < blocks_.size(); ++i)
                if (blockRectPx(blocks_[i]).intersects(mr)) multiSel_.push_back(i);
            sel_ = multiSel_.isEmpty() ? -1 : multiSel_.last();
            emit selectionChanged(sel_);
        }
        update();
    }
    measuring_ = false;
    dragging_ = false; resizing_ = -1;
}

void LayoutCanvas::setTool(CanvasTool t) {
    finishTextEdit(true);
    tool_ = t;
    setCursor(t == CanvasTool::Hand ? Qt::OpenHandCursor
              : t == CanvasTool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
    emit toolChanged(t);
}

void LayoutCanvas::startTextEdit(int idx) {
    if (idx < 0 || idx >= blocks_.size() || !blocks_[idx].isText) return;
    finishTextEdit(true);
    editBlock_ = idx;
    editor_ = new QTextEdit(this);
    const QRectF r = blockRectPx(blocks_[idx]);
    editor_->setGeometry(r.toRect());
    QFont f = font();
    f.setPointSizeF(qMax(7.0, r.height() * 0.42));
    f.setBold(true);
    editor_->setFont(f);
    editor_->setPlainText(blocks_[idx].text);
    editor_->setStyleSheet(
        "QTextEdit{background:rgba(255,255,255,235);color:#100A36;"
        "border:1px solid #0BBAEE;border-radius:0;padding:0;}");
    editor_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor_->installEventFilter(this);
    editor_->show();
    editor_->setFocus();
    editor_->moveCursor(QTextCursor::End);
}

void LayoutCanvas::finishTextEdit(bool commit) {
    if (!editor_) return;
    QTextEdit* ed = editor_;
    editor_ = nullptr;                      // 先置空防重入（focusOut 会再触发）
    const int idx = editBlock_;
    editBlock_ = -1;
    if (idx >= 0 && idx < blocks_.size() && blocks_[idx].isText) {
        const QString t = ed->toPlainText();
        if (commit && !t.trimmed().isEmpty()) {
            blocks_[idx].text = t;
        } else if (blocks_[idx].text.isEmpty()) {
            blocks_.remove(idx);            // 空文本框 = 取消（InDesign 同款）
            if (sel_ == idx) sel_ = -1;
            emit selectionChanged(sel_);
        }
    }
    ed->deleteLater();
    setFocus();
    update();
}

bool LayoutCanvas::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == editor_) {
        if (ev->type() == QEvent::FocusOut) { finishTextEdit(true); return false; }
        if (ev->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(ev);
            if (ke->key() == Qt::Key_Escape) { finishTextEdit(true); return true; }
        }
    }
    return QWidget::eventFilter(obj, ev);
}

void LayoutCanvas::zoomAtBy(const QPointF& anchorPx, int dp) {
    const QPointF mm = pxToMm(anchorPx);
    zoomBy(dp);
    // 让锚点下的纸面位置保持在光标下（InDesign/所有排版软件的缩放手感）
    const QRectF p = paperRectPx();
    const double s = p.width() / kPaperW;
    scrollX_ += anchorPx.x() - (p.x() + mm.x() * s);
    scroll_  -= anchorPx.y() - (p.y() + mm.y() * s);
    clampScroll();
    update();
}

void LayoutCanvas::stepRepeatGrid(int rows, int cols, double gapXmm, double gapYmm) {
    if (sel_ < 0 || sel_ >= blocks_.size()) return;
    const CanvasBlock src = blocks_[sel_];
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            if (r == 0 && c == 0) continue;             // 原块即 (0,0)
            CanvasBlock b = src;
            b.x = src.x + c * (src.w + gapXmm);
            b.y = src.y + r * (src.h + gapYmm);
            if (b.x + b.w > kPaperW + 0.01 || b.y + b.h > kPaperH + 0.01) continue;  // 出纸不放
            blocks_.push_back(b);
        }
    sel_ = blocks_.size() - 1;
    update();
    emit blockAdded(sel_);
    emit selectionChanged(sel_);
}

void LayoutCanvas::keyPressEvent(QKeyEvent* e) {
    // 空格按住 = 临时抓手（InDesign/Photoshop 通用）
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat() && !spacePan_ && !editor_) {
        spacePan_ = true;
        prevTool_ = tool_;
        tool_ = CanvasTool::Hand;
        setCursor(Qt::OpenHandCursor);
        return;
    }
    // 单键工具切换（InDesign：V 选择 · T 文字 · H 抓手 · K 测量）
    if (!editor_ && e->modifiers() == Qt::NoModifier) {
        switch (e->key()) {
            case Qt::Key_V: setTool(CanvasTool::Select);  return;
            case Qt::Key_T: setTool(CanvasTool::Text);    return;
            case Qt::Key_H: setTool(CanvasTool::Hand);    return;
            case Qt::Key_K: setTool(CanvasTool::Measure); return;
            default: break;
        }
    }
    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) { deleteSelected(); return; }
    if ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_D) { duplicateSelected(); return; }
    if (e->key() == Qt::Key_Escape && hasMeasure_) { hasMeasure_ = false; update(); return; }
    QWidget::keyPressEvent(e);
}

void LayoutCanvas::keyReleaseEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat() && spacePan_) {
        spacePan_ = false;
        panning_ = false;
        tool_ = prevTool_;
        setCursor(tool_ == CanvasTool::Hand ? Qt::OpenHandCursor
                  : tool_ == CanvasTool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
        return;
    }
    QWidget::keyReleaseEvent(e);
}

void LayoutCanvas::mouseDoubleClickEvent(QMouseEvent* e) {
    const QPointF pos = e->position();
    // 双击角落方块 = 零位复位到纸张左上角（InDesign 同款）
    if (showRulers_ && pos.x() < kRuler && pos.y() < kRuler) {
        rulerOrigin_ = QPointF(0, 0);
        update();
        return;
    }
    // 双击标尺 = 在该处加参考线（InDesign：顶标尺加竖线，左标尺加横线）
    if (showRulers_ && pos.y() < kRuler && pos.x() >= kRuler) { addGuideV(pxToMm(pos).x()); return; }
    if (showRulers_ && pos.x() < kRuler && pos.y() >= kRuler) { addGuideH(pxToMm(pos).y()); return; }
    // 双击文字块 = 进入编辑（InDesign：选择工具双击进入文字编辑）
    for (int i = blocks_.size() - 1; i >= 0; --i)
        if (blocks_[i].isText && blockRectPx(blocks_[i]).contains(pos)) {
            select(i);
            startTextEdit(i);
            return;
        }
    QWidget::mouseDoubleClickEvent(e);
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
    finishTextEdit(true);
    if (e->modifiers() & Qt::ControlModifier) {
        zoomAtBy(e->position(), e->angleDelta().y() > 0 ? 10 : -10);   // 锚定光标
    } else {
        scroll_  -= e->angleDelta().y() * 0.6;
        scrollX_ += e->angleDelta().x() * 0.6;   // 触控板横向双指
        clampScroll();
        update();
    }
}

void LayoutCanvas::resizeEvent(QResizeEvent*) { clampScroll(); }

void LayoutCanvas::duplicateSelected() {
    if (multiSel_.isEmpty()) return;
    QVector<int> clones;
    for (int k : multiSel_) {                 // InDesign：整个选区一起复制
        CanvasBlock b = blocks_[k];
        b.x = qMin(b.x + 12.0, kPaperW - b.w);   // 错位 12mm，不越纸
        b.y = qMin(b.y + 12.0, kPaperH - b.h);
        blocks_.push_back(b);
        clones.push_back(blocks_.size() - 1);
    }
    multiSel_ = clones;                       // 新副本成为当前选区
    sel_ = clones.last();
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
    if (multiSel_.isEmpty()) return;
    for (int k : multiSel_) {
        auto& b = blocks_[k];
        std::swap(b.w, b.h);
        b.x = qBound(0.0, b.x, kPaperW - b.w);   // 旋转后拉回纸内
        b.y = qBound(0.0, b.y, kPaperH - b.h);
    }
    update();
    emit blockMoved(sel_);
}

void LayoutCanvas::deleteSelected() {
    if (multiSel_.isEmpty()) return;
    QVector<int> idx = multiSel_;
    std::sort(idx.begin(), idx.end(), std::greater<int>());
    for (int k : idx) blocks_.remove(k);
    sel_ = -1;
    multiSel_.clear();
    update();
    emit selectionChanged(-1);
}

void LayoutCanvas::alignSelected(Align a) {
    if (multiSel_.isEmpty()) return;
    const bool toPage = alignTo_ == AlignTo::Page;
    if (!toPage && multiSel_.size() < 2) return;   // InDesign：对齐到选区需 ≥2 个对象
    // 参照边界：页面 或 选区包围盒
    double L = 0, T = 0, R = kPaperW, B = kPaperH;
    if (!toPage) {
        L = 1e9; T = 1e9; R = -1e9; B = -1e9;
        for (int k : multiSel_) {
            const auto& b = blocks_[k];
            L = qMin(L, b.x); T = qMin(T, b.y);
            R = qMax(R, b.x + b.w); B = qMax(B, b.y + b.h);
        }
    }
    for (int k : multiSel_) {
        auto& b = blocks_[k];
        switch (a) {
            case Align::Left:    b.x = L; break;
            case Align::HCenter: b.x = (L + R) / 2 - b.w / 2; break;
            case Align::Right:   b.x = R - b.w; break;
            case Align::Top:     b.y = T; break;
            case Align::VCenter: b.y = (T + B) / 2 - b.h / 2; break;
            case Align::Bottom:  b.y = B - b.h; break;
        }
        b.x = qBound(0.0, b.x, kPaperW - b.w);
        b.y = qBound(0.0, b.y, kPaperH - b.h);
    }
    update();
    emit blockMoved(sel_);
}

void LayoutCanvas::distributeSelected(bool horizontal, bool centers) {
    if (multiSel_.size() < 3) return;              // InDesign：分布需 ≥3 个对象
    QVector<int> idx = multiSel_;
    std::sort(idx.begin(), idx.end(), [this, horizontal](int a, int b) {
        return horizontal ? blocks_[a].x < blocks_[b].x : blocks_[a].y < blocks_[b].y;
    });
    const int n = idx.size();
    if (centers) {
        // 居中分布：首末中心不动，其余中心等距
        auto center = [this, horizontal](int k) {
            const auto& b = blocks_[k];
            return horizontal ? b.x + b.w / 2 : b.y + b.h / 2;
        };
        const double c0 = center(idx.first()), c1 = center(idx.last());
        for (int i = 1; i < n - 1; ++i) {
            auto& b = blocks_[idx[i]];
            const double c = c0 + (c1 - c0) * i / (n - 1);
            if (horizontal) b.x = qBound(0.0, c - b.w / 2, kPaperW - b.w);
            else            b.y = qBound(0.0, c - b.h / 2, kPaperH - b.h);
        }
    } else {
        // 等间距：首末外缘不动，块间空隙相等
        double span = 0, sum = 0;
        const auto& f = blocks_[idx.first()];
        const auto& l = blocks_[idx.last()];
        span = horizontal ? (l.x + l.w - f.x) : (l.y + l.h - f.y);
        for (int k : idx) sum += horizontal ? blocks_[k].w : blocks_[k].h;
        const double gap = (span - sum) / (n - 1);
        double cur = horizontal ? f.x : f.y;
        for (int i = 0; i < n; ++i) {
            auto& b = blocks_[idx[i]];
            if (horizontal) { b.x = qBound(0.0, cur, kPaperW - b.w); cur = b.x + b.w + gap; }
            else            { b.y = qBound(0.0, cur, kPaperH - b.h); cur = b.y + b.h + gap; }
        }
    }
    update();
    emit blockMoved(sel_);
}

bool LayoutCanvas::event(QEvent* e) {
    // macOS 触控板捏合缩放；标尺/纸张同一 paint 通道，天然跟随
    if (e->type() == QEvent::NativeGesture) {
        auto* g = static_cast<QNativeGestureEvent*>(e);
        if (g->gestureType() == Qt::ZoomNativeGesture) {
            zoomAtBy(g->position(), int(g->value() * 100));   // 捏合锚定指下
            return true;
        }
    }
    return QWidget::event(e);
}

// ============================================================================
// LayoutWindow
// ============================================================================

// ---- ErgoSoft 式底部 RIP 队列监控条 -----------------------------------------
// 常驻一行（可折叠）：作业芯片流（名称+状态色点+状态），标题行给
// 待处理/打印中/完成 汇总。数据从 PrintQueueWidget 快照拉取（800ms +
// 队列信号触发刷新），与右侧「队列」页共用同一数据源。

class QueueMonitorStrip : public QWidget {
public:
    explicit QueueMonitorStrip(PrintQueueWidget* queue, QWidget* parent = nullptr)
        : QWidget(parent), queue_(queue) {
        setStyleSheet("background:#F1EFE8;border-top:1px solid #DDD9CF;");
        auto* v = new QVBoxLayout(this);
        v->setContentsMargins(8, 2, 8, 4);
        v->setSpacing(3);

        auto* head = new QHBoxLayout;
        toggle_ = new QToolButton;
        toggle_->setText(QStringLiteral("▾"));
        toggle_->setAutoRaise(true);
        toggle_->setFixedSize(18, 16);
        title_ = new QLabel(QStringLiteral("RIP 队列"));
        title_->setStyleSheet("font-size:10.5px;font-weight:700;color:#100A36;");
        summary_ = new QLabel;
        summary_->setStyleSheet("font-size:10px;color:#7A776D;");
        head->addWidget(toggle_);
        head->addWidget(title_);
        head->addWidget(summary_);
        head->addStretch();
        v->addLayout(head);

        chipRow_ = new QWidget;
        chipLay_ = new QHBoxLayout(chipRow_);
        chipLay_->setContentsMargins(0, 0, 0, 0);
        chipLay_->setSpacing(6);
        chipLay_->addStretch();
        v->addWidget(chipRow_);

        QSettings s;
        collapsed_ = s.value("ui/queueStripCollapsed", false).toBool();
        applyCollapsed();
        connect(toggle_, &QToolButton::clicked, this, [this] {
            collapsed_ = !collapsed_;
            QSettings().setValue("ui/queueStripCollapsed", collapsed_);
            applyCollapsed();
        });

        auto* timer = new QTimer(this);
        timer->setInterval(800);
        connect(timer, &QTimer::timeout, this, [this] { refresh(); });
        timer->start();
        if (queue_) {
            connect(queue_, &PrintQueueWidget::jobStarted,   this, [this](const QString&) { refresh(); });
            connect(queue_, &PrintQueueWidget::jobCompleted, this, [this](const QString&) { refresh(); });
            connect(queue_, &PrintQueueWidget::jobFailed,    this,
                    [this](const QString&, const QString&) { refresh(); });
        }
        refresh();
    }

private:
    void applyCollapsed() {
        chipRow_->setVisible(!collapsed_);
        toggle_->setText(collapsed_ ? QStringLiteral("▸") : QStringLiteral("▾"));
    }

    void refresh() {
        if (!queue_) return;
        const auto pending = queue_->pendingSnapshot();
        const auto done    = queue_->completedSnapshot();
        int failed = 0;
        for (const auto& j : done) if (j.status == QLatin1String("failed")) ++failed;
        summary_->setText(QStringLiteral("·  %1 待处理 · %2 完成%3%4")
            .arg(pending.size()).arg(done.size() - failed)
            .arg(failed ? QStringLiteral(" · %1 失败").arg(failed) : QString())
            .arg(queue_->processing() ? QStringLiteral(" · 处理中…") : QString()));

        // 重建芯片（数量小，直接重建）
        while (chipLay_->count() > 1) {
            auto* item = chipLay_->takeAt(0);
            delete item->widget();
            delete item;
        }
        auto addChip = [this](const jcolour::queue::PrintJob& j) {
            const bool fail = j.status == QLatin1String("failed");
            const bool doneS = j.status == QLatin1String("completed");
            const bool printing = j.status == QLatin1String("printing");
            const char* dot = fail ? "#E71655" : doneS ? "#2F9E6B"
                             : printing ? "#0BBAEE" : "#C9C5B9";
            auto* chip = new QLabel(QStringLiteral(
                "<span style='color:%1'>●</span> %2")
                .arg(QString::fromLatin1(dot),
                     j.id.left(26).toHtmlEscaped()));
            chip->setStyleSheet(QStringLiteral(
                "font-size:9.5px;color:#4A4740;background:%1;border:1px solid #DDD9CF;"
                "border-radius:4px;padding:2px 7px;")
                .arg(printing ? QStringLiteral("#E7F7FD") : QStringLiteral("#FAFAF7")));
            chip->setToolTip(j.presetName + "\n" + j.status);
            chipLay_->insertWidget(chipLay_->count() - 1, chip);
        };
        int shown = 0;
        for (const auto& j : pending)  { if (shown++ < 14) addChip(j); }
        for (auto it = done.rbegin(); it != done.rend() && shown < 14; ++it, ++shown)
            addChip(*it);
        if (pending.isEmpty() && done.isEmpty()) {
            auto* empty = new QLabel(QStringLiteral("空 —— 「打印整版」提交作业后在此监控"));
            empty->setStyleSheet("font-size:9.5px;color:#8A8779;");
            chipLay_->insertWidget(0, empty);
        }
    }

    PrintQueueWidget* queue_ = nullptr;
    QToolButton* toggle_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* summary_ = nullptr;
    QWidget* chipRow_ = nullptr;
    QHBoxLayout* chipLay_ = nullptr;
    bool collapsed_ = false;
};


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
        zrow->setStyleSheet("background:#F1EFE8;border-top:1px solid #DDD9CF;");
        auto* zl = new QHBoxLayout(zrow);
        zl->setContentsMargins(8, 2, 8, 2);
        zl->addStretch();
        auto* zm = iconBtn(ic::Icon::Minus, QStringLiteral("缩小"));
        auto* zv = new QLabel("100%");
        zv->setStyleSheet("color:#4A4740;font-size:10px;min-width:34px;qproperty-alignment:AlignCenter;");
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
    rightPanelW_ = buildRightPanel();
    mid->addWidget(rightPanelW_);

    auto* midW = new QWidget;
    midW->setLayout(mid);
    col->addWidget(midW, 1);
    // ErgoSoft 式：常驻 RIP 队列监控条（可折叠）位于胶片条上方
    queueMonitor_ = new QueueMonitorStrip(queueWidget_);
    col->addWidget(queueMonitor_);
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
    view->addAction(QStringLiteral("显示/隐藏右侧面板"), QKeySequence(QStringLiteral("Ctrl+Alt+R")),
                    [this] { if (rightPanelW_) rightPanelW_->setVisible(!rightPanelW_->isVisible()); });
    view->addAction(QStringLiteral("显示/隐藏参考线"), QKeySequence(QStringLiteral("Ctrl+;")),
                    [this] { canvas_->toggleGuides(); });
    view->addAction(QStringLiteral("显示/隐藏标尺"), QKeySequence(QStringLiteral("Ctrl+R")),
                    [this] { canvas_->toggleRulers(); });
    view->addAction(QStringLiteral("软打样"))->setCheckable(true);
    view->addAction(QStringLiteral("色域警告"))->setCheckable(true);

    mb->addMenu(QStringLiteral("对象"));
    QMenu* arrange = mb->addMenu(QStringLiteral("排列"));
    arrange->addAction(QStringLiteral("自动拼版 / 嵌套"), QKeySequence(QStringLiteral("Ctrl+Shift+N")),
                       [this] { runAutoNest(); });
    QMenu* colour = mb->addMenu(QStringLiteral("色彩"));
    colour->addAction(QStringLiteral("打印环境 / 校准…"), [this] { openEnvManager(); });
    colour->addAction(QStringLiteral("专色手工调整（蜂巢）…"), [this] {
        auto* dlg = new VisualSpotTunerDialog(this);
        connect(dlg, &VisualSpotTunerDialog::recipeAccepted, this,
                [this](const QString& name, const jcolour::ui::SpotRecipe& r) {
            statusBar()->showMessage(
                QStringLiteral("专色「%1」已采用：C%2 M%3 Y%4 K%5")
                    .arg(name)
                    .arg(r.c, 0, 'f', 1).arg(r.m, 0, 'f', 1)
                    .arg(r.y, 0, 'f', 1).arg(r.k, 0, 'f', 1), 8000);
        });
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    colour->addAction(QStringLiteral("墨量上限 TIL 测定…"), [this] {
        const QString paper = paperCombo_ ? paperCombo_->currentText()
                                          : QStringLiteral("Photo Rag");
        auto* dlg = new TILWizardDialog(paper, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    colour->addAction(QStringLiteral("专色库…"), [this] {
        auto* dlg = new SpotLibraryDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    QMenu* tools = mb->addMenu(QStringLiteral("工具"));
    tools->addAction(QStringLiteral("作业核算（用量/成本）…"), [this] { openJobCostDialog(); });
    tools->addAction(QStringLiteral("米尺走纸校正…"), [this] { openEnvManager(); });
    tools->addAction(QStringLiteral("检查线性…"), [this] { openEnvManager(); });
    mb->addMenu(QStringLiteral("帮助"));
}

// ---- toolbar ----------------------------------------------------------------

QWidget* LayoutWindow::buildToolbar() {
    auto* bar = new QWidget;
    bar->setFixedHeight(34);
    bar->setStyleSheet("background:#EAE7DE;border-bottom:1px solid #DDD9CF;");
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
        l->setStyleSheet("color:#7A776D;font-size:10px;margin:0 2px;");
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
    envBtn->setStyleSheet("color:#0A6E8C;background:#E7F7FD;border:1px solid #bcd4f2;"
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
    print->setStyleSheet("background:#100A36;color:#FAFAF7;border:none;border-radius:4px;"
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
        "QStackedWidget{background:#EAE7DE;border-bottom:1px solid #DDD9CF;}"
        "QDoubleSpinBox,QComboBox{font-size:10px;}"
        "QLabel{color:#7A776D;font-size:10px;}");

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
        auto* link = iconBtn(ic::Icon::LinkIc, QStringLiteral("等比锁定"));
        link->setCheckable(true); link->setChecked(true);
        h->addWidget(link);
        h->addWidget(vsep());
        auto* rot = new QDoubleSpinBox;
        rot->setRange(-360, 360); rot->setDecimals(0); rot->setSuffix("°");
        rot->setFixedWidth(56);
        h->addWidget(rot);
        h->addWidget(iconBtn(ic::Icon::RotateCw, QStringLiteral("顺时针 90°")));
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
        aa->setStyleSheet("font-size:10px;color:#7A776D;");
        h->addWidget(aa);
        h->addStretch();
        warnChip_ = new QPushButton(QStringLiteral("⚠ 2 警告"));
        warnChip_->setToolTip(QStringLiteral("点击查看预检清单"));
        warnChip_->setStyleSheet("background:#FDF6D9;border:1px solid #E4C60F;color:#6B5A00;"
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
        tate->setStyleSheet("font-size:10px;color:#7A776D;");
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
    box->setStyleSheet("background:#F1EFE8;border-right:1px solid #DDD9CF;"
                       "QToolButton{font-size:13px;}");
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(4, 6, 4, 6);
    v->setSpacing(2);
    // 工具栏分组对标 Mirage / ImagePrint / Caldera 排版编辑器：
    //   工具（互斥）→ 块操作 → 拼版 → 显示开关 → 视图缩放
    const auto sep = [&v] {
        auto* s = new QFrame;
        s->setFixedSize(18, 1);
        s->setStyleSheet("background:#DDD9CF;border:none;");
        v->addSpacing(3);
        v->addWidget(s, 0, Qt::AlignHCenter);
        v->addSpacing(3);
    };
    // InDesign 式工具提示：加粗名称 (快捷键) + 灰色说明行；悬停即显
    const auto rich = [](const QString& name, const QString& key, const QString& desc) {
        return QStringLiteral("<b>%1</b>%2<br/><span style='color:#8A8779'>%3</span>")
            .arg(name, key.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(key), desc);
    };
    const auto add = [&](ic::Icon glyph, const QString& tip, bool exclusiveTool,
                         std::function<void()> fn) {
        auto* b = iconBtn(glyph, tip);
        b->setFixedSize(30, 28);
        b->setIconSize(QSize(20, 20));
        if (exclusiveTool) { b->setCheckable(true); b->setAutoExclusive(true); }
        if (fn) connect(b, &QToolButton::clicked, this, std::move(fn));
        v->addWidget(b);
        return b;
    };
    // 点选工具 → 紧挨工具条弹出工具名小标签（1.5s 自动隐藏），状态栏同步用法提示
    auto* toolHud = new QLabel(canvas_);
    toolHud->setStyleSheet(
        "QLabel{background:rgba(16,10,54,235);color:#fff;border-radius:6px;"
        "padding:5px 12px;font-size:12px;font-weight:600;}");
    toolHud->hide();
    auto* hudTimer = new QTimer(this);
    hudTimer->setSingleShot(true);
    connect(hudTimer, &QTimer::timeout, toolHud, &QLabel::hide);
    const auto announce = [this, toolHud, hudTimer](const QString& name, const QString& hint) {
        toolHud->setText(name);
        toolHud->adjustSize();
        toolHud->move(26, 26);            // 画布左上角，紧挨工具条
        toolHud->show();
        toolHud->raise();
        hudTimer->start(1500);
        statusBar()->showMessage(hint.isEmpty() ? name : name + QStringLiteral("　—　") + hint, 5000);
    };

    // ── 工具（互斥选中；V/T/H/K 单键切换与按钮双向同步）──
    selectToolBtn_ = add(ic::Icon::Cursor,
        rich(QStringLiteral("选择工具"), "V", QStringLiteral("移动、缩放块；双击文字可编辑")), true,
        [this] { canvas_->setTool(CanvasTool::Select); });
    selectToolBtn_->setChecked(true);
    auto* textBtn = add(ic::Icon::Type,
        rich(QStringLiteral("文字工具"), "T", QStringLiteral("拖出文本框，松手直接输入")), true,
        [this] { canvas_->setTool(CanvasTool::Text); });
    auto* qrBtn = add(ic::Icon::QrCode,
        rich(QStringLiteral("二维码工具"), {}, QStringLiteral("在纸张上点击放置二维码 / 条码")), true,
        [this] { canvas_->setTool(CanvasTool::Qr); });
    auto* handBtn = add(ic::Icon::Hand,
        rich(QStringLiteral("抓手工具"), "H", QStringLiteral("拖动画布；任何工具下按住空格临时切换")), true,
        [this] { canvas_->setTool(CanvasTool::Hand); });
    auto* measBtn = add(ic::Icon::Measure,
        rich(QStringLiteral("测量工具"), "K", QStringLiteral("拖线读距离；Shift 锁 45°，Esc 清除")), true,
        [this] { canvas_->setTool(CanvasTool::Measure); });
    {
        // 按钮点击与 V/T/H/K 键盘切换都经 toolChanged：同步选中态 + 状态栏播报工具名
        const std::array<QToolButton*, 5> tb{selectToolBtn_, textBtn, qrBtn, handBtn, measBtn};
        connect(canvas_, &LayoutCanvas::toolChanged, this, [this, tb, announce](CanvasTool t) {
            const int i = t == CanvasTool::Select ? 0 : t == CanvasTool::Text ? 1
                        : t == CanvasTool::Qr ? 2 : t == CanvasTool::Hand ? 3 : 4;
            tb[size_t(i)]->setChecked(true);
            static const char* names[] = {"选择工具 (V)", "文字工具 (T)", "二维码工具",
                                          "抓手工具 (H)", "测量工具 (K)"};
            static const char* hints[] = {"", "在纸上拖出文本框，松手直接输入",
                                          "在纸张上点击放置", "拖动平移画布",
                                          "拖动读取 mm 距离与 Δx/Δy"};
            announce(QString::fromUtf8(names[i]), QString::fromUtf8(hints[i]));
        });
    }

    // ── 块操作 ──
    sep();
    add(ic::Icon::Copy, rich(QStringLiteral("复制"), QStringLiteral("Cmd+D"),
        QStringLiteral("复制选中块，+12mm 错位")), false, [this] { canvas_->duplicateSelected(); });
    add(ic::Icon::Copies, rich(QStringLiteral("份数…"), {},
        QStringLiteral("同图打 N 份并自动排版（ImagePrint 方式）")), false, [this] {
        bool ok = false;
        const int n = QInputDialog::getInt(this, QStringLiteral("份数"),
            QStringLiteral("再复制多少份？（复制后自动拼版归位）"), 3, 1, 99, 1, &ok);
        if (!ok) return;
        canvas_->duplicateSelectedN(n);
        runAutoNest(); });                    // ImagePrint：设份数即自动排列
    add(ic::Icon::RotateCw, rich(QStringLiteral("旋转 90°"), {},
        QStringLiteral("顺时针旋转，省纸拼版常用")), false, [this] { canvas_->rotateSelected90(); });
    add(ic::Icon::Trash, rich(QStringLiteral("删除"), QStringLiteral("Delete"),
        QStringLiteral("删除选中块")), false, [this] { canvas_->deleteSelected(); });
    {
        // InDesign 对齐面板：对齐到(选区/页面) · 对齐对象 · 分布对象 · 分布间距
        auto* b = add(ic::Icon::AlignLeftIc, rich(QStringLiteral("对齐与分布"), {},
            QStringLiteral("InDesign 对齐面板：对齐到选区/页面、分布对象、等间距")), false, nullptr);
        auto* menu = new QMenu(b);
        using A = LayoutCanvas::Align;

        auto* secTo = menu->addSection(QStringLiteral("对齐到"));
        Q_UNUSED(secTo);
        auto* toGrp = new QActionGroup(menu);
        auto* toSel = menu->addAction(QStringLiteral("选区（需选中 ≥2 个块）"));
        auto* toPage = menu->addAction(QStringLiteral("页面"));
        for (auto* a : {toSel, toPage}) { a->setCheckable(true); toGrp->addAction(a); }
        toSel->setChecked(true);
        connect(toSel, &QAction::triggered, this,
                [this] { canvas_->setAlignTo(LayoutCanvas::AlignTo::Selection); });
        connect(toPage, &QAction::triggered, this,
                [this] { canvas_->setAlignTo(LayoutCanvas::AlignTo::Page); });

        menu->addSection(QStringLiteral("对齐对象"));
        const std::pair<QString, A> items[] = {
            {QStringLiteral("左对齐"), A::Left},   {QStringLiteral("水平居中"), A::HCenter},
            {QStringLiteral("右对齐"), A::Right},  {QStringLiteral("顶对齐"), A::Top},
            {QStringLiteral("垂直居中"), A::VCenter}, {QStringLiteral("底对齐"), A::Bottom},
        };
        for (const auto& [txt, mode] : items)
            menu->addAction(txt, this, [this, mode] { canvas_->alignSelected(mode); });

        menu->addSection(QStringLiteral("分布对象（需 ≥3 个块）"));
        menu->addAction(QStringLiteral("水平居中分布"), this,
                        [this] { canvas_->distributeSelected(true, true); });
        menu->addAction(QStringLiteral("垂直居中分布"), this,
                        [this] { canvas_->distributeSelected(false, true); });
        menu->addSection(QStringLiteral("分布间距"));
        menu->addAction(QStringLiteral("水平等间距"), this,
                        [this] { canvas_->distributeSelected(true, false); });
        menu->addAction(QStringLiteral("垂直等间距"), this,
                        [this] { canvas_->distributeSelected(false, false); });

        b->setMenu(menu);
        b->setPopupMode(QToolButton::InstantPopup);
    }

    // ── 拼版 ──
    sep();
    add(ic::Icon::Grid, rich(QStringLiteral("自动拼版"), {},
        QStringLiteral("嵌套排列全部块，省纸")), false, [this] { runAutoNest(); });
    add(ic::Icon::StepRepeat, rich(QStringLiteral("步进重复…"), {},
        QStringLiteral("InDesign「多重复制」：选中块铺 行×列")), false, [this] {
        if (canvas_->selected() < 0) {
            statusBar()->showMessage(QStringLiteral("步进重复：请先选中一个块"), 4000);
            return;
        }
        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("步进重复（多重复制）"));
        auto* form = new QFormLayout(&dlg);
        auto* rows = new QSpinBox; rows->setRange(1, 30); rows->setValue(2);
        auto* cols = new QSpinBox; cols->setRange(1, 30); cols->setValue(3);
        auto* gx = new QDoubleSpinBox; gx->setRange(0, 200); gx->setValue(5); gx->setSuffix(" mm");
        auto* gy = new QDoubleSpinBox; gy->setRange(0, 200); gy->setValue(5); gy->setSuffix(" mm");
        form->addRow(QStringLiteral("行数"), rows);
        form->addRow(QStringLiteral("列数"), cols);
        form->addRow(QStringLiteral("水平间距"), gx);
        form->addRow(QStringLiteral("垂直间距"), gy);
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        form->addRow(bb);
        if (dlg.exec() == QDialog::Accepted)
            canvas_->stepRepeatGrid(rows->value(), cols->value(), gx->value(), gy->value()); });
    add(ic::Icon::TilingIc, rich(QStringLiteral("分幅 Tiling…"), {},
        QStringLiteral("超幅面拼接：选中块拆行×列幅，带重叠搭接（ErgoSoft 方式）")), false, [this] {
        if (canvas_->selected() < 0) {
            statusBar()->showMessage(QStringLiteral("分幅：请先选中一个块"), 4000);
            return;
        }
        auto& blk = canvas_->blocks()[canvas_->selected()];
        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("分幅 Tiling"));
        auto* form = new QFormLayout(&dlg);
        auto* cols = new QSpinBox; cols->setRange(1, 8); cols->setValue(qMax(1, blk.tileCols));
        auto* rows = new QSpinBox; rows->setRange(1, 8); rows->setValue(qMax(1, blk.tileRows));
        auto* ov = new QDoubleSpinBox; ov->setRange(0, 50); ov->setSuffix(" mm");
        ov->setValue(blk.tiled() ? blk.tileOverlapMm : 10.0);
        auto* alt = new QCheckBox(QStringLiteral("交替旋转 180°（补偿喷头方向色差）"));
        alt->setChecked(blk.tileAlternate);
        auto* sizeLbl = new QLabel;
        sizeLbl->setStyleSheet("color:#7A776D;");
        auto refresh = [&, sizeLbl] {
            const double tw = blk.w / cols->value() + (cols->value() > 1 ? ov->value() : 0);
            const double th = blk.h / rows->value() + (rows->value() > 1 ? ov->value() : 0);
            sizeLbl->setText(QStringLiteral("每幅约 %1 × %2 mm（含重叠）")
                                 .arg(tw, 0, 'f', 0).arg(th, 0, 'f', 0));
        };
        QObject::connect(cols, &QSpinBox::valueChanged, &dlg, refresh);
        QObject::connect(rows, &QSpinBox::valueChanged, &dlg, refresh);
        QObject::connect(ov, &QDoubleSpinBox::valueChanged, &dlg, refresh);
        refresh();
        form->addRow(QStringLiteral("列数"), cols);
        form->addRow(QStringLiteral("行数"), rows);
        form->addRow(QStringLiteral("重叠搭接"), ov);
        form->addRow(alt);
        form->addRow(sizeLbl);
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        auto* clear = bb->addButton(QStringLiteral("取消分幅"), QDialogButtonBox::ResetRole);
        connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        connect(clear, &QPushButton::clicked, &dlg, [&] {
            blk.tileCols = blk.tileRows = 1; blk.tileOverlapMm = 0; blk.tileAlternate = false;
            dlg.reject();
            canvas_->update();
        });
        form->addRow(bb);
        if (dlg.exec() == QDialog::Accepted) {
            blk.tileCols = cols->value();
            blk.tileRows = rows->value();
            blk.tileOverlapMm = ov->value();
            blk.tileAlternate = alt->isChecked();
            canvas_->update();
            statusBar()->showMessage(QStringLiteral("已分幅 %1×%2 · 重叠 %3mm — 打印整版时逐幅生成作业")
                .arg(cols->value()).arg(rows->value()).arg(ov->value()), 6000);
        } });
    add(ic::Icon::ContactSheet, rich(QStringLiteral("联系表…"), {},
        QStringLiteral("多图缩略排版 Contact Sheet")), false, [this] {
        auto* w = new ContactSheetDialog(nullptr);
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->setWindowTitle(QStringLiteral("联系表"));
        w->show(); });

    // ── 显示开关 ──
    sep();
    add(ic::Icon::CropMarks, rich(QStringLiteral("裁切角线"), {},
        QStringLiteral("开/关 每块四角的裁纸标记")), false, [this] { canvas_->toggleCropMarks(); });
    add(ic::Icon::Grid3, rich(QStringLiteral("网格"), {},
        QStringLiteral("开/关 纸面 50mm 网格")), false, [this] { canvas_->toggleGrid(); });
    add(ic::Icon::RulerIc, rich(QStringLiteral("标尺"), QStringLiteral("Cmd+R"),
        QStringLiteral("开/关 标尺；从标尺可拖出参考线")), false, [this] { canvas_->toggleRulers(); });

    // ── 视图缩放 ──
    sep();
    add(ic::Icon::ZoomIn,  rich(QStringLiteral("放大"), QStringLiteral("Cmd+= / Cmd+滚轮"),
        QStringLiteral("锚定光标缩放")), false, [this] { canvas_->zoomBy(+10); });
    add(ic::Icon::ZoomOut, rich(QStringLiteral("缩小"), QStringLiteral("Cmd+-"),
        {}), false, [this] { canvas_->zoomBy(-10); });
    add(ic::Icon::FitView, rich(QStringLiteral("适合窗口"), QStringLiteral("Cmd+0"),
        {}), false, [this] { canvas_->zoomFit(); });
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
        "QTabWidget::pane{border:none;border-left:1px solid #DDD9CF;background:#FAFAF7;}"
        "QTabBar::tab{font-size:10px;padding:5px 10px;}"
        "QLabel{font-size:10px;}");

    auto hdLbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet("font-size:9px;font-weight:600;color:#8A8779;background:#F1EFE8;"
                         "padding:4px 8px;border-bottom:1px solid #EAE7DE;");
        return l;
    };
    auto kvRow = [](const QString& k, QLabel*& vOut, bool warn = false) {
        auto* w = new QWidget;
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(10, 2, 10, 2);
        auto* kl = new QLabel(k);
        kl->setStyleSheet("color:#7A776D;");
        vOut = new QLabel;
        vOut->setStyleSheet(warn ? "color:#B58900;font-weight:600;" : "color:#100A36;font-weight:600;");
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
                "QPushButton:checked{background:#E7F7FD;border-color:#0BBAEE;color:#0A6E8C;font-weight:600;}")
                .arg(stale ? "#c0392b" : "#DDD9CF"));
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
        modeHint_->setStyleSheet("color:#0A6E8C;background:#E7F7FD;border:1px solid #A9E4F7;"
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
            l->setStyleSheet("color:#7A776D;");
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
            l->setStyleSheet("color:#7A776D;");
            h->addWidget(l);
            h->addStretch();
            for (int i = 0; i < opts.size(); ++i) {
                auto* b = new QPushButton(opts[i]);
                b->setCheckable(true);
                b->setAutoExclusive(true);
                b->setChecked(i == on);
                b->setFixedHeight(18);
                b->setStyleSheet("QPushButton{font-size:9px;padding:0 7px;border:1px solid #c4c9d0;background:#fff;}"
                                 "QPushButton:checked{background:#E7F7FD;color:#0A6E8C;font-weight:600;}");
                h->addWidget(b);
            }
            return w;
        };
        auto hint = [](const QString& t) {
            auto* l = new QLabel(t);
            l->setWordWrap(true);
            l->setStyleSheet("color:#0A6E8C;background:#E7F7FD;border:1px solid #A9E4F7;"
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
            il->setStyleSheet("color:#7A776D;");
            auto* ic = new QComboBox;
            ic->addItem(QStringLiteral("PhotoRag_P9500_RGB.icc"));
            ih->addWidget(il);
            ih->addWidget(ic, 1);
            auto* intRow = new QWidget;
            auto* nh = new QHBoxLayout(intRow);
            nh->setContentsMargins(10, 1, 10, 1);
            auto* nl = new QLabel(QStringLiteral("渲染意图"));
            nl->setFixedWidth(52);
            nl->setStyleSheet("color:#7A776D;");
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
                l->setStyleSheet("color:#7A776D;");
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
                     QStringLiteral("RGB 需检查"), "#6B5A00"},
                    {QStringLiteral("P9500 · Canson Baryta 310"),
                     QStringLiteral("总墨 280% · 全部模式"),
                     QStringLiteral("过期 18天"), "#6B5A00"},
                    {QStringLiteral("P9500 · Legacy Fibre"),
                     QStringLiteral("墨批 #A6 旧批"),
                     QStringLiteral("墨批已变"), "#9a2418"}};
        for (const auto& r : rows) {
            auto* b = new QPushButton;
            b->setFixedHeight(44);
            b->setStyleSheet("QPushButton{background:#fff;border:none;border-bottom:1px solid #EAE7DE;text-align:left;}"
                             "QPushButton:hover{background:#F1EFE8;}");
            auto* bl = new QHBoxLayout(b);
            bl->setContentsMargins(10, 4, 10, 4);
            auto* tcol = new QVBoxLayout;
            tcol->setSpacing(1);
            auto* nm = new QLabel(r.name);
            nm->setStyleSheet("font-weight:600;color:#100A36;");
            auto* sb = new QLabel(r.sub);
            sb->setStyleSheet("color:#8A8779;font-size:8px;");
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
        pfBanner_->setStyleSheet("background:#FDF6D9;border:1px solid #E4C60F;border-radius:4px;"
                                 "color:#6B5A00;font-size:9px;padding:4px 7px;margin:4px 10px;");
        v->addWidget(pfBanner_);
        auto pfRow = [&](const QString& t, bool ok) {
            auto* l = new QLabel((ok ? QStringLiteral("✓ ") : QStringLiteral("⚠ ")) + t);
            l->setStyleSheet(QStringLiteral("color:%1;font-size:10px;margin:0 12px;")
                                 .arg(ok ? "#2F9E6B" : "#B58900"));
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
        fix->setStyleSheet("background:#100A36;color:#FAFAF7;border:none;border-radius:4px;padding:3px 10px;");
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
    w->setStyleSheet("background:#fff;border:1px solid #DDD9CF;border-radius:5px;");
    struct D { const char* c; const char* tip; };
    const D dots[] = {{"#ff5f57", "离线"}, {"#febc2e", "手动开始"}, {"#28c840", "连线（自动）"}};
    auto* lbl = new QLabel(QStringLiteral("手动开始"));
    lbl->setObjectName("tlLbl");
    lbl->setStyleSheet("font-size:8px;color:#7A776D;border:none;");
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
            "QToolButton:checked{border:2px solid #100A36;}").arg(d.c));
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
    bar->setStyleSheet("background:#F1EFE8;border-top:1px solid #DDD9CF;");
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
            "QToolButton:checked{border:2px solid #0BBAEE;}").arg(thumbs[i].colour));
        connect(b, &QToolButton::clicked, this, [this, i] { canvas_->select(i); });
        connect(canvas_, &LayoutCanvas::selectionChanged, b,
                [b, i](int s) { b->setChecked(s == i); });
        h->addWidget(b);
    }
    auto* add = new QToolButton;
    add->setFixedSize(42, 50);
    add->setText(QStringLiteral("＋\n导入"));
    add->setStyleSheet("QToolButton{border:1px dashed #b9c2cc;border-radius:4px;color:#0BBAEE;font-size:8px;}");
    h->addWidget(add);
    h->addStretch();
    auto* stat = new QLabel(QStringLiteral("5 张 · Photo Rag　|　利用率 <b style='color:#2F9E6B'>81%</b>"));
    stat->setStyleSheet("font-size:9px;color:#7A776D;");
    h->addWidget(stat);
    h->addWidget(buildTrafficLight());
    auto* queueN = new QLabel(QStringLiteral("队列 4"));
    queueN->setStyleSheet("font-size:9px;color:#7A776D;");
    h->addWidget(queueN);
    auto* print = new QPushButton(QStringLiteral("🖶 打印整版"));
    print->setToolTip(QStringLiteral("打印整版（5 张 · 当前纸张）"));
    print->setStyleSheet("background:#100A36;color:#FAFAF7;border:none;border-radius:4px;"
                         "padding:6px 16px;font-weight:600;");
    connect(print, &QPushButton::clicked, this, [this] { submitLayoutToQueue(); });
    h->addWidget(print);
    return bar;
}

// ---- status bar -----------------------------------------------------------------

void LayoutWindow::buildStatusBar() {
    auto* sb = statusBar();
    sb->setStyleSheet("QStatusBar{background:#F1EFE8;border-top:1px solid #DDD9CF;font-size:9px;color:#7A776D;}"
                      "QLabel{font-size:9px;color:#7A776D;}");
    auto mk = [&](const QString& t, const QString& dot) {
        auto* l = new QLabel(QStringLiteral("<span style='color:%1'>●</span> %2").arg(dot, t));
        sb->addWidget(l);
        return l;
    };
    mk(QStringLiteral("P9500 · 已连线"), "#2F9E6B");
    mk(QStringLiteral("软打样 · 启用"), "#2F9E6B");
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
        imgIcc_->setStyleSheet(info.warn ? "color:#B58900;font-weight:600;" : "color:#100A36;font-weight:600;");
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
        pfCal_->setStyleSheet("color:#B58900;font-size:10px;");
    } else {
        pfCal_->setText(QStringLiteral("✓ 校准有效 · %1 天").arg(m->days));
        pfCal_->setStyleSheet("color:#2F9E6B;font-size:10px;");
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
    else if (action == QLatin1String("origin"))     // 演示零位偏移（截图验证）
        canvas_->setRulerOrigin(150, 100);
    else if (action == QLatin1String("tiling")) {   // 演示分幅（截图验证）
        for (auto& b : canvas_->blocks())
            if (b.name == QLatin1String("seascape_05")) {
                b.tileCols = 2; b.tileRows = 2; b.tileOverlapMm = 12; b.tileAlternate = true;
            }
        canvas_->update();
    }
    else if (action == QLatin1String("cost"))
        QTimer::singleShot(0, this, [this] { openJobCostDialog(); });   // 模态对话框延迟弹出，让截图定时器先排上
    else if (action == QLatin1String("til")) {
        auto* dlg = new TILWizardDialog(QStringLiteral("Photo Rag"), this);
        dlg->show();
    }
    else if (action == QLatin1String("spottuner")) {
        auto* dlg = new VisualSpotTunerDialog(this);
        dlg->show();
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
        // 分幅块 → 逐幅生成作业（ErgoSoft "print every tile on a separate
        // page"）；幅名带 [幅号/总幅 行-列]，交替旋转幅标注 ↻180°。
        const int tiles = qMax(1, b.tileCols) * qMax(1, b.tileRows);
        for (int t = 0; t < tiles; ++t) {
            jcolour::queue::PrintJob job;
            QString tileTag;
            if (b.tiled()) {
                const int rr = t / b.tileCols, cc = t % b.tileCols;
                tileTag = QStringLiteral(" [幅%1/%2 %3-%4%5]")
                    .arg(t + 1).arg(tiles).arg(rr + 1).arg(cc + 1)
                    .arg(b.tileAlternate && (t % 2 == 1) ? QStringLiteral(" ↻180°")
                                                         : QString());
            }
            job.id          = b.name + tileTag + "-" +
                              QDateTime::currentDateTime().toString("hhmmsszzz");
            job.imagePath   = b.name + ".tif";
            job.presetName  = printer + QStringLiteral(" · ") + paper + QStringLiteral(" · ") +
                              QString::fromUtf8(m->name) + QStringLiteral("（") +
                              QString::fromUtf8(m->lin) + QStringLiteral("）") + tileTag;
            job.copies      = 1;
            job.status      = QStringLiteral("pending");
            job.submittedAt = QDateTime::currentDateTime();
            job.blackInk    = (b.modeKey == QLatin1String("rgb") || b.modeKey == QLatin1String("rgb16"))
                                  ? jcolour::queue::BlackInk::Either
                                  : jcolour::queue::BlackInk::PK;
            queueWidget_->addJob(job);
            ++submitted;
        }
    }
    queueWidget_->startProcessing();
    if (rightTabs_) rightTabs_->setCurrentIndex(3);   // 跳到队列
    statusBar()->showMessage(
        QStringLiteral("已送 %1 个工作到 RIP 队列（%2 · %3）").arg(submitted).arg(printer, paper), 5000);
}

void LayoutWindow::openJobCostDialog() {
    // 作业核算（ErgoSoft QueueManager & CostManager 的估算版）：
    // 介质长度 = 版面内容最低点 + 收纸边；墨耗 = Σ 块面积 × 模式基准 ×
    // 平均覆盖率。全部为估算值 —— 正式版由 RIP 光栅逐像素统计替换。
    QSettings st;
    const double defMedia = st.value("cost/mediaPerSqm", 180.0).toDouble();
    const double defInk   = st.value("cost/inkPerMl", 1.5).toDouble();
    const double defMlSqm = st.value("cost/inkMlPerSqm", 12.0).toDouble();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("作业核算 · 用量与成本估算"));
    dlg.resize(660, 480);
    auto* v = new QVBoxLayout(&dlg);

    // 单价参数（记住设置）
    auto* priceRow = new QHBoxLayout;
    auto mkPrice = [](double v, double max, const char* suffix) {
        auto* s = new QDoubleSpinBox;
        s->setRange(0, max); s->setValue(v); s->setSuffix(QString::fromUtf8(suffix));
        s->setFixedWidth(120);
        return s;
    };
    auto* mediaPrice = mkPrice(defMedia, 9999, " 元/m²");
    auto* inkPrice   = mkPrice(defInk, 99, " 元/ml");
    auto* mlPerSqm   = mkPrice(defMlSqm, 60, " ml/m²");
    priceRow->addWidget(new QLabel(QStringLiteral("介质单价")));
    priceRow->addWidget(mediaPrice);
    priceRow->addWidget(new QLabel(QStringLiteral("墨水单价")));
    priceRow->addWidget(inkPrice);
    priceRow->addWidget(new QLabel(QStringLiteral("彩色墨耗基准")));
    priceRow->addWidget(mlPerSqm);
    priceRow->addStretch();
    v->addLayout(priceRow);

    auto* table = new QTableWidget;
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels(
        {QStringLiteral("块"), QStringLiteral("模式"), QStringLiteral("面积 m²"),
         QStringLiteral("墨耗 ml"), QStringLiteral("幅数")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    v->addWidget(table, 1);

    auto* totals = new QLabel;
    totals->setStyleSheet(
        "background:#E7F7FD;border:1px solid #A9E4F7;border-radius:6px;"
        "padding:9px 12px;font-size:12.5px;color:#100A36;");
    totals->setWordWrap(true);
    v->addWidget(totals);
    auto* note = new QLabel(QStringLiteral(
        "估算口径：覆盖率按 75% 计；单黑/黑+灰按彩色基准的 50%/70%；"
        "介质长度 = 内容最低点 + 20mm 收纸边 × 整卷宽计价。正式版由 RIP 光栅统计替换。"));
    note->setStyleSheet("font-size:10.5px;color:#8A8779;");
    note->setWordWrap(true);
    v->addWidget(note);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close);
    bb->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(bb->button(QDialogButtonBox::Close), &QPushButton::clicked, &dlg, &QDialog::accept);
    v->addWidget(bb);

    auto recompute = [&, table, totals] {
        const double mlColor = mlPerSqm->value();
        double sumArea = 0, sumInk = 0, maxBottom = 0;
        const auto& blocks = canvas_->blocks();
        int rows = 0;
        for (const auto& b : blocks) if (!b.isText) ++rows;
        table->setRowCount(rows);
        int r = 0;
        for (const auto& b : blocks) {
            if (b.isText) continue;
            const ModeDef* m = modeDef(b.modeKey);
            const int tiles = qMax(1, b.tileCols) * qMax(1, b.tileRows);
            // 分幅含重叠：每幅尺寸 = 块/幅数 + 重叠
            double area = (b.w / 1000.0) * (b.h / 1000.0);
            if (b.tiled()) {
                const double tw = b.w / b.tileCols + (b.tileCols > 1 ? b.tileOverlapMm : 0);
                const double th = b.h / b.tileRows + (b.tileRows > 1 ? b.tileOverlapMm : 0);
                area = tiles * (tw / 1000.0) * (th / 1000.0);
            }
            const double factor = (b.modeKey == QLatin1String("mono")) ? 0.5
                                : (b.modeKey == QLatin1String("gray")) ? 0.7 : 1.0;
            const double ink = area * mlColor * 0.75 * factor;
            sumArea += area;
            sumInk += ink;
            maxBottom = qMax(maxBottom, b.y + b.h);
            table->setItem(r, 0, new QTableWidgetItem(b.name));
            table->setItem(r, 1, new QTableWidgetItem(QString::fromUtf8(m->name)));
            table->setItem(r, 2, new QTableWidgetItem(QString::number(area, 'f', 3)));
            table->setItem(r, 3, new QTableWidgetItem(QString::number(ink, 'f', 1)));
            table->setItem(r, 4, new QTableWidgetItem(
                b.tiled() ? QStringLiteral("%1×%2").arg(b.tileCols).arg(b.tileRows)
                          : QStringLiteral("—")));
            ++r;
        }
        const double lengthM = (maxBottom + 20.0) / 1000.0;
        const double mediaSqm = lengthM * (kPaperW / 1000.0);
        const double mediaCost = mediaSqm * mediaPrice->value();
        const double inkCost = sumInk * inkPrice->value();
        totals->setText(QStringLiteral(
            "<b>介质</b>　长度 %1 m · 计价面积 %2 m² · %3 元　　"
            "<b>墨水</b>　≈%4 ml · %5 元　　<b>合计 ≈ %6 元</b>")
            .arg(lengthM, 0, 'f', 2).arg(mediaSqm, 0, 'f', 2)
            .arg(mediaCost, 0, 'f', 0).arg(sumInk, 0, 'f', 1)
            .arg(inkCost, 0, 'f', 0).arg(mediaCost + inkCost, 0, 'f', 0));
        table->resizeColumnsToContents();
    };
    for (auto* s : {mediaPrice, inkPrice, mlPerSqm})
        connect(s, &QDoubleSpinBox::valueChanged, &dlg, recompute);
    recompute();
    dlg.exec();

    st.setValue("cost/mediaPerSqm", mediaPrice->value());
    st.setValue("cost/inkPerMl", inkPrice->value());
    st.setValue("cost/inkMlPerSqm", mlPerSqm->value());
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
