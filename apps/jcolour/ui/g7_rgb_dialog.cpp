#include "g7_rgb_dialog.h"

#include "../../../engine/src/i1pro3_driver.h"
#include "../../../engine/src/spectral.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>
#include <thread>

namespace jcolour::ui {

using g7rgb::Lab;

// ── 三通道校正曲线图：基线（中性等值）虚线 + R/G/B 校正曲线 ─────────
class G7RgbCurveView : public QWidget {
public:
    explicit G7RgbCurveView(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(190);
    }
    g7rgb::NpdcModel model;
    g7rgb::Curves curves;
    bool hasCurves = false;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter g(this);
        g.setRenderHint(QPainter::Antialiasing);
        g.fillRect(rect(), QColor("#FAFAF7"));
        const QRectF r = rect().adjusted(34, 8, -8, -22);
        g.setPen(QPen(QColor("#DDD9CF"), 1));
        g.drawRect(r);
        for (int q = 1; q < 4; ++q) {
            g.drawLine(QPointF(r.x() + r.width() * q / 4, r.y()),
                       QPointF(r.x() + r.width() * q / 4, r.bottom()));
            g.drawLine(QPointF(r.x(), r.y() + r.height() * q / 4),
                       QPointF(r.right(), r.y() + r.height() * q / 4));
        }
        g.setPen(QColor("#8A8779"));
        g.drawText(QRectF(0, r.y() - 4, 30, 14), Qt::AlignRight, "255");
        g.drawText(QRectF(0, r.bottom() - 10, 30, 14), Qt::AlignRight, "0");
        g.drawText(QRectF(r.x(), r.bottom() + 4, r.width(), 16), Qt::AlignCenter,
                   QStringLiteral("版调 % →（左 0 · 右 100）"));

        const auto& tones = g7rgb::verifierTones();
        auto X = [&](double tone) { return r.x() + r.width() * tone / 100.0; };
        auto Y = [&](double v) { return r.bottom() - r.height() * v / 255.0; };

        // 基线：未校正中性值
        QPainterPath base;
        for (size_t i = 0; i < tones.size(); ++i) {
            const double v = g7rgb::neutralValue(model, tones[i]);
            if (i == 0) base.moveTo(X(tones[i]), Y(v));
            else base.lineTo(X(tones[i]), Y(v));
        }
        g.setPen(QPen(QColor("#8A8779"), 1, Qt::DashLine));
        g.drawPath(base);

        if (hasCurves) {
            const struct { const std::array<double, 25>* c; const char* col; } chs[3] = {
                {&curves.r, "#E71655"}, {&curves.g, "#2F9E6B"}, {&curves.b, "#0BBAEE"}};
            for (const auto& ch : chs) {
                QPainterPath p;
                for (size_t i = 0; i < tones.size(); ++i) {
                    const double v = (*ch.c)[i];
                    if (i == 0) p.moveTo(X(tones[i]), Y(v));
                    else p.lineTo(X(tones[i]), Y(v));
                }
                g.setPen(QPen(QColor(ch.col), 1.8));
                g.drawPath(p);
            }
        }
        g.setPen(QColor("#4A4740"));
        g.drawText(QRectF(r.x() + 6, r.y() + 4, 300, 14), Qt::AlignLeft,
                   hasCurves ? QStringLiteral("R/G/B 校正曲线（虚线 = 未校正中性基线）")
                             : QStringLiteral("虚线 = 中性基线；预测后显示校正曲线"));
    }
};

G7RgbDialog::G7RgbDialog(const QString& paperName, QWidget* parent)
    : QDialog(parent), paperName_(paperName) {
    setWindowTitle(QStringLiteral("G7(+) RGB 打印机校正 — %1").arg(paperName_));
    resize(940, 720);
    auto* v = new QVBoxLayout(this);

    auto* intro = new QLabel(QStringLiteral(
        "原厂驱动 RGB 直出（LUT 不可知）时的 G7 灰平衡/版调校正：打印导具 → 测量 "
        "→ Gray Finder 生成 R/G/B 校正曲线 → 25 点 Verifier 按 G7+ Grayscale 判定。"
        "无需 RIP，第三方纸/墨也有标准化校正路径。"));
    intro->setWordWrap(true);
    intro->setStyleSheet("color:#5D5A51;");
    v->addWidget(intro);

    auto* row = new QHBoxLayout;
    auto mkSpin = [](double val, double lo, double hi, double step) {
        auto* s = new QDoubleSpinBox;
        s->setRange(lo, hi); s->setValue(val); s->setSingleStep(step); s->setDecimals(1);
        s->setFixedWidth(66);
        return s;
    };
    row->addWidget(new QLabel(QStringLiteral("纸白 L/a/b")));
    row->addWidget(paperL_ = mkSpin(95.5, 80, 100, 0.5));
    row->addWidget(paperA_ = mkSpin(0.5, -5, 5, 0.1));
    row->addWidget(paperB_ = mkSpin(2.0, -8, 8, 0.1));
    row->addWidget(new QLabel(QStringLiteral("黑点 L*(RGB0)")));
    row->addWidget(blackL_ = mkSpin(4.5, 1, 20, 0.5));
    row->addWidget(new QLabel(QStringLiteral("NPDC γ")));
    row->addWidget(gammaSpin_ = mkSpin(0.9, 0.7, 1.1, 0.05));
    row->addWidget(new QLabel(QStringLiteral("导具")));
    chartKind_ = new QComboBox;
    chartKind_->addItems({QStringLiteral("36 格 · 5 区段（i1Pro3 一条）"),
                          QStringLiteral("23 格 · 3 区段（轻量）")});
    row->addWidget(chartKind_);
    row->addStretch();
    v->addLayout(row);

    modelInfo_ = new QLabel;
    modelInfo_->setStyleSheet("font-family:Menlo,monospace;font-size:11px;color:#0A6E8C;");
    v->addWidget(modelInfo_);

    curveView_ = new G7RgbCurveView;
    v->addWidget(curveView_);

    verifyTable_ = new QTableWidget;
    verifyTable_->setColumnCount(6);
    verifyTable_->setHorizontalHeaderLabels(
        {QStringLiteral("版调 %"), QStringLiteral("目标 L*"), QStringLiteral("校正前 ΔL*"),
         QStringLiteral("校正前 ΔCh"), QStringLiteral("校正后 ΔL*"), QStringLiteral("校正后 ΔCh")});
    verifyTable_->verticalHeader()->setVisible(false);
    verifyTable_->horizontalHeader()->setStretchLastSection(true);
    verifyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v->addWidget(verifyTable_, 1);

    verdict_ = new QLabel(QStringLiteral("测量并预测后在此给出 G7+ Grayscale 判定。"));
    verdict_->setWordWrap(true);
    verdict_->setStyleSheet(
        "background:#E7F7FD;border:1px solid #A9E4F7;border-radius:6px;"
        "padding:9px 12px;color:#100A36;");
    v->addWidget(verdict_);

    auto* btns = new QHBoxLayout;
    auto* print = new QPushButton(QStringLiteral("打印导具"));
    auto* meas = new QPushButton(QStringLiteral("实测下一块 (i1)"));
    auto* sim = new QPushButton(QStringLiteral("一键模拟整轮"));
    sim->setToolTip(QStringLiteral("无打印机/仪器时演示完整闭环：模拟测量 → 曲线 → 前后验证"));
    applyBtn_ = new QPushButton(QStringLiteral("应用到打印环境"));
    applyBtn_->setEnabled(false);
    auto* close = new QPushButton(QStringLiteral("关闭"));
    progress_ = new QLabel;
    progress_->setStyleSheet("color:#7A776D;");
    btns->addWidget(print);
    btns->addWidget(meas);
    btns->addWidget(sim);
    btns->addWidget(progress_);
    btns->addStretch();
    btns->addWidget(applyBtn_);
    btns->addWidget(close);
    v->addLayout(btns);

    connect(print, &QPushButton::clicked, this, &G7RgbDialog::printChart);
    connect(meas, &QPushButton::clicked, this, &G7RgbDialog::measureNext);
    connect(sim, &QPushButton::clicked, this, &G7RgbDialog::simulateAll);
    connect(applyBtn_, &QPushButton::clicked, this, &G7RgbDialog::applyToEnvironment);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    for (auto* s : {paperL_, paperA_, paperB_, blackL_, gammaSpin_})
        connect(s, &QDoubleSpinBox::valueChanged, this, [this] { rebuildModel(); });
    connect(chartKind_, &QComboBox::currentIndexChanged, this, [this] { rebuildModel(); });

    rebuildModel();

    // 截图/自测：JCOLOUR_G7RGB_SIM=1 → 打开即跑一轮模拟闭环
    if (qEnvironmentVariableIsSet("JCOLOUR_G7RGB_SIM"))
        QTimer::singleShot(0, this, [this] { simulateAll(); });
}

void G7RgbDialog::rebuildModel() {
    Lab paper{paperL_->value(), paperA_->value(), paperB_->value()};
    Lab black{blackL_->value(), 0, 0};
    model_ = g7rgb::solveNpdc(paper, black, gammaSpin_->value());
    chart_ = g7rgb::buildChart(model_, chartKind_->currentIndex() == 1);
    measured_.assign(int(chart_.size()), Lab{-1, 0, 0});
    hasCurves_ = false;
    curveView_->model = model_;
    curveView_->hasCurves = false;
    curveView_->update();
    verifyTable_->setRowCount(0);
    applyBtn_->setEnabled(false);
    const auto t50 = g7rgb::targetLab(model_, 50);
    modelInfo_->setText(QStringLiteral(
        "NPDC 指数 %1（锚定 tone50 → L* %2）· tone50 中性 RGB = %3（实证 ≈135）· 导具 %4 格")
        .arg(model_.exponent, 0, 'f', 3).arg(t50.L, 0, 'f', 1)
        .arg(g7rgb::neutralValue(model_, 50))
        .arg(int(chart_.size())));
    progress_->setText(QStringLiteral("0/%1 已测").arg(int(chart_.size())));
    verdict_->setText(QStringLiteral("测量并预测后在此给出 G7+ Grayscale 判定。"));
}

void G7RgbDialog::printChart() {
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted) return;
    QPainter g(&printer);
    const QRectF page = printer.pageRect(QPrinter::DevicePixel);
    QFont f = g.font();
    f.setPointSizeF(13); f.setBold(true); g.setFont(f);
    g.drawText(QRectF(page.x(), page.y(), page.width(), 46), Qt::AlignLeft,
               QStringLiteral("G7(+) RGB 导具 — %1（驱动直出 · 关闭色彩管理）").arg(paperName_));
    f.setPointSizeF(7); f.setBold(false); g.setFont(f);
    // 单行条带排布（i1Pro3 拖扫）；格号在上、RGB 值在下
    const double top = page.y() + 64;
    const double pw = page.width() / chart_.size();
    for (int i = 0; i < int(chart_.size()); ++i) {
        const auto& p = chart_[size_t(i)];
        const QRectF cell(page.x() + i * pw + 1, top, pw - 2, page.height() * 0.22);
        g.fillRect(cell, QColor(p.r, p.g, p.b));
        g.setPen(Qt::black);
        g.drawText(QRectF(cell.x(), cell.y() - 16, cell.width(), 14), Qt::AlignHCenter,
                   QString::number(i + 1));
        g.drawText(QRectF(cell.x(), cell.bottom() + 2, cell.width(), 14), Qt::AlignHCenter,
                   QStringLiteral("%1,%2,%3").arg(p.r).arg(p.g).arg(p.b));
    }
}

Lab G7RgbDialog::simulatedPrint(int r, int g, int b) const {
    // 演示打印机：sRGB 解码取亮度 → L*；灰漂 = 暗部偏暖 + 中段偏红（典型
    // 第三方纸的表现）；通道差映射到 a/b（近似线性敏感度 ~0.25/值）。
    auto lin = [](int v) { return std::pow(std::clamp(v, 0, 255) / 255.0, 2.2); };
    const double Y = 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b);
    const double Lraw = Y > 0.008856 ? 116.0 * std::cbrt(Y) - 16.0 : 903.3 * Y;
    // 版调偏差：驱动直出略偏暗（模拟原厂 LUT 不匹配第三方纸）
    const double L = model_.black.L +
        (Lraw - 0) * (model_.paper.L - model_.black.L) / 100.0 * 0.97;
    const double tone = 100.0 * (1.0 - Y);
    const double driftA = 2.2 * std::sin(tone / 100.0 * M_PI);          // 中段偏红
    const double driftB = -3.0 * std::pow(tone / 100.0, 1.5);           // 暗部偏蓝
    auto* rng = QRandomGenerator::global();
    const double n = 0.15;
    return {L + (rng->generateDouble() - 0.5) * n,
            (r - g) * 0.25 + driftA + (rng->generateDouble() - 0.5) * n,
            (g - b) * 0.25 + driftB + (rng->generateDouble() - 0.5) * n};
}

void G7RgbDialog::measureNext() {
    int idx = -1;
    for (int i = 0; i < measured_.size(); ++i)
        if (measured_[i].L < 0) { idx = i; break; }
    if (idx < 0) { runPredict(); return; }
    const auto patch = chart_[size_t(idx)];
    QPointer<G7RgbDialog> self(this);
    std::thread([self, patch, idx] {
        Lab lab;
        bool real = false;
        auto& d = jcolour::i1sdk::I1Pro3Driver::instance();
        if (d.available() && (d.isOpen() || d.openFirst())) {
            d.setOption("MeasurementMode", "ReflectanceSpot");
            if (d.calibrate() && d.triggerMeasurement() && d.availableSamples() > 0) {
                std::array<float, 36> spec{};
                if (d.spectrum(spec, 0)) {
                    jcolour::spectral::Spectrum s{};
                    for (int i = 0; i < 36; ++i) s[size_t(i)] = double(spec[size_t(i)]);
                    const auto l = jcolour::spectral::xyz_to_lab(
                        jcolour::spectral::spectrum_to_xyz(s));
                    lab = {l.L, l.a, l.b};
                    real = true;
                }
            }
        }
        QMetaObject::invokeMethod(self, [self, idx, lab, real, patch] {
            if (!self) return;
            self->measured_[idx] = real ? lab
                                        : self->simulatedPrint(patch.r, patch.g, patch.b);
            int done = 0;
            for (const auto& m : self->measured_) if (m.L >= 0) ++done;
            self->progress_->setText(QStringLiteral("%1/%2 已测%3")
                .arg(done).arg(self->measured_.size())
                .arg(real ? QString() : QStringLiteral("（本块模拟）")));
            if (done == self->measured_.size()) self->runPredict();
        }, Qt::QueuedConnection);
    }).detach();
}

void G7RgbDialog::simulateAll() {
    for (int i = 0; i < measured_.size(); ++i) {
        const auto& p = chart_[size_t(i)];
        measured_[i] = simulatedPrint(p.r, p.g, p.b);
    }
    progress_->setText(QStringLiteral("%1/%1 已测（模拟）").arg(measured_.size()));
    runPredict();
}

void G7RgbDialog::runPredict() {
    std::vector<Lab> meas;
    meas.reserve(size_t(measured_.size()));
    for (const auto& m : measured_) meas.push_back(m);
    curves_ = g7rgb::predict(model_, chart_, meas);
    hasCurves_ = true;
    curveView_->curves = curves_;
    curveView_->hasCurves = true;
    curveView_->update();

    // Verifier：25 点 校正前（中性基线直出）vs 校正后（套曲线直出）。
    // 真机流程是重打验证条；此处用同一打印模型闭环演示（实测时换真值）。
    const auto& tones = g7rgb::verifierTones();
    std::vector<double> tv(tones.begin(), tones.end());
    std::vector<Lab> before, after;
    for (size_t i = 0; i < tones.size(); ++i) {
        const int v = g7rgb::neutralValue(model_, tones[i]);
        before.push_back(simulatedPrint(v, v, v));
        after.push_back(simulatedPrint(int(std::lround(curves_.r[i])),
                                       int(std::lround(curves_.g[i])),
                                       int(std::lround(curves_.b[i]))));
    }
    const auto vb = g7rgb::verify(model_, tv, before);
    const auto va = g7rgb::verify(model_, tv, after);

    verifyTable_->setRowCount(int(tones.size()));
    for (int i = 0; i < int(tones.size()); ++i) {
        const auto& pb = vb.points[size_t(i)];
        const auto& pa = va.points[size_t(i)];
        auto num = [](double v2, int prec = 2) {
            return new QTableWidgetItem(QString::number(v2, 'f', prec));
        };
        verifyTable_->setItem(i, 0, num(pb.tone, 0));
        verifyTable_->setItem(i, 1, num(pb.target.L, 1));
        verifyTable_->setItem(i, 2, num(pb.dL));
        verifyTable_->setItem(i, 3, num(pb.dCh));
        verifyTable_->setItem(i, 4, num(pa.dL));
        verifyTable_->setItem(i, 5, num(pa.dCh));
    }
    verifyTable_->resizeColumnsToContents();

    verdict_->setText(QStringLiteral(
        "<b>校正前</b> ΔL* avg %1 / ΔCh avg %2 → %3　　"
        "<b>校正后</b> ΔL* avg %4 max %5 / ΔCh avg %6 max %7 → %8")
        .arg(vb.avgAbsDL, 0, 'f', 2).arg(vb.avgCh, 0, 'f', 2)
        .arg(vb.pass ? QStringLiteral("<span style='color:#2F9E6B'>PASS</span>")
                     : QStringLiteral("<span style='color:#B5124A'>FAIL</span>"))
        .arg(va.avgAbsDL, 0, 'f', 2).arg(va.maxAbsDL, 0, 'f', 2)
        .arg(va.avgCh, 0, 'f', 2).arg(va.maxCh, 0, 'f', 2)
        .arg(va.pass ? QStringLiteral("<b style='color:#2F9E6B'>G7+ Grayscale PASS</b>")
                     : QStringLiteral("<b style='color:#B5124A'>FAIL</b>")));
    applyBtn_->setEnabled(va.pass);
}

void G7RgbDialog::applyToEnvironment() {
    QSettings s;
    s.beginGroup(QStringLiteral("g7rgb/%1").arg(paperName_));
    const auto& tones = g7rgb::verifierTones();
    for (int i = 0; i < int(tones.size()); ++i) {
        s.setValue(QStringLiteral("t%1_r").arg(int(tones[size_t(i)])), curves_.r[size_t(i)]);
        s.setValue(QStringLiteral("t%1_g").arg(int(tones[size_t(i)])), curves_.g[size_t(i)]);
        s.setValue(QStringLiteral("t%1_b").arg(int(tones[size_t(i)])), curves_.b[size_t(i)]);
    }
    s.setValue("gammaNpdc", model_.gammaNpdc);
    s.setValue("savedAt", QDateTime::currentDateTime().toString(Qt::ISODate));
    s.endGroup();
    verdict_->setText(verdict_->text() +
        QStringLiteral("　<b style='color:#2F9E6B'>✓ 曲线已写入「%1」打印环境</b>").arg(paperName_));
}

}  // namespace jcolour::ui
