#include "linearization_dialog.h"

#include "../print_base_io.h"
#include "i1_spot.h"
#include "lab_color.h"
#include "recalibration.h"

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
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <thread>

namespace jcolour::ui {

using namespace jcolour::printbase;

// ── 曲线图：原始 SCTV 响应（灰）+ 线性化曲线（蓝）+ 理想直线（虚线） ─────────
class CurvePlot : public QWidget {
public:
    explicit CurvePlot(QWidget* parent = nullptr) : QWidget(parent) { setMinimumSize(260, 220); }
    void set(const ChannelBase* cb) { cb_ = cb ? *cb : ChannelBase{}; has_ = cb != nullptr; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#FFFFFF"));
        const QRectF r = rect().adjusted(34, 12, -12, -28);
        p.setPen(QColor("#DDD9CF"));
        p.drawRect(r);
        for (int i = 1; i < 4; ++i) {
            p.drawLine(QPointF(r.left() + r.width() * i / 4, r.top()), QPointF(r.left() + r.width() * i / 4, r.bottom()));
            p.drawLine(QPointF(r.left(), r.top() + r.height() * i / 4), QPointF(r.right(), r.top() + r.height() * i / 4));
        }
        QFont f = p.font(); f.setPointSizeF(8.5); p.setFont(f);
        p.setPen(QColor("#8A8779"));
        p.drawText(QRectF(r.left(), r.bottom() + 4, r.width(), 14), Qt::AlignCenter,
                   QStringLiteral("输入 0–100%（灰：原始墨量；蓝：请求值）"));
        p.drawText(QRectF(0, r.top(), 30, 14), Qt::AlignRight, QStringLiteral("100"));
        p.drawText(QRectF(0, r.bottom() - 12, 30, 14), Qt::AlignRight, QStringLiteral("0"));
        auto pt = [&](double x, double y) { return QPointF(r.left() + x * r.width(), r.bottom() - y * r.height()); };
        p.setPen(QPen(QColor("#B8B4A8"), 1, Qt::DashLine));
        p.drawLine(pt(0, 0), pt(1, 1));
        if (!has_ || !cb_.ramp.valid()) {
            p.setPen(QColor("#8A8779"));
            p.drawText(r, Qt::AlignCenter, QStringLiteral("测完该通道后显示曲线"));
            return;
        }
        const double L = cb_.effectiveLimit();
        // 原始响应：x = 原始墨量，y = SCTV/100（墨限处归一）
        QPainterPath raw;
        for (int i = 0; i <= 100; ++i) {
            const double u = i / 100.0;
            const double y = ramp_sctv_at(cb_.ramp, std::min(u, L), L) / 100.0;
            i == 0 ? raw.moveTo(pt(u, y)) : raw.lineTo(pt(u, std::min(1.05, y)));
        }
        p.setPen(QPen(QColor("#8A8779"), 1.6));
        p.drawPath(raw);
        for (const auto& rp : cb_.ramp.patches) {
            const double y = ramp_sctv_at(cb_.ramp, std::min(rp.input, L), L) / 100.0;
            p.setBrush(QColor("#8A8779"));
            p.drawEllipse(pt(rp.input, std::min(1.05, y)), 2.5, 2.5);
        }
        // 线性化曲线：x = 请求值 t，y = 设备墨量 u
        QPainterPath lin;
        for (int i = 0; i <= 100; ++i) {
            const double t = i / 100.0;
            i == 0 ? lin.moveTo(pt(t, cb_.curve.apply(t))) : lin.lineTo(pt(t, cb_.curve.apply(t)));
        }
        p.setPen(QPen(QColor("#0A6E8C"), 2.2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(lin);
        // 墨限线
        p.setPen(QPen(QColor("#c0392b"), 1, Qt::DotLine));
        p.drawLine(pt(L, 0), pt(L, 1));
        p.drawText(pt(L, 1) + QPointF(3, 12), QStringLiteral("墨限 %1%").arg(int(std::lround(L * 100))));
    }

private:
    ChannelBase cb_;
    bool has_ = false;
};

namespace {
const char* kChannelZh(const std::string& ch) {
    static const std::pair<const char*, const char*> k[] = {
        {"K", "主黑 PK/MK"}, {"LC", "浅青"}, {"C", "青"}, {"VLM", "浅洋红"}, {"VM", "洋红"},
        {"Y", "黄"}, {"Or", "橙"}, {"GY", "灰"}, {"LGY", "浅灰"}, {"V", "紫"}, {"Gr", "绿"}};
    for (const auto& e : k) if (ch == e.first) return e.second;
    return "";
}
}  // namespace

SimulatedPrinter LinearizationDialog::simulatorForPaper(const QString& paperName) {
    SimulatedPrinter sim = SimulatedPrinter::epson_p_nominal();
    QRandomGenerator rng(quint32(qHash(paperName)));
    for (const auto& ch : epson_p_active_channels())
        sim.driftInk(ch, 0.94 + 0.12 * rng.generateDouble(), 0.03 * rng.generateDouble());
    return sim;
}

LinearizationDialog::LinearizationDialog(const QString& paperName, const PrintBase& existing,
                                         QWidget* parent)
    : QDialog(parent), paperName_(paperName),
      channels_(epson_p_active_channels()), steps_(default_ramp_steps()),
      sim_(simulatorForPaper(paperName)) {
    setWindowTitle(QStringLiteral("每通道墨限 + 线性化 — %1").arg(paperName_));
    resize(980, 660);

    meas_.resize(int(channels_.size()));
    measured_.resize(int(channels_.size()));
    for (int i = 0; i < int(channels_.size()); ++i) {
        meas_[i].resize(int(steps_.size()));
        measured_[i].fill(false, int(steps_.size()));
    }
    // 已有地基（重做线性化时）：把旧实测带进来，便于只补测个别通道
    for (int i = 0; i < int(channels_.size()); ++i) {
        const ChannelBase* cb = existing.find(channels_[size_t(i)]);
        if (!cb) continue;
        for (const auto& rp : cb->ramp.patches)
            for (int s = 0; s < int(steps_.size()); ++s)
                if (std::fabs(rp.input - steps_[size_t(s)]) < 1e-6) {
                    meas_[i][s] = rp.lab;
                    measured_[i][s] = true;
                }
    }
    base_ = existing;
    base_.totalInkLimit = 0;                 // 线性化一变，TIL 必须重测
    base_.tilMeasuredOnLinearized = false;

    auto* v = new QVBoxLayout(this);
    auto* intro = new QLabel(QStringLiteral(
        "<b>建档前置 · 第 1 步：单通道墨限 + 每通道线性化</b>（对标 GMG SmartProfiler：特性化之前先线性化与限墨）<br>"
        "打印 11 个活动通道 × 13 阶单通道渐变（直通、无色彩管理）→ 逐块实测 → 每通道自动找墨限"
        "（ΔE00 距纸白达峰值 98%），可手改；墨限内按 <b>SCTV（ISO 20654）</b>反解线性化曲线。"
        "完成后才能测总墨量 TIL，TIL 测完才能做 G7 / ICC。"));
    intro->setWordWrap(true);
    intro->setStyleSheet("color:#4A4740;");
    v->addWidget(intro);

    auto* bar = new QHBoxLayout;
    auto* printBtn = new QPushButton(QStringLiteral("打印线性化靶"));
    auto* measBtn = new QPushButton(QStringLiteral("实测下一块 (i1)"));
    auto* simBtn = new QPushButton(QStringLiteral("模拟整张（无仪器演示）"));
    simBtn->setToolTip(QStringLiteral("用内置模拟打印机生成读数，结果标「模拟」，仅供熟悉流程"));
    progress_ = new QLabel;
    progress_->setStyleSheet("font-family:Menlo,monospace;color:#0A6E8C;");
    bar->addWidget(printBtn);
    bar->addWidget(measBtn);
    bar->addWidget(simBtn);
    bar->addSpacing(12);
    bar->addWidget(progress_);
    bar->addStretch();
    v->addLayout(bar);

    auto* mid = new QHBoxLayout;
    table_ = new QTableWidget;
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({QStringLiteral("通道"), QStringLiteral("已测"),
                                       QStringLiteral("自动墨限"), QStringLiteral("墨限"),
                                       QStringLiteral("峰值 ΔE00"), QStringLiteral("线性化前偏差"),
                                       QStringLiteral("状态")});
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mid->addWidget(table_, 3);
    plot_ = new CurvePlot;
    mid->addWidget(plot_, 2);
    v->addLayout(mid, 1);

    grayInfo_ = new QLabel;
    grayInfo_->setWordWrap(true);
    grayInfo_->setStyleSheet("font-size:11.5px;color:#4A4740;");
    auto* grayRow = new QHBoxLayout;
    auto* grayBtn = new QPushButton(QStringLiteral("实测基准灰块 (i1)"));
    grayBtn->setToolTip(QStringLiteral("线性化完成后，打印再校准靶上的 3 个灰块并实测，作为换纸 / 换墨批次后再校准的对照"));
    grayRow->addWidget(grayInfo_, 1);
    grayRow->addWidget(grayBtn);
    v->addLayout(grayRow);

    verdict_ = new QLabel;
    verdict_->setWordWrap(true);
    verdict_->setStyleSheet("background:#E7F7FD;border:1px solid #A9E4F7;border-radius:6px;"
                            "padding:8px 12px;color:#100A36;");
    v->addWidget(verdict_);

    auto* btns = new QHBoxLayout;
    btns->addStretch();
    auto* cancel = new QPushButton(QStringLiteral("取消"));
    doneBtn_ = new QPushButton(QStringLiteral("完成线性化"));
    doneBtn_->setObjectName("primary");
    btns->addWidget(cancel);
    btns->addWidget(doneBtn_);
    v->addLayout(btns);

    connect(printBtn, &QPushButton::clicked, this, &LinearizationDialog::printTarget);
    connect(measBtn, &QPushButton::clicked, this, &LinearizationDialog::measureNext);
    connect(simBtn, &QPushButton::clicked, this, &LinearizationDialog::simulateAll);
    connect(grayBtn, &QPushButton::clicked, this, &LinearizationDialog::measureGrayNext);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(doneBtn_, &QPushButton::clicked, this, &QDialog::accept);
    connect(table_, &QTableWidget::currentCellChanged, this,
            [this](int r, int, int, int) { showCurve(r); });

    buildTable();
    const auto keptGray = base_.grayReference;   // 重建同一份实测不改曲线，保留灰块基准
    for (int i = 0; i < int(channels_.size()); ++i) rebuildChannel(i);
    base_.grayReference = keptGray;
    table_->selectRow(0);
    updateVerdict();
}

void LinearizationDialog::buildTable() {
    table_->setRowCount(int(channels_.size()));
    limitSpins_.clear();
    for (int i = 0; i < int(channels_.size()); ++i) {
        const auto& ch = channels_[size_t(i)];
        table_->setItem(i, 0, new QTableWidgetItem(QStringLiteral("%1  %2")
                                                       .arg(QString::fromStdString(ch), QString::fromUtf8(kChannelZh(ch)))));
        for (int c = 1; c < 7; ++c) if (c != 3) table_->setItem(i, c, new QTableWidgetItem(QStringLiteral("—")));
        auto* spin = new QSpinBox;
        spin->setRange(0, 100);
        spin->setSingleStep(5);
        spin->setSuffix("%");
        spin->setSpecialValueText(QStringLiteral("自动"));
        spin->setMinimumWidth(84);
        spin->setToolTip(QStringLiteral("0 = 用自动墨限；改值后该通道曲线立即按新墨限重建"));
        if (const ChannelBase* cb = base_.find(ch); cb && cb->limitOverride > 0)
            spin->setValue(int(std::lround(cb->limitOverride * 100)));
        connect(spin, &QSpinBox::valueChanged, this, [this, i] { rebuildChannel(i); updateVerdict(); });
        table_->setCellWidget(i, 3, spin);
        limitSpins_.push_back(spin);
    }
    table_->resizeColumnsToContents();
}

void LinearizationDialog::rebuildChannel(int i) {
    const auto& ch = channels_[size_t(i)];
    ChannelRamp ramp;
    ramp.channel = ch;
    for (int s = 0; s < int(steps_.size()); ++s)
        if (measured_[i][s]) ramp.patches.push_back({steps_[size_t(s)], meas_[i][s]});
    ChannelBase* cb = base_.find(ch);
    if (!ramp.valid()) {
        if (cb) {   // 实测不足：去掉该通道的曲线
            cb->ramp = ramp;
            cb->curve = {};
            cb->referenceCurve = {};
        }
        refreshRow(i);
        return;
    }
    const int ov = limitSpins_.value(i) ? limitSpins_[i]->value() : 0;
    ChannelBase nb = make_channel_base(ramp, ov > 0 ? ov / 100.0 : -1.0);
    if (cb) *cb = nb; else base_.channels.push_back(nb);
    // 线性化变了：已有的灰块基准作废
    base_.grayReference.clear();
    refreshRow(i);
    if (table_->currentRow() == i) showCurve(i);
}

void LinearizationDialog::refreshRow(int i) {
    const auto& ch = channels_[size_t(i)];
    int n = 0;
    for (bool m : measured_[i]) n += m;
    table_->item(i, 1)->setText(QStringLiteral("%1/%2").arg(n).arg(steps_.size()));
    const ChannelBase* cb = base_.find(ch);
    if (!cb || cb->curve.empty()) {
        for (int c : {2, 4, 5}) table_->item(i, c)->setText(QStringLiteral("—"));
        table_->item(i, 6)->setText(n ? QStringLiteral("测量中") : QStringLiteral("未测"));
        table_->item(i, 6)->setForeground(QColor("#8A8779"));
        return;
    }
    table_->item(i, 2)->setText(QStringLiteral("%1%").arg(int(std::lround(cb->limit.limit * 100))));
    table_->item(i, 4)->setText(QString::number(cb->limit.peakResponse, 'f', 1));
    table_->item(i, 5)->setText(QStringLiteral("%1 SCTV").arg(cb->curve.nonlinearityBefore, 0, 'f', 1));
    QString st = QStringLiteral("✓ 已线性化");
    QColor col("#2F9E6B");
    if (!cb->curve.monotonic) { st = QStringLiteral("✗ 响应非单调：清洗喷头后重测"); col = QColor("#c0392b"); }
    else if (cb->limit.reversal) { st = QStringLiteral("⚠ 峰后回落（青铜化），已限在峰前"); col = QColor("#B58900"); }
    table_->item(i, 6)->setText(st);
    table_->item(i, 6)->setForeground(col);
}

void LinearizationDialog::showCurve(int row) {
    if (row < 0 || row >= int(channels_.size())) { plot_->set(nullptr); return; }
    plot_->set(base_.find(channels_[size_t(row)]));
}

int LinearizationDialog::measuredCount() const {
    int n = 0;
    for (const auto& ch : measured_) for (bool m : ch) n += m;
    return n;
}

void LinearizationDialog::updateVerdict() {
    table_->resizeColumnsToContents();
    progress_->setText(QStringLiteral("已测 %1 / %2%3")
                           .arg(measuredCount()).arg(channels_.size() * steps_.size())
                           .arg(simulated_ ? QStringLiteral("（含模拟读数）") : QString()));
    PrintBase probe = base_;
    probe.totalInkLimit = 0;
    const GateResult g = profiling_gate(probe, channels_);
    QStringList chIssues;
    for (const auto& is : g.issues)
        if (!is.channel.empty()) chIssues << gateIssueText(is);
    const bool linReady = chIssues.isEmpty();
    doneBtn_->setEnabled(linReady && !busy_);
    if (linReady) {
        QStringList lim;
        for (const auto& c : base_.channels)
            lim << QStringLiteral("%1 %2%").arg(QString::fromStdString(c.channel))
                       .arg(int(std::lround(c.effectiveLimit() * 100)));
        verdict_->setText(QStringLiteral("<b>11 个通道均已线性化。</b>墨限：%1<br>"
                                         "下一步：在已线性化设备上测总墨量 TIL。%2")
                              .arg(lim.join(QStringLiteral(" · ")),
                                   simulated_ ? QStringLiteral("<span style='color:#B58900'>（本次含模拟读数，仅演示流程）</span>")
                                              : QString()));
    } else {
        verdict_->setText(QStringLiteral("尚不能完成：%1").arg(chIssues.mid(0, 4).join(QStringLiteral("；"))
                                                                + (chIssues.size() > 4 ? QStringLiteral("…") : QString())));
    }
    grayInfo_->setText(base_.grayReference.empty()
        ? QStringLiteral("再校准基准（可选）：实测再校准靶上的 3 个灰块（25/50/75%），换纸 / 换墨批次后用来对照灰平衡。未记录时再校准只对照单通道色块。")
        : QStringLiteral("再校准基准：已记录 %1 个灰块%2。").arg(base_.grayReference.size())
              .arg(simulated_ ? QStringLiteral("（模拟）") : QString()));
}

void LinearizationDialog::storeMeasurement(int ch, int step, const Lab& lab, bool real) {
    meas_[ch][step] = lab;
    measured_[ch][step] = true;
    if (!real) simulated_ = true;
    rebuildChannel(ch);
    updateVerdict();
}

void LinearizationDialog::measureNext() {
    if (busy_) return;
    int ci = -1, si = -1;
    for (int c = 0; c < int(channels_.size()) && ci < 0; ++c)
        for (int s = 0; s < int(steps_.size()); ++s)
            if (!measured_[c][s]) { ci = c; si = s; break; }
    if (ci < 0) return;
    busy_ = true;
    progress_->setText(QStringLiteral("请把 i1 放在 %1 通道 %2% 色块上…")
                           .arg(QString::fromStdString(channels_[size_t(ci)]))
                           .arg(int(std::lround(steps_[size_t(si)] * 100))));
    QPointer<LinearizationDialog> self(this);
    const std::string ch = channels_[size_t(ci)];
    const double u = steps_[size_t(si)];
    const SimulatedPrinter sim = sim_;
    std::thread([self, ci, si, ch, u, sim] {
        Lab lab{};
        const bool real = measureSpotLab(lab);
        if (!real) {
            lab = sim.printSingle(ch, u);
            lab.L += (QRandomGenerator::global()->generateDouble() - 0.5) * 0.3;
        }
        QMetaObject::invokeMethod(self, [self, ci, si, lab, real] {
            if (!self) return;
            self->busy_ = false;
            self->storeMeasurement(ci, si, lab, real);
            self->table_->selectRow(ci);
        }, Qt::QueuedConnection);
    }).detach();
}

void LinearizationDialog::simulateAll() {
    for (int c = 0; c < int(channels_.size()); ++c)
        for (int s = 0; s < int(steps_.size()); ++s) {
            Lab lab = sim_.printSingle(channels_[size_t(c)], steps_[size_t(s)]);
            lab.L += (QRandomGenerator::global()->generateDouble() - 0.5) * 0.2;
            meas_[c][s] = lab;
            measured_[c][s] = true;
        }
    simulated_ = true;
    for (int c = 0; c < int(channels_.size()); ++c) rebuildChannel(c);
    // 模拟时顺带记录灰块基准（经新曲线打印）
    recal::ChartSpec spec;
    spec.channels = channels_;
    for (const auto& p : recal::build_chart(spec))
        if (p.channel == "GRAY")
            base_.grayReference.push_back({p.id, sim_.print(recal::device_inks(p, base_, spec))});
    updateVerdict();
    showCurve(table_->currentRow());
}

void LinearizationDialog::measureGrayNext() {
    if (busy_) return;
    recal::ChartSpec spec;
    spec.channels = channels_;
    std::vector<recal::ChartPatch> grays;
    for (const auto& p : recal::build_chart(spec))
        if (p.channel == "GRAY") grays.push_back(p);
    for (const auto& g : base_.grayReference)
        grays.erase(std::remove_if(grays.begin(), grays.end(),
                                   [&](const recal::ChartPatch& p) { return p.id == g.id; }),
                    grays.end());
    if (grays.empty()) return;
    for (const auto& ch : spec.grayRecipe)
        if (const ChannelBase* cb = base_.find(ch.first); !cb || cb->curve.empty()) {
            verdict_->setText(QStringLiteral("先完成 %1 通道的线性化，再测基准灰块。").arg(QString::fromStdString(ch.first)));
            return;
        }
    busy_ = true;
    const recal::ChartPatch target = grays.front();
    grayInfo_->setText(QStringLiteral("请把 i1 放在再校准靶的 %1 灰块上…").arg(QString::fromStdString(target.id)));
    QPointer<LinearizationDialog> self(this);
    const auto inks = recal::device_inks(target, base_, spec);
    const SimulatedPrinter sim = sim_;
    std::thread([self, target, inks, sim] {
        Lab lab{};
        const bool real = measureSpotLab(lab);
        if (!real) lab = sim.print(inks);
        QMetaObject::invokeMethod(self, [self, target, lab, real] {
            if (!self) return;
            self->busy_ = false;
            if (!real) self->simulated_ = true;
            self->base_.grayReference.push_back({target.id, lab});
            self->updateVerdict();
        }, Qt::QueuedConnection);
    }).detach();
}

void LinearizationDialog::printTarget() {
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted) return;
    QPainter g(&printer);
    const QRectF page = printer.pageRect(QPrinter::DevicePixel);
    QFont f = g.font();
    f.setPointSizeF(12); f.setBold(true); g.setFont(f);
    g.drawText(QRectF(page.x(), page.y(), page.width(), 40), Qt::AlignLeft,
               QStringLiteral("线性化靶 — %1（11 通道 × 13 阶 · 直通打印 · 无色彩管理）").arg(paperName_));
    f.setPointSizeF(7); f.setBold(false); g.setFont(f);
    const double top = page.y() + 60;
    const double rowH = (page.height() - 120) / double(channels_.size());
    const double labelW = page.width() * 0.08;
    const double cellW = (page.width() - labelW) / double(steps_.size());
    for (int c = 0; c < int(channels_.size()); ++c) {
        const double y = top + c * rowH;
        g.setPen(Qt::black);
        g.drawText(QRectF(page.x(), y, labelW, rowH), Qt::AlignVCenter | Qt::AlignLeft,
                   QString::fromStdString(channels_[size_t(c)]));
        for (int s = 0; s < int(steps_.size()); ++s) {
            const QRectF cell(page.x() + labelW + s * cellW + 2, y + 2, cellW - 4, rowH - 4);
            // 屏幕 / 系统打印只能给出 RGB 近似；实机须经 PJL 直通原始通道值（ADR-26）
            g.fillRect(cell, labToQColor(sim_.printSingle(channels_[size_t(c)], steps_[size_t(s)])));
            if (c == 0)
                g.drawText(QRectF(cell.x(), top - 16, cell.width(), 14), Qt::AlignHCenter,
                           QStringLiteral("%1%").arg(int(std::lround(steps_[size_t(s)] * 100))));
        }
    }
    g.drawText(QRectF(page.x(), page.bottom() - 50, page.width(), 48), Qt::AlignLeft | Qt::TextWordWrap,
               QStringLiteral("注意：通过系统打印对话框输出的是 RGB 近似色块，只用于排版与流程演练；"
                              "真实线性化须由 jcolour PJL 直连把原始通道墨量送到打印机（实机验收项）。"));
}

}  // namespace jcolour::ui
