#include "recalibration_dialog.h"

#include "i1_spot.h"
#include "lab_color.h"
#include "linearization_dialog.h"

#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPointer>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <thread>

namespace jcolour::ui {

using namespace jcolour::printbase;

namespace {

QString labText(const Lab& l) {
    return QStringLiteral("%1 / %2 / %3").arg(l.L, 0, 'f', 1).arg(l.a, 0, 'f', 1).arg(l.b, 0, 'f', 1);
}

QTableWidgetItem* item(const QString& t, const QColor& fg = QColor()) {
    auto* it = new QTableWidgetItem(t);
    if (fg.isValid()) it->setForeground(fg);
    return it;
}

QColor deColour(double de) {
    return de <= 1.0 ? QColor("#2F9E6B") : de <= 2.0 ? QColor("#B58900") : QColor("#c0392b");
}

}  // namespace

RecalibrationDialog::RecalibrationDialog(QWidget* parent, const QString& configId) : QDialog(parent) {
    setWindowTitle(QStringLiteral("打印机再校准（换纸 / 换墨批次）"));
    resize(1000, 700);
    auto* v = new QVBoxLayout(this);

    auto* intro = new QLabel(QStringLiteral(
        "<b>打印机再校准</b>（对标 GMG ColorServer「印刷机再校准」）：换纸批次或换墨批次后，"
        "打一张 <b>48 块</b>的小靶（每通道 4 阶 + 纸白 + 3 个灰），把机器拉回建档时的状态。"
        "<b>只更新线性化，不重建特性文件</b>——ICC、G7、墨限、TIL 都不动。"));
    intro->setWordWrap(true);
    intro->setStyleSheet("color:#4A4740;");
    v->addWidget(intro);

    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(QStringLiteral("打印环境")));
    envCombo_ = new QComboBox;
    row->addWidget(envCombo_, 2);
    row->addWidget(new QLabel(QStringLiteral("原因")));
    reasonCombo_ = new QComboBox;
    reasonCombo_->addItems({QStringLiteral("换墨批次"), QStringLiteral("换纸批次"), QStringLiteral("定期复查")});
    row->addWidget(reasonCombo_);
    row->addWidget(new QLabel(QStringLiteral("新批号")));
    batchEdit_ = new QLineEdit;
    batchEdit_->setPlaceholderText(QStringLiteral("如 #A8（可空）"));
    batchEdit_->setMaximumWidth(120);
    row->addWidget(batchEdit_);
    v->addLayout(row);

    envInfo_ = new QLabel;
    envInfo_->setWordWrap(true);
    envInfo_->setStyleSheet("font-size:11.5px;color:#7A776D;");
    v->addWidget(envInfo_);

    auto* bar = new QHBoxLayout;
    auto* printBtn = new QPushButton(QStringLiteral("打印再校准靶"));
    auto* measBtn = new QPushButton(QStringLiteral("实测下一块 (i1)"));
    auto* simBtn = new QPushButton(QStringLiteral("模拟整张（含模拟漂移）"));
    simBtn->setToolTip(QStringLiteral("无仪器时演示：按所选原因给模拟打印机加一组漂移，读数标「模拟」"));
    computeBtn_ = new QPushButton(QStringLiteral("计算校正"));
    verifyBtn_ = new QPushButton(QStringLiteral("复测验证（经新曲线）"));
    progress_ = new QLabel;
    progress_->setStyleSheet("font-family:Menlo,monospace;color:#0A6E8C;");
    for (auto* b : {printBtn, measBtn, simBtn, computeBtn_, verifyBtn_}) bar->addWidget(b);
    bar->addSpacing(10);
    bar->addWidget(progress_);
    bar->addStretch();
    v->addLayout(bar);

    auto* split = new QSplitter(Qt::Horizontal);
    chTable_ = new QTableWidget;
    chTable_->setColumnCount(5);
    chTable_->setHorizontalHeaderLabels({QStringLiteral("通道"), QStringLiteral("校准前 ΔE00 平均 / 最大"),
                                         QStringLiteral("复测 ΔE00 平均 / 最大"), QStringLiteral("50% 处修正"),
                                         QStringLiteral("状态")});
    patchTable_ = new QTableWidget;
    patchTable_->setColumnCount(6);
    patchTable_->setHorizontalHeaderLabels({QStringLiteral("色块"), QStringLiteral("建档时 Lab"),
                                            QStringLiteral("校准前实测"), QStringLiteral("ΔE00 前"),
                                            QStringLiteral("复测"), QStringLiteral("ΔE00 后")});
    for (auto* t : {chTable_, patchTable_}) {
        t->verticalHeader()->setVisible(false);
        t->horizontalHeader()->setStretchLastSection(true);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setSelectionBehavior(QAbstractItemView::SelectRows);
        split->addWidget(t);
    }
    split->setSizes({430, 570});
    v->addWidget(split, 1);

    summary_ = new QLabel;
    summary_->setWordWrap(true);
    summary_->setStyleSheet("background:#E7F7FD;border:1px solid #A9E4F7;border-radius:6px;"
                            "padding:9px 12px;color:#100A36;");
    v->addWidget(summary_);

    auto* btns = new QHBoxLayout;
    btns->addStretch();
    applyBtn_ = new QPushButton(QStringLiteral("应用到打印环境（只更新线性化）"));
    applyBtn_->setObjectName("primary");
    auto* close = new QPushButton(QStringLiteral("关闭"));
    btns->addWidget(applyBtn_);
    btns->addWidget(close);
    v->addLayout(btns);

    connect(printBtn, &QPushButton::clicked, this, &RecalibrationDialog::printChart);
    connect(measBtn, &QPushButton::clicked, this, &RecalibrationDialog::measureNext);
    connect(simBtn, &QPushButton::clicked, this, &RecalibrationDialog::simulateAll);
    connect(computeBtn_, &QPushButton::clicked, this, &RecalibrationDialog::compute);
    connect(verifyBtn_, &QPushButton::clicked, this, [this] {
        phase_ = Phase::After;
        std::fill(afterDone_.begin(), afterDone_.end(), false);
        afterEval_ = {};
        refreshTables();
        refreshSummary();
    });
    connect(applyBtn_, &QPushButton::clicked, this, &RecalibrationDialog::applyToEnvironment);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(reasonCombo_, &QComboBox::currentIndexChanged, this, [this] { haveDrift_ = false; });

    configs_ = PaperState::instance().listConfigs();
    int sel = -1;
    for (int i = 0; i < configs_.size(); ++i) {
        const auto& c = configs_[i];
        envCombo_->addItem(QStringLiteral("%1 · %2 · v%3 · 墨批 %4%5")
                               .arg(c.printer, c.paperName).arg(c.version).arg(c.inkBatch,
                                    c.hasBase ? QString() : QStringLiteral("（无建档基准）")));
        if (c.id == configId) sel = i;
        if (sel < 0 && configId.isEmpty() && c.hasBase && c.enabled) sel = i;
    }
    if (sel < 0 && !configs_.isEmpty()) sel = 0;
    connect(envCombo_, &QComboBox::currentIndexChanged, this, &RecalibrationDialog::loadConfig);
    if (sel >= 0) {
        const QSignalBlocker b(envCombo_);
        envCombo_->setCurrentIndex(sel);
        loadConfig(sel);
    } else {
        envInfo_->setText(QStringLiteral("尚无打印环境：请先在「打印环境 / 校准」里完整建档。"));
        for (auto* b : {printBtn, measBtn, simBtn, computeBtn_, verifyBtn_, applyBtn_}) b->setEnabled(false);
    }
}

void RecalibrationDialog::loadConfig(int index) {
    if (index < 0 || index >= configs_.size()) return;
    config_ = configs_[index];
    demoBase_ = !config_.hasBase;
    if (!demoBase_) {
        base_ = config_.base;
    } else {
        // 旧版环境没有记录建档地基：用模拟机生成一份「演示基准」，只演示流程，不允许应用
        const SimulatedPrinter sim = LinearizationDialog::simulatorForPaper(config_.paperName);
        base_ = {};
        for (const auto& ch : epson_p_active_channels())
            base_.channels.push_back(make_channel_base(sim.measureRamp(ch, default_ramp_steps())));
        recal::ChartSpec s;
        s.channels = epson_p_active_channels();
        for (const auto& p : recal::build_chart(s))
            if (p.channel == "GRAY") base_.grayReference.push_back({p.id, sim.print(recal::device_inks(p, base_, s))});
    }
    spec_ = {};
    for (const auto& c : base_.channels)
        if (c.ramp.valid()) spec_.channels.push_back(c.channel);
    chart_ = recal::build_chart(spec_);
    ref_ = recal::reference_labs(base_, chart_);

    QString hist;
    if (!config_.recalHistory.isEmpty()) {
        const auto& r = config_.recalHistory.last();
        hist = QStringLiteral("　上次再校准 %1（%2）：%3 → %4")
                   .arg(r.at.toString(QStringLiteral("yyyy-MM-dd")), r.reason)
                   .arg(r.beforeAvg, 0, 'f', 2)
                   .arg(r.afterAvg >= 0 ? QString::number(r.afterAvg, 'f', 2) : QStringLiteral("未复测"));
    }
    envInfo_->setText(demoBase_
        ? QStringLiteral("<span style='color:#B58900'>此环境建档时没有记录线性化基准（旧版环境）——下面用<b>模拟基准</b>演示流程，不能应用。"
                         "要真正再校准，请先用「重新校正」走完整建档（含墨限 + 线性化）。</span>")
        : QStringLiteral("建档 %1 · 墨限 / 线性化 %2 通道 · TIL %3% · 灰块基准 %4 块%5")
              .arg(config_.lastCalibrated.toString(QStringLiteral("yyyy-MM-dd")))
              .arg(spec_.channels.size()).arg(base_.totalInkLimit, 0, 'f', 0)
              .arg(base_.grayReference.size()).arg(hist));
    resetMeasurements();
}

void RecalibrationDialog::resetMeasurements() {
    phase_ = Phase::Before;
    before_.assign(chart_.size(), Lab{0, 0, 0});
    after_.assign(chart_.size(), Lab{0, 0, 0});
    beforeDone_.assign(chart_.size(), false);
    afterDone_.assign(chart_.size(), false);
    computed_ = false;
    result_ = {};
    afterEval_ = {};
    simulated_ = false;
    haveDrift_ = false;
    refreshTables();
    refreshSummary();
}

bool RecalibrationDialog::allMeasured() const {
    const auto& d = phase_ == Phase::Before ? beforeDone_ : afterDone_;
    return !d.empty() && std::all_of(d.begin(), d.end(), [](bool b) { return b; });
}

PrintBase RecalibrationDialog::currentPrintBase() const {
    return phase_ == Phase::After && computed_ ? recal::apply_recalibration(base_, result_) : base_;
}

void RecalibrationDialog::printChart() {
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted) return;
    QPainter g(&printer);
    const QRectF page = printer.pageRect(QPrinter::DevicePixel);
    QFont f = g.font();
    f.setPointSizeF(12); f.setBold(true); g.setFont(f);
    g.drawText(QRectF(page.x(), page.y(), page.width(), 40), Qt::AlignLeft,
               QStringLiteral("再校准靶 — %1 · %2（%3 块 · 经%4线性化 · 直通打印）")
                   .arg(config_.printer, config_.paperName).arg(chart_.size())
                   .arg(phase_ == Phase::After ? QStringLiteral("新") : QStringLiteral("当前")));
    f.setPointSizeF(7); f.setBold(false); g.setFont(f);
    const int cols = 8;
    const double cw = page.width() / cols, ch = cw * 0.8;
    const SimulatedPrinter sim = LinearizationDialog::simulatorForPaper(config_.paperName);
    const PrintBase pb = currentPrintBase();
    for (size_t i = 0; i < chart_.size(); ++i) {
        const QRectF cell(page.x() + (i % cols) * cw + 3, page.y() + 60 + (i / cols) * (ch + 22), cw - 6, ch);
        g.fillRect(cell, labToQColor(sim.print(recal::device_inks(chart_[i], pb, spec_))));
        g.setPen(Qt::black);
        g.drawText(QRectF(cell.x(), cell.bottom() + 2, cell.width(), 16), Qt::AlignHCenter,
                   QString::fromStdString(chart_[i].id));
    }
    g.drawText(QRectF(page.x(), page.bottom() - 40, page.width(), 38), Qt::AlignLeft | Qt::TextWordWrap,
               QStringLiteral("系统打印对话框只能输出 RGB 近似色块（演练用）；真实再校准须由 PJL 直连送原始通道墨量（实机验收项）。"));
}

void RecalibrationDialog::simulateAll() {
    if (!haveDrift_) {
        driftedSim_ = LinearizationDialog::simulatorForPaper(config_.paperName);
        QRandomGenerator* rng = QRandomGenerator::global();
        const auto& chs = spec_.channels;
        switch (reasonCombo_->currentIndex()) {
            case 0:   // 换墨批次：随机 4 个通道浓度 ±(3–8)%
                for (int k = 0; k < 4 && !chs.empty(); ++k) {
                    const double sign = rng->generateDouble() < 0.5 ? -1.0 : 1.0;
                    driftedSim_.driftInk(chs[size_t(rng->bounded(int(chs.size())))],
                                         1.0 + sign * (0.03 + 0.05 * rng->generateDouble()), 0.0);
                }
                break;
            case 1: {  // 换纸批次：纸白变冷（增白剂多一些）+ 网点扩大略增
                const Lab p = driftedSim_.paper();
                driftedSim_.setPaper({p.L + 0.6, p.a + 0.8, p.b - 2.5 - 1.5 * rng->generateDouble()});
                for (const auto& c : chs) driftedSim_.driftInk(c, 1.0, 0.03);
                break;
            }
            default:   // 定期复查：全通道小漂移
                for (const auto& c : chs)
                    driftedSim_.driftInk(c, 0.96 + 0.08 * rng->generateDouble(), 0.0);
        }
        haveDrift_ = true;
    }
    const PrintBase pb = currentPrintBase();
    auto& meas = phase_ == Phase::Before ? before_ : after_;
    auto& done = phase_ == Phase::Before ? beforeDone_ : afterDone_;
    for (size_t i = 0; i < chart_.size(); ++i) {
        if (done[i]) continue;
        Lab l = driftedSim_.print(recal::device_inks(chart_[i], pb, spec_));
        l.L += (QRandomGenerator::global()->generateDouble() - 0.5) * 0.15;
        meas[i] = l;
        done[i] = true;
    }
    simulated_ = true;
    if (phase_ == Phase::After) afterEval_ = recal::evaluate(chart_, ref_, after_);
    refreshTables();
    refreshSummary();
}

void RecalibrationDialog::measureNext() {
    if (busy_) return;
    auto& done = phase_ == Phase::Before ? beforeDone_ : afterDone_;
    size_t idx = done.size();
    for (size_t i = 0; i < done.size(); ++i) if (!done[i]) { idx = i; break; }
    if (idx >= done.size()) return;
    if (!i1Available()) {
        progress_->setText(QStringLiteral("未检测到 i1Pro3（X-Rite SDK 未装或仪器未连）——可用「模拟整张」演示。"));
        return;
    }
    busy_ = true;
    progress_->setText(QStringLiteral("请把 i1 放在 %1 色块上…").arg(QString::fromStdString(chart_[idx].id)));
    QPointer<RecalibrationDialog> self(this);
    const Phase ph = phase_;
    std::thread([self, idx, ph] {
        Lab lab{};
        const bool real = measureSpotLab(lab);
        QMetaObject::invokeMethod(self, [self, idx, ph, lab, real] {
            if (!self) return;
            self->busy_ = false;
            if (!real || self->phase_ != ph) { self->refreshSummary(); return; }
            auto& meas = ph == Phase::Before ? self->before_ : self->after_;
            auto& dn = ph == Phase::Before ? self->beforeDone_ : self->afterDone_;
            meas[idx] = lab;
            dn[idx] = true;
            if (ph == Phase::After && self->allMeasured())
                self->afterEval_ = recal::evaluate(self->chart_, self->ref_, self->after_);
            self->refreshTables();
            self->refreshSummary();
        }, Qt::QueuedConnection);
    }).detach();
}

void RecalibrationDialog::compute() {
    if (!std::all_of(beforeDone_.begin(), beforeDone_.end(), [](bool b) { return b; })) return;
    result_ = recal::compute_recalibration(base_, chart_, before_);
    computed_ = result_.ok;
    refreshTables();
    refreshSummary();
}

void RecalibrationDialog::applyToEnvironment() {
    if (!computed_ || demoBase_) return;
    config_.base = recal::apply_recalibration(base_, result_);
    config_.lastRecalibrated = QDateTime::currentDateTime();
    RecalRecord rec;
    rec.at = config_.lastRecalibrated;
    rec.reason = reasonCombo_->currentText();
    rec.newBatch = batchEdit_->text().trimmed();
    rec.beforeAvg = result_.before.all.avg;
    rec.beforeMax = result_.before.all.max;
    if (afterEval_.all.count > 0) { rec.afterAvg = afterEval_.all.avg; rec.afterMax = afterEval_.all.max; }
    rec.simulated = simulated_;
    config_.recalHistory.append(rec);
    if (reasonCombo_->currentIndex() == 0 && !rec.newBatch.isEmpty())
        config_.inkBatch = rec.newBatch;     // 换墨批次：环境跟上新墨批，列印前闸门不再报墨批不符
    PaperState::instance().saveConfig(config_);
    base_ = config_.base;
    configs_ = PaperState::instance().listConfigs();
    summary_->setText(summary_->text() +
                      QStringLiteral("<br><b style='color:#2F9E6B'>✓ 已写入「%1」：只更新了线性化曲线，ICC / G7 / 墨限 / TIL 与版本号 v%2 不变。</b>")
                          .arg(config_.paperName).arg(config_.version));
    applyBtn_->setEnabled(false);
    emit applied(config_.id);
}

void RecalibrationDialog::refreshTables() {
    patchTable_->setRowCount(int(chart_.size()));
    for (int i = 0; i < int(chart_.size()); ++i) {
        patchTable_->setItem(i, 0, item(QString::fromStdString(chart_[size_t(i)].id)));
        patchTable_->setItem(i, 1, item(ref_.valid[size_t(i)] ? labText(ref_.labs[size_t(i)]) : QStringLiteral("（未记录）")));
        const bool b = beforeDone_[size_t(i)], a = afterDone_[size_t(i)];
        patchTable_->setItem(i, 2, item(b ? labText(before_[size_t(i)]) : QStringLiteral("—")));
        QString deB = QStringLiteral("—"), deA = QStringLiteral("—");
        QColor cB, cA;
        if (computed_ && size_t(i) < result_.before.patches.size() && result_.before.patches[size_t(i)].hasReference) {
            const double d = result_.before.patches[size_t(i)].de00;
            deB = QString::number(d, 'f', 2); cB = deColour(d);
        }
        patchTable_->setItem(i, 3, item(deB, cB));
        patchTable_->setItem(i, 4, item(a ? labText(after_[size_t(i)]) : QStringLiteral("—")));
        if (size_t(i) < afterEval_.patches.size() && afterEval_.patches[size_t(i)].hasReference) {
            const double d = afterEval_.patches[size_t(i)].de00;
            deA = QString::number(d, 'f', 2); cA = deColour(d);
        }
        patchTable_->setItem(i, 5, item(deA, cA));
    }
    patchTable_->resizeColumnsToContents();

    chTable_->setRowCount(computed_ ? int(result_.channels.size()) : 0);
    for (int i = 0; computed_ && i < int(result_.channels.size()); ++i) {
        const auto& cc = result_.channels[size_t(i)];
        chTable_->setItem(i, 0, item(QString::fromStdString(cc.channel)));
        chTable_->setItem(i, 1, item(QStringLiteral("%1 / %2").arg(cc.before.avg, 0, 'f', 2).arg(cc.before.max, 0, 'f', 2),
                                     deColour(cc.before.max)));
        QString after = QStringLiteral("—");
        QColor ac;
        if (afterEval_.all.count > 0) {
            recal::Summary s;
            for (const auto& p : afterEval_.patches)
                if (p.channel == cc.channel && p.hasReference) {
                    s.avg = (s.avg * s.count + p.de00) / (s.count + 1);
                    s.max = std::max(s.max, p.de00);
                    ++s.count;
                }
            after = QStringLiteral("%1 / %2").arg(s.avg, 0, 'f', 2).arg(s.max, 0, 'f', 2);
            ac = deColour(s.max);
        }
        chTable_->setItem(i, 2, item(after, ac));
        chTable_->setItem(i, 3, item(QStringLiteral("%1%2%").arg(cc.shiftAtMid >= 0 ? "+" : "")
                                         .arg(cc.shiftAtMid * 100, 0, 'f', 1)));
        chTable_->setItem(i, 4, cc.hitLimit
            ? item(QStringLiteral("⚠ 已到墨限，只能部分拉回"), QColor("#B58900"))
            : item(cc.maxShift < 0.005 ? QStringLiteral("无需修正") : QStringLiteral("✓ 已修正"),
                   QColor("#2F9E6B")));
    }
    chTable_->resizeColumnsToContents();
}

void RecalibrationDialog::refreshSummary() {
    const auto& done = phase_ == Phase::Before ? beforeDone_ : afterDone_;
    const int n = int(std::count(done.begin(), done.end(), true));
    progress_->setText(QStringLiteral("%1：已测 %2 / %3%4")
                           .arg(phase_ == Phase::Before ? QStringLiteral("校准前") : QStringLiteral("复测"))
                           .arg(n).arg(done.size())
                           .arg(simulated_ ? QStringLiteral("（模拟）") : QString()));
    computeBtn_->setEnabled(phase_ == Phase::Before &&
                            std::all_of(beforeDone_.begin(), beforeDone_.end(), [](bool b) { return b; }));
    verifyBtn_->setEnabled(computed_);
    applyBtn_->setEnabled(computed_ && !demoBase_);
    applyBtn_->setToolTip(demoBase_ ? QStringLiteral("演示基准不能写入环境：先完整建档") : QString());

    if (!computed_) {
        summary_->setText(QStringLiteral("实测（或模拟）整张校准前色块后按「计算校正」。"));
        return;
    }
    QStringList warn;
    for (const auto& w : result_.warnings) {
        const QString q = QString::fromStdString(w);
        if (q.startsWith("PAPER_SHIFT:"))
            warn << QStringLiteral("纸白比建档时变了 ΔE00 %1（换纸批次？线性化可拉回阶调，纸白本身拉不回）").arg(q.mid(12));
        else if (q.startsWith("HIT_LIMIT:"))
            warn << QStringLiteral("%1 通道墨变淡，已到墨限只能部分拉回——偏差仍大时请完整重新建档").arg(q.mid(10));
        else if (q == "NO_REFERENCE_GRAY")
            warn << QStringLiteral("建档时没有记录灰块基准，灰块只列实测、不计偏差");
    }
    QString txt = QStringLiteral("<b>校准前</b>：全部 ΔE00 平均 %1 / 最大 %2（通道色块 %3 / %4%5）")
                      .arg(result_.before.all.avg, 0, 'f', 2).arg(result_.before.all.max, 0, 'f', 2)
                      .arg(result_.before.channelsOnly.avg, 0, 'f', 2).arg(result_.before.channelsOnly.max, 0, 'f', 2)
                      .arg(result_.before.grays.count
                               ? QStringLiteral("；灰块 %1 / %2").arg(result_.before.grays.avg, 0, 'f', 2)
                                     .arg(result_.before.grays.max, 0, 'f', 2)
                               : QString());
    if (afterEval_.all.count > 0)
        txt += QStringLiteral("<br><b>复测（新线性化）</b>：全部 ΔE00 平均 %1 / 最大 %2（通道色块 %3 / %4%5）")
                   .arg(afterEval_.all.avg, 0, 'f', 2).arg(afterEval_.all.max, 0, 'f', 2)
                   .arg(afterEval_.channelsOnly.avg, 0, 'f', 2).arg(afterEval_.channelsOnly.max, 0, 'f', 2)
                   .arg(afterEval_.grays.count
                            ? QStringLiteral("；灰块 %1 / %2").arg(afterEval_.grays.avg, 0, 'f', 2)
                                  .arg(afterEval_.grays.max, 0, 'f', 2)
                            : QString());
    else if (phase_ == Phase::After)
        txt += QStringLiteral("<br>复测：请用新曲线重新打印同一张靶并实测（或模拟）。");
    else
        txt += QStringLiteral("<br>下一步：「复测验证」确认效果，或直接「应用」。");
    if (!warn.isEmpty()) txt += QStringLiteral("<br><span style='color:#B58900'>⚠ %1</span>").arg(warn.join(QStringLiteral("；")));
    if (simulated_) txt += QStringLiteral("<br><span style='color:#8A8779'>本次读数含模拟数据，仅演示流程。</span>");
    summary_->setText(txt);
}

}  // namespace jcolour::ui
