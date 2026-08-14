#include "spot_tuner_dialog.h"

#include "../../../engine/src/i1pro3_driver.h"
#include "../../../engine/src/spectral.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <QPointer>
#include <QDateTime>
#include <QHeaderView>
#include <QTableWidget>
#include <QRandomGenerator>
#include <cmath>
#include <thread>

namespace jcolour::ui {

namespace {

// 屏幕预览：朴素 CMYK→RGB（近似；正式评估靠打印目测）
QColor previewColor(const SpotRecipe& r) {
    auto cl = [](double v) { return qBound(0.0, v, 100.0); };
    const double kk = 1.0 - cl(r.k) / 100.0;
    return QColor::fromRgbF(float((1.0 - cl(r.c) / 100.0) * kk),
                            float((1.0 - cl(r.m) / 100.0) * kk),
                            float((1.0 - cl(r.y) / 100.0) * kk));
}

// 近似 Lab（屏幕参考值；印后实测才是真 Lab）
void previewLab(const SpotRecipe& r, double& L, double& a, double& b) {
    const QColor c = previewColor(r);
    auto lin = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    const double R = lin(c.redF()), G = lin(c.greenF()), B = lin(c.blueF());
    const double X = (R * 0.4124 + G * 0.3576 + B * 0.1805) / 0.9505;
    const double Y =  R * 0.2126 + G * 0.7152 + B * 0.0722;
    const double Z = (R * 0.0193 + G * 0.1192 + B * 0.9505) / 1.089;
    auto f = [](double t) { return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116; };
    L = 116 * f(Y) - 16; a = 500 * (f(X) - f(Y)); b = 200 * (f(Y) - f(Z));
}

// 六个方向通道选项（ORIS 默认前 6 项；含黑通道备选）
const char* kChannelNames[] = {"青 C +", "品 M +", "黄 Y +", "青 C −", "品 M −", "黄 Y −",
                               "黑 K +", "黑 K −"};

// 通道选项 idx → 配方增量（每 1% 步长）
SpotRecipe channelDelta(int idx) {
    SpotRecipe d;
    switch (idx) {
        case 0: d.c = +1; break; case 1: d.m = +1; break; case 2: d.y = +1; break;
        case 3: d.c = -1; break; case 4: d.m = -1; break; case 5: d.y = -1; break;
        case 6: d.k = +1; break; case 7: d.k = -1; break;
    }
    return d;
}

SpotRecipe operator+(SpotRecipe a, const SpotRecipe& b) {
    a.c += b.c; a.m += b.m; a.y += b.y; a.k += b.k; return a;
}
SpotRecipe operator*(SpotRecipe a, double s) {
    a.c *= s; a.m *= s; a.y *= s; a.k *= s; return a;
}

// 完整 CIEDE2000（专色判定的行业标准差值）
double deltaE2000(double L1, double a1, double b1, double L2, double a2, double b2) {
    const double rad = M_PI / 180, deg = 180 / M_PI;
    const double C1 = std::hypot(a1, b1), C2 = std::hypot(a2, b2), Cb = (C1 + C2) / 2;
    const double G = 0.5 * (1 - std::sqrt(std::pow(Cb, 7) / (std::pow(Cb, 7) + std::pow(25.0, 7))));
    const double ap1 = a1 * (1 + G), ap2 = a2 * (1 + G);
    const double Cp1 = std::hypot(ap1, b1), Cp2 = std::hypot(ap2, b2);
    auto hp = [&](double a, double b) {
        if (a == 0 && b == 0) return 0.0;
        double h = std::atan2(b, a) * deg;
        return h < 0 ? h + 360 : h;
    };
    const double hp1 = hp(ap1, b1), hp2 = hp(ap2, b2);
    const double dL = L2 - L1, dC = Cp2 - Cp1;
    double dh = hp2 - hp1;
    if (Cp1 * Cp2 == 0) dh = 0;
    else if (dh > 180) dh -= 360;
    else if (dh < -180) dh += 360;
    const double dH = 2 * std::sqrt(Cp1 * Cp2) * std::sin(dh / 2 * rad);
    const double Lb = (L1 + L2) / 2, Cpb = (Cp1 + Cp2) / 2;
    double hb = hp1 + hp2;
    if (Cp1 * Cp2 != 0) {
        if (std::abs(hp1 - hp2) > 180) hb += (hp1 + hp2 < 360) ? 360 : -360;
        hb /= 2;
    }
    const double T = 1 - 0.17 * std::cos((hb - 30) * rad) + 0.24 * std::cos(2 * hb * rad)
                   + 0.32 * std::cos((3 * hb + 6) * rad) - 0.20 * std::cos((4 * hb - 63) * rad);
    const double SL = 1 + 0.015 * std::pow(Lb - 50, 2) / std::sqrt(20 + std::pow(Lb - 50, 2));
    const double SC = 1 + 0.045 * Cpb;
    const double SH = 1 + 0.015 * Cpb * T;
    const double dTheta = 30 * std::exp(-std::pow((hb - 275) / 25, 2));
    const double RC = 2 * std::sqrt(std::pow(Cpb, 7) / (std::pow(Cpb, 7) + std::pow(25.0, 7)));
    const double RT = -RC * std::sin(2 * dTheta * rad);
    return std::sqrt(std::pow(dL / SL, 2) + std::pow(dC / SC, 2) + std::pow(dH / SH, 2)
                   + RT * (dC / SC) * (dH / SH));
}

}  // namespace

// ─── 蜂巢控件 ───────────────────────────────────────────────────────

struct HexPatch {
    SpotRecipe recipe;
    double px = 0, py = 0;        // 逻辑坐标（未缩放）
    int ring = 0;
    int row = 0, col = 0;         // 行字母 / 列数字 标号
    bool clipped = false;
};

class SpotHexChart : public QWidget {
    Q_OBJECT
public:
    explicit SpotHexChart(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(520, 480);
    }

    QVector<HexPatch> patches;
    int selected = 0;             // index into patches；0 = 中心
    bool showClipped = true;
    bool outlines = false;

    // 供打印复用：把整张蜂巢画进任意画布
    void render(QPainter& g, const QRectF& target) const {
        if (patches.isEmpty()) return;
        // 逻辑范围 → target 等比缩放
        double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
        for (const auto& p : patches) {
            minX = std::min(minX, p.px); maxX = std::max(maxX, p.px);
            minY = std::min(minY, p.py); maxY = std::max(maxY, p.py);
        }
        const double margin = 1.6;                 // 六边形半径余量 + 标号空间
        minX -= margin; maxX += margin; minY -= margin; maxY += margin;
        const double s = std::min(target.width() / (maxX - minX),
                                  target.height() / (maxY - minY));
        const double ox = target.x() - minX * s + (target.width() - (maxX - minX) * s) / 2;
        const double oy = target.y() - minY * s + (target.height() - (maxY - minY) * s) / 2;
        auto X = [&](double v) { return ox + v * s; };
        auto Y = [&](double v) { return oy + v * s; };
        const double hr = 0.985 * s;               // 六边形外接半径（留缝）

        auto hexPath = [&](double cx, double cy) {
            QPainterPath path;
            for (int i = 0; i < 6; ++i) {
                const double ang = (60.0 * i - 30.0) * M_PI / 180.0;  // pointy-top
                const QPointF pt(cx + hr * std::cos(ang), cy + hr * std::sin(ang));
                if (i == 0) path.moveTo(pt); else path.lineTo(pt);
            }
            path.closeSubpath();
            return path;
        };

        g.setRenderHint(QPainter::Antialiasing);
        // 行字母 / 列数字（用于打印后纸上定位）
        QFont lf = g.font(); lf.setPointSizeF(std::max(7.0, s * 0.28)); g.setFont(lf);
        g.setPen(QColor("#100A36"));
        QSet<int> doneRow, doneCol;
        for (const auto& p : patches) {
            if (!showClipped && p.clipped) continue;
            if (!doneRow.contains(p.row)) {
                // 该行最左块的左侧放字母
                double leftMost = 1e9;
                for (const auto& q : patches)
                    if (q.row == p.row && (showClipped || !q.clipped))
                        leftMost = std::min(leftMost, q.px);
                if (leftMost < 1e8)
                    g.drawText(QRectF(X(leftMost) - 2.2 * s, Y(p.py) - s / 2, 1.4 * s, s),
                               Qt::AlignRight | Qt::AlignVCenter,
                               QString(QChar('A' + p.row)));
                doneRow.insert(p.row);
            }
            if (p.ring > 0 && !doneCol.contains(p.col)) {
                double topMost = 1e9; double colX = 0;
                for (const auto& q : patches)
                    if (q.col == p.col && (showClipped || !q.clipped))
                        if (q.py < topMost) { topMost = q.py; colX = q.px; }
                if (topMost < 1e8)
                    g.drawText(QRectF(X(colX) - s, Y(topMost) - 2.1 * s, 2 * s, s),
                               Qt::AlignHCenter | Qt::AlignBottom,
                               QString::number(p.col + 1));
                doneCol.insert(p.col);
            }
        }

        for (int i = 0; i < patches.size(); ++i) {
            const auto& p = patches[i];
            if (!showClipped && p.clipped) continue;
            const QPainterPath path = hexPath(X(p.px), Y(p.py));
            if (p.clipped) {
                g.setPen(QPen(QColor("#C9C5B9"), 1, Qt::DashLine));
                g.setBrush(QColor(0, 0, 0, 12));
            } else {
                g.setPen(outlines ? QPen(QColor(0, 0, 0, 70), 1) : Qt::NoPen);
                g.setBrush(previewColor(p.recipe));
            }
            g.drawPath(path);
            if (i == selected) {
                g.setPen(QPen(QColor("#100A36"), std::max(1.6, s * 0.06)));
                g.setBrush(Qt::NoBrush);
                g.drawPath(path);
            }
        }
    }

signals:
    void patchClicked(int index);

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter g(this);
        g.fillRect(rect(), QColor("#FAFAF7"));
        render(g, rect().adjusted(8, 8, -8, -8));
    }
    void mousePressEvent(QMouseEvent* e) override {
        // 命中：取最近中心且距离 < 六边形半径的块
        if (patches.isEmpty()) return;
        // 与 render 相同的变换
        double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
        for (const auto& p : patches) {
            minX = std::min(minX, p.px); maxX = std::max(maxX, p.px);
            minY = std::min(minY, p.py); maxY = std::max(maxY, p.py);
        }
        const double margin = 1.6;
        minX -= margin; maxX += margin; minY -= margin; maxY += margin;
        const QRectF target = rect().adjusted(8, 8, -8, -8);
        const double s = std::min(target.width() / (maxX - minX),
                                  target.height() / (maxY - minY));
        const double ox = target.x() - minX * s + (target.width() - (maxX - minX) * s) / 2;
        const double oy = target.y() - minY * s + (target.height() - (maxY - minY) * s) / 2;
        int best = -1; double bestD = 1e18;
        for (int i = 0; i < patches.size(); ++i) {
            if (!showClipped && patches[i].clipped) continue;
            const double dx = e->position().x() - (ox + patches[i].px * s);
            const double dy = e->position().y() - (oy + patches[i].py * s);
            const double d = dx * dx + dy * dy;
            if (d < bestD) { bestD = d; best = i; }
        }
        if (best >= 0 && bestD < s * s) {
            selected = best;
            update();
            emit patchClicked(best);
        }
    }
};

// ─── 方向玫瑰图（左侧小示意：中心六边形 + 6 个方向三角）────────────

class DirectionRosette : public QWidget {
public:
    explicit DirectionRosette(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(150, 140);
    }
    QColor centerColor = QColor("#6FA8DC");
    QVector<QColor> dirColors = {QColor("#0A7FD4"), QColor("#E71655"), QColor("#FADA15"),
                                 QColor("#8FD8F2"), QColor("#F2A9CF"), QColor("#EFE58A")};
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter g(this);
        g.setRenderHint(QPainter::Antialiasing);
        g.fillRect(rect(), Qt::white);
        const QPointF c(width() / 2.0, height() / 2.0);
        const double R = 34, T = 58;
        QPainterPath hex;
        for (int i = 0; i < 6; ++i) {
            const double ang = (60.0 * i - 30.0) * M_PI / 180.0;
            const QPointF pt(c.x() + R * std::cos(ang), c.y() + R * std::sin(ang));
            if (i == 0) hex.moveTo(pt); else hex.lineTo(pt);
        }
        hex.closeSubpath();
        g.setPen(Qt::NoPen);
        g.setBrush(centerColor);
        g.drawPath(hex);
        QFont f = g.font(); f.setPointSizeF(9); f.setBold(true); g.setFont(f);
        // 方向 1..6：从东北起顺时针（与蜂巢一致）
        static const double angles[] = {-60, 0, 60, 120, 180, 240};
        for (int i = 0; i < 6; ++i) {
            const double a0 = (angles[i] - 14) * M_PI / 180.0;
            const double a1 = (angles[i] + 14) * M_PI / 180.0;
            const double am = angles[i] * M_PI / 180.0;
            QPainterPath tri;
            tri.moveTo(c.x() + (R + 3) * std::cos(a0), c.y() + (R + 3) * std::sin(a0));
            tri.lineTo(c.x() + T * std::cos(am), c.y() + T * std::sin(am));
            tri.lineTo(c.x() + (R + 3) * std::cos(a1), c.y() + (R + 3) * std::sin(a1));
            tri.closeSubpath();
            g.setBrush(dirColors[i]);
            g.drawPath(tri);
            g.setPen(QColor("#100A36"));
            g.drawText(QRectF(c.x() + (T + 8) * std::cos(am) - 8,
                              c.y() + (T + 8) * std::sin(am) - 8, 16, 16),
                       Qt::AlignCenter, QString::number(i + 1));
            g.setPen(Qt::NoPen);
        }
    }
};

// ─── 对话框 ─────────────────────────────────────────────────────────

VisualSpotTunerDialog::VisualSpotTunerDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("可视化专色优化"));
    resize(1080, 700);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(14);

    // ── 左列：读数 + 方向配置 ──
    auto* left = new QVBoxLayout;
    left->setSpacing(8);

    auto* form = new QFormLayout;
    form->setSpacing(6);
    nameEdit_ = new QLineEdit(QStringLiteral("专色 1"));
    form->addRow(QStringLiteral("专色"), nameEdit_);

    auto* labRow = new QHBoxLayout;
    labL_ = new QLabel("—"); labA_ = new QLabel("—"); labB_ = new QLabel("—");
    for (auto* l : {labL_, labA_, labB_}) {
        l->setStyleSheet("font-family:Menlo,monospace;background:#fff;"
                         "border:1px solid #DDD9CF;border-radius:4px;padding:3px 8px;");
        l->setMinimumWidth(58);
    }
    labRow->addWidget(new QLabel("L")); labRow->addWidget(labL_);
    labRow->addWidget(new QLabel("a")); labRow->addWidget(labA_);
    labRow->addWidget(new QLabel("b")); labRow->addWidget(labB_);
    measSrc_ = new QLabel(QStringLiteral("屏幕近似"));
    measSrc_->setStyleSheet("font-size:10px;color:#8A8779;");
    labRow->addWidget(measSrc_);
    labRow->addStretch();
    form->addRow(QStringLiteral("当前 Lab"), labRow);

    // 目标 Lab（专色的参考值，ΔE00 以它为基准）
    auto* tgtRow = new QHBoxLayout;
    auto mkTgt = [](double lo, double hi, double v) {
        auto* s = new QDoubleSpinBox;
        s->setRange(lo, hi); s->setDecimals(1); s->setValue(v); s->setFixedWidth(70);
        return s;
    };
    tgtL_ = mkTgt(0, 100, 71.0); tgtA_ = mkTgt(-128, 127, -12.0); tgtB_ = mkTgt(-128, 127, -30.0);
    tgtRow->addWidget(new QLabel("L")); tgtRow->addWidget(tgtL_);
    tgtRow->addWidget(new QLabel("a")); tgtRow->addWidget(tgtA_);
    tgtRow->addWidget(new QLabel("b")); tgtRow->addWidget(tgtB_);
    deltaE_ = new QLabel("ΔE00 —");
    deltaE_->setStyleSheet("font-family:Menlo,monospace;font-weight:700;color:#100A36;");
    tgtRow->addWidget(deltaE_);
    tgtRow->addStretch();
    form->addRow(QStringLiteral("目标 Lab"), tgtRow);

    auto* devRow = new QHBoxLayout;
    auto mkSpin = [] {
        auto* s = new QDoubleSpinBox;
        s->setRange(0, 100); s->setDecimals(2); s->setFixedWidth(74);
        return s;
    };
    devC_ = mkSpin(); devM_ = mkSpin(); devY_ = mkSpin(); devK_ = mkSpin();
    devRow->addWidget(new QLabel("C")); devRow->addWidget(devC_);
    devRow->addWidget(new QLabel("M")); devRow->addWidget(devM_);
    devRow->addWidget(new QLabel("Y")); devRow->addWidget(devY_);
    devRow->addWidget(new QLabel("K")); devRow->addWidget(devK_);
    devRow->addStretch();
    form->addRow(QStringLiteral("设备色"), devRow);
    left->addLayout(form);

    auto* rosette = new DirectionRosette;
    left->addWidget(rosette, 0, Qt::AlignHCenter);

    // 方向表：6 行 [通道] [步长]
    auto* dirHead = new QLabel(QStringLiteral("方向　　通道　　　　　　步长"));
    dirHead->setStyleSheet("font-weight:600;color:#4A4740;");
    left->addWidget(dirHead);
    static const int defaultChannel[] = {0, 1, 2, 3, 4, 5};
    static const char* defaultStep[]  = {"10%", "10%", "10%", "10%", "5%", "5%"};
    for (int i = 0; i < 6; ++i) {
        auto* row = new QHBoxLayout;
        auto* n = new QLabel(QString::number(i + 1));
        n->setFixedWidth(24);
        auto* ch = new QComboBox;
        for (const char* name : kChannelNames) ch->addItem(QString::fromUtf8(name));
        ch->setCurrentIndex(defaultChannel[i]);
        ch->setFixedWidth(120);
        auto* st = new QComboBox;
        st->addItems({"2.5%", "5%", "10%"});
        st->setCurrentText(QString::fromUtf8(defaultStep[i]));
        st->setFixedWidth(72);
        dirChannel_.push_back(ch);
        dirStep_.push_back(st);
        row->addWidget(n); row->addWidget(ch); row->addWidget(st); row->addStretch();
        left->addLayout(row);
        connect(ch, &QComboBox::currentIndexChanged, this, [this] { rebuildChart(); });
        connect(st, &QComboBox::currentIndexChanged, this, [this](int) {
            if (sameSteps_->isChecked()) {
                const QString v = qobject_cast<QComboBox*>(sender())->currentText();
                for (auto* s : dirStep_) { QSignalBlocker b(s); s->setCurrentText(v); }
            }
            rebuildChart();
        });
    }

    auto* optRow1 = new QHBoxLayout;
    optRow1->addWidget(new QLabel(QStringLiteral("页面")));
    pageSize_ = new QComboBox;
    pageSize_->addItems({QStringLiteral("A4 210×297 mm"), QStringLiteral("A3 297×420 mm")});
    optRow1->addWidget(pageSize_);
    optRow1->addWidget(new QLabel(QStringLiteral("外圈数")));
    shells_ = new QComboBox;
    shells_->addItems({"1", "2", "3", "4"});
    shells_->setCurrentText("4");
    optRow1->addWidget(shells_);
    optRow1->addStretch();
    left->addLayout(optRow1);

    sameSteps_ = new QCheckBox(QStringLiteral("相同步长"));
    showClipped_ = new QCheckBox(QStringLiteral("显示裁剪块"));
    showClipped_->setChecked(true);
    outlines_ = new QCheckBox(QStringLiteral("色块描边"));
    left->addWidget(sameSteps_);
    left->addWidget(showClipped_);
    left->addWidget(outlines_);
    left->addStretch();
    root->addLayout(left);

    // ── 右列：蜂巢 + 按钮 ──
    auto* right = new QVBoxLayout;
    chart_ = new SpotHexChart;
    right->addWidget(chart_, 1);

    auto* btns = new QHBoxLayout;
    auto* recenter = new QPushButton(QStringLiteral("设为中心"));
    recenter->setToolTip(QStringLiteral("以选中色块为新中心，围绕它继续细调（迭代逼近）"));
    auto* measure = new QPushButton(QStringLiteral("实测选中块 (i1)"));
    measure->setToolTip(QStringLiteral(
        "把 i1 对准打印蜂巢图上对应标号的色块按测量键：真实 Lab 回填并算 ΔE00。\n"
        "无仪器时用模拟读数演示流程。"));
    auto* save = new QPushButton(QStringLiteral("保存"));
    auto* print = new QPushButton(QStringLiteral("打印"));
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    bb->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    btns->addWidget(recenter);
    btns->addWidget(measure);
    btns->addStretch();
    btns->addWidget(save);
    btns->addWidget(print);
    btns->addWidget(bb);
    right->addLayout(btns);
    root->addLayout(right, 1);

    // ── 行为 ──
    connect(shells_, &QComboBox::currentIndexChanged, this, [this] { rebuildChart(); });
    connect(showClipped_, &QCheckBox::toggled, this, [this](bool on) {
        chart_->showClipped = on; chart_->update();
    });
    connect(outlines_, &QCheckBox::toggled, this, [this](bool on) {
        chart_->outlines = on; chart_->update();
    });
    for (auto* s : {devC_, devM_, devY_, devK_})
        connect(s, &QDoubleSpinBox::valueChanged, this, [this] { rebuildChart(); });
    connect(chart_, &SpotHexChart::patchClicked, this, &VisualSpotTunerDialog::onPatchSelected);
    connect(recenter, &QPushButton::clicked, this, &VisualSpotTunerDialog::recenterOnSelection);
    connect(measure, &QPushButton::clicked, this, &VisualSpotTunerDialog::measureSelected);
    for (auto* s : {tgtL_, tgtA_, tgtB_})
        connect(s, &QDoubleSpinBox::valueChanged, this, [this] { updateDeltaE(); });
    connect(save, &QPushButton::clicked, this, &VisualSpotTunerDialog::saveToLibrary);
    connect(print, &QPushButton::clicked, this, &VisualSpotTunerDialog::printChart);
    connect(bb, &QDialogButtonBox::accepted, this, [this] {
        emit recipeAccepted(nameEdit_->text(), selectedRecipe());
        accept();
    });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setCenterRecipe({48.81, 18.24, 4.0, 0.0});   // 演示初值（截图同款）
}

void VisualSpotTunerDialog::setSpotName(const QString& name) { nameEdit_->setText(name); }

void VisualSpotTunerDialog::setCenterRecipe(const SpotRecipe& r) {
    for (auto* s : {devC_, devM_, devY_, devK_}) { QSignalBlocker b(s); }
    QSignalBlocker bc(devC_), bm(devM_), by(devY_), bk(devK_);
    devC_->setValue(r.c); devM_->setValue(r.m); devY_->setValue(r.y); devK_->setValue(r.k);
    rebuildChart();
}

SpotRecipe VisualSpotTunerDialog::selectedRecipe() const {
    if (chart_->selected >= 0 && chart_->selected < chart_->patches.size())
        return chart_->patches[chart_->selected].recipe;
    return {devC_->value(), devM_->value(), devY_->value(), devK_->value()};
}

void VisualSpotTunerDialog::rebuildChart() {
    const SpotRecipe center{devC_->value(), devM_->value(), devY_->value(), devK_->value()};
    const int shells = shells_->currentText().toInt();

    // 每个方向的单步配方增量
    QVector<SpotRecipe> delta(6);
    for (int i = 0; i < 6; ++i) {
        const double step = dirStep_[i]->currentText().chopped(1).toDouble();
        delta[i] = channelDelta(dirChannel_[i]->currentIndex()) * step;
    }

    // 轴向坐标（pointy-top）方向 1..6：东北起顺时针（与玫瑰图一致）
    struct Ax { int q, r; };
    static const Ax dirs[6] = {{+1, -1}, {+1, 0}, {0, +1}, {-1, +1}, {-1, 0}, {0, -1}};

    QVector<HexPatch> out;
    auto push = [&](int q, int r, const SpotRecipe& rec, int ring) {
        HexPatch p;
        p.recipe = rec;
        p.clipped = rec.clipped();
        p.px = std::sqrt(3.0) * (q + r / 2.0);   // pointy-top axial → 逻辑坐标
        p.py = 1.5 * r;
        p.ring = ring;
        p.row = r + shells;                      // 行字母：顶行 A
        p.col = q + shells;                      // 列数字
        out.push_back(p);
    };
    push(0, 0, center, 0);
    for (int ring = 1; ring <= shells; ++ring) {
        for (int k = 0; k < 6; ++k) {
            const int k2 = (k + 1) % 6;
            for (int j = 0; j < ring; ++j) {
                const int a = ring - j, b = j;   // 块 = a·方向k + b·方向k+1
                const int q = a * dirs[k].q + b * dirs[k2].q;
                const int r = a * dirs[k].r + b * dirs[k2].r;
                push(q, r, center + delta[k] * a + delta[k2] * b, ring);
            }
        }
    }
    chart_->patches = out;
    chart_->selected = 0;
    chart_->update();
    onPatchSelected(0);
}

void VisualSpotTunerDialog::onPatchSelected(int index) {
    if (index < 0 || index >= chart_->patches.size()) return;
    hasMeasured_ = false;                       // 换块 → 旧实测作废
    measSrc_->setText(QStringLiteral("屏幕近似"));
    const SpotRecipe& r = chart_->patches[index].recipe;
    double L, a, b;
    previewLab(r, L, a, b);
    labL_->setText(QString::number(L, 'f', 1));
    labA_->setText(QString::number(a, 'f', 1));
    labB_->setText(QString::number(b, 'f', 1));
    updateDeltaE();
}

void VisualSpotTunerDialog::updateDeltaE() {
    double L, a, b;
    if (hasMeasured_) {
        L = measL_; a = measA_; b = measB_;
    } else {
        previewLab(selectedRecipe(), L, a, b);
    }
    const double dE = deltaE2000(tgtL_->value(), tgtA_->value(), tgtB_->value(), L, a, b);
    deltaE_->setText(QStringLiteral("ΔE00 %1").arg(dE, 0, 'f', 2));
    // 专色验收惯例：≤1 视为达标（绿），≤3 可接受（黄），否则红。
    const char* col = dE <= 1.0 ? "#2F9E6B" : dE <= 3.0 ? "#B58900" : "#B5124A";
    deltaE_->setStyleSheet(QStringLiteral(
        "font-family:Menlo,monospace;font-weight:700;color:%1;").arg(col));
}

void VisualSpotTunerDialog::setTargetLab(double L, double a, double b) {
    tgtL_->setValue(L); tgtA_->setValue(a); tgtB_->setValue(b);
}

void VisualSpotTunerDialog::measureSelected() {
    // 真仪器优先：i1Pro3 SDK 单点（阻塞调用放 std::thread，Queued 回主线程）。
    // 框架/设备不可用 → 模拟读数（屏幕近似 + 噪声），来源徽标如实标注。
    measSrc_->setText(QStringLiteral("测量中…"));
    QPointer<VisualSpotTunerDialog> self(this);
    const SpotRecipe rec = selectedRecipe();
    std::thread([self, rec] {
        double L = 0, a = 0, b = 0;
        bool real = false;
        auto& d = jcolour::i1sdk::I1Pro3Driver::instance();
        if (d.available() && (d.isOpen() || d.openFirst())) {
            d.setOption("MeasurementMode", "ReflectanceSpot");
            if (d.calibrate() && d.triggerMeasurement() && d.availableSamples() > 0) {
                std::array<float, 36> spec{};
                if (d.spectrum(spec, 0)) {
                    jcolour::spectral::Spectrum s{};
                    for (int i = 0; i < 36; ++i) s[size_t(i)] = double(spec[size_t(i)]);
                    const auto lab = jcolour::spectral::xyz_to_lab(
                        jcolour::spectral::spectrum_to_xyz(s));
                    L = lab.L; a = lab.a; b = lab.b;
                    real = true;
                }
            }
        }
        if (!real) {                           // 模拟：近似 Lab + 小噪声
            previewLab(rec, L, a, b);
            auto* rng = QRandomGenerator::global();
            L += (rng->generateDouble() - 0.5) * 1.2;
            a += (rng->generateDouble() - 0.5) * 1.2;
            b += (rng->generateDouble() - 0.5) * 1.2;
        }
        QMetaObject::invokeMethod(self, [self, L, a, b, real] {
            if (!self) return;
            self->hasMeasured_ = true;
            self->measL_ = L; self->measA_ = a; self->measB_ = b;
            self->labL_->setText(QString::number(L, 'f', 1));
            self->labA_->setText(QString::number(a, 'f', 1));
            self->labB_->setText(QString::number(b, 'f', 1));
            self->measSrc_->setText(real ? QStringLiteral("i1 实测")
                                         : QStringLiteral("模拟读数"));
            self->updateDeltaE();
        }, Qt::QueuedConnection);
    }).detach();
}

void VisualSpotTunerDialog::recenterOnSelection() {
    setCenterRecipe(selectedRecipe());
}

void VisualSpotTunerDialog::saveToLibrary() {
    const SpotRecipe r = selectedRecipe();
    double L, a, b;
    if (hasMeasured_) { L = measL_; a = measA_; b = measB_; }
    else previewLab(r, L, a, b);
    QSettings s;
    const int n = s.beginReadArray("spotColors");
    s.endArray();
    s.beginWriteArray("spotColors");
    s.setArrayIndex(n);
    s.setValue("name", nameEdit_->text());
    s.setValue("c", r.c); s.setValue("m", r.m); s.setValue("y", r.y); s.setValue("k", r.k);
    s.setValue("L", L); s.setValue("a", a); s.setValue("b", b);
    s.setValue("measured", hasMeasured_);
    s.setValue("targetL", tgtL_->value());
    s.setValue("targetA", tgtA_->value());
    s.setValue("targetB", tgtB_->value());
    s.setValue("savedAt", QDateTime::currentDateTime().toString(Qt::ISODate));
    s.endArray();
    measSrc_->setText(QStringLiteral("已存入专色库"));
}

void VisualSpotTunerDialog::printChart() {
    // 打印是这套方法的核心：屏幕只做布局预览，配方好坏在纸上目测。
    const SpotRecipe center{devC_->value(), devM_->value(), devY_->value(), devK_->value()};
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageOrientation(QPageLayout::Portrait);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted) return;

    QPainter g(&printer);
    const QRectF page = printer.pageRect(QPrinter::DevicePixel);
    // 页眉：专色名 + 中心配方 + 方向/步长说明（照 ORIS 打印版式）
    QFont f = g.font();
    f.setPointSizeF(14); f.setBold(true); g.setFont(f);
    g.drawText(QRectF(page.x(), page.y(), page.width(), 60), Qt::AlignLeft,
               QStringLiteral("可视化专色优化 — %1").arg(nameEdit_->text()));
    f.setPointSizeF(9); f.setBold(false); g.setFont(f);
    QString sub = QStringLiteral("中心 C%1 M%2 Y%3 K%4　·　")
        .arg(center.c, 0, 'f', 1).arg(center.m, 0, 'f', 1)
        .arg(center.y, 0, 'f', 1).arg(center.k, 0, 'f', 1);
    for (int i = 0; i < 6; ++i)
        sub += QStringLiteral("%1=%2 %3　")
            .arg(i + 1)
            .arg(dirChannel_[i]->currentText())
            .arg(dirStep_[i]->currentText());
    g.drawText(QRectF(page.x(), page.y() + 64, page.width(), 40), Qt::AlignLeft, sub);
    chart_->render(g, page.adjusted(0, 120, 0, -20));
}

// ─── 专色库管理 ─────────────────────────────────────────────────────

SpotLibraryDialog::SpotLibraryDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("专色库"));
    resize(720, 420);
    auto* v = new QVBoxLayout(this);

    table_ = new QTableWidget;
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels(
        {QStringLiteral("名称"), QStringLiteral("色样"), QStringLiteral("设备色 CMYK"),
         QStringLiteral("Lab"), QStringLiteral("来源"), QStringLiteral("保存时间")});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    v->addWidget(table_);

    auto* btns = new QHBoxLayout;
    auto* tune = new QPushButton(QStringLiteral("调整…"));
    tune->setToolTip(QStringLiteral("以该配方为中心重开蜂巢细调"));
    auto* del = new QPushButton(QStringLiteral("删除"));
    auto* close = new QPushButton(QStringLiteral("关闭"));
    btns->addWidget(tune);
    btns->addWidget(del);
    btns->addStretch();
    btns->addWidget(close);
    v->addLayout(btns);

    connect(tune, &QPushButton::clicked, this, &SpotLibraryDialog::tuneSelected);
    connect(del, &QPushButton::clicked, this, &SpotLibraryDialog::removeSelected);
    connect(table_, &QTableWidget::doubleClicked, this, [this] { tuneSelected(); });
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    reload();
}

void SpotLibraryDialog::reload() {
    QSettings s;
    const int n = s.beginReadArray("spotColors");
    table_->setRowCount(n);
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        const SpotRecipe r{s.value("c").toDouble(), s.value("m").toDouble(),
                           s.value("y").toDouble(), s.value("k").toDouble()};
        table_->setItem(i, 0, new QTableWidgetItem(s.value("name").toString()));
        auto* sw = new QTableWidgetItem;
        sw->setBackground(previewColor(r));
        table_->setItem(i, 1, sw);
        table_->setItem(i, 2, new QTableWidgetItem(
            QStringLiteral("%1 / %2 / %3 / %4")
                .arg(r.c, 0, 'f', 1).arg(r.m, 0, 'f', 1)
                .arg(r.y, 0, 'f', 1).arg(r.k, 0, 'f', 1)));
        table_->setItem(i, 3, new QTableWidgetItem(
            QStringLiteral("%1 %2 %3")
                .arg(s.value("L").toDouble(), 0, 'f', 1)
                .arg(s.value("a").toDouble(), 0, 'f', 1)
                .arg(s.value("b").toDouble(), 0, 'f', 1)));
        table_->setItem(i, 4, new QTableWidgetItem(
            s.value("measured").toBool() ? QStringLiteral("i1 实测")
                                         : QStringLiteral("屏幕近似")));
        table_->setItem(i, 5, new QTableWidgetItem(
            s.value("savedAt").toString().replace('T', ' ')));
    }
    s.endArray();
    table_->resizeColumnsToContents();
}

void SpotLibraryDialog::tuneSelected() {
    const int row = table_->currentRow();
    if (row < 0) return;
    QSettings s;
    s.beginReadArray("spotColors");
    s.setArrayIndex(row);
    const SpotRecipe r{s.value("c").toDouble(), s.value("m").toDouble(),
                       s.value("y").toDouble(), s.value("k").toDouble()};
    const QString name = s.value("name").toString();
    const double tL = s.value("targetL", 50.0).toDouble();
    const double tA = s.value("targetA", 0.0).toDouble();
    const double tB = s.value("targetB", 0.0).toDouble();
    s.endArray();

    auto* dlg = new VisualSpotTunerDialog(this);
    dlg->setSpotName(name);
    dlg->setCenterRecipe(r);
    dlg->setTargetLab(tL, tA, tB);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &QDialog::finished, this, [this] { reload(); });
    dlg->show();
}

void SpotLibraryDialog::removeSelected() {
    const int row = table_->currentRow();
    if (row < 0) return;
    // QSettings 数组不支持删中间项：整读整写。
    QSettings s;
    struct Entry { QVariantMap kv; };
    QVector<Entry> all;
    const int n = s.beginReadArray("spotColors");
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        if (i == row) continue;
        Entry e;
        for (const QString& key : s.childKeys()) e.kv[key] = s.value(key);
        all.push_back(e);
    }
    s.endArray();
    s.remove("spotColors");
    s.beginWriteArray("spotColors");
    for (int i = 0; i < all.size(); ++i) {
        s.setArrayIndex(i);
        for (auto it = all[i].kv.begin(); it != all[i].kv.end(); ++it)
            s.setValue(it.key(), it.value());
    }
    s.endArray();
    reload();
}

}  // namespace jcolour::ui

#include "spot_tuner_dialog.moc"
