#include "til_wizard.h"

#include "../../../engine/src/i1pro3_driver.h"
#include "../../../engine/src/spectral.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPointer>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>
#include <thread>

namespace jcolour::ui {

namespace {

// 介质类型 → 行业参考区间（用户提供的印前经验值 + 喷墨补充）
struct MediaRef { const char* name; int lo, hi; };
const MediaRef kMediaRefs[] = {
    {"高等级涂布（铜版）", 300, 320},
    {"亚光涂布", 280, 300},
    {"未涂布胶版纸", 220, 240},
    {"新闻纸", 200, 220},
    {"喷墨艺术纸（棉浆）", 260, 300},
    {"喷墨相纸（RC）", 280, 320},
};

// 组合色块的屏幕预览（朴素叠墨近似——真实评估靠打印）
QColor previewColor(double c, double m, double y, double k) {
    const double kk = 1.0 - k / 100.0;
    auto f = [kk](double v) { return (1.0 - v / 100.0) * kk; };
    return QColor::fromRgbF(float(f(c)), float(f(m)), float(f(y)));
}

}  // namespace


TILWizardDialog::TILWizardDialog(const QString& paperName, QWidget* parent)
    : QDialog(parent), paperName_(paperName) {
    setWindowTitle(QStringLiteral("总墨量上限 TIL 测定 — %1").arg(paperName_));
    resize(860, 640);
    auto* v = new QVBoxLayout(this);

    auto* intro = new QLabel(QStringLiteral(
        "GMG ColorProof / Fiery XF 两步墨限流程：先定各通道墨限（最大密度点），"
        "初始 TIL = 通道限之和；再打多列 TIL 靶，自动测量拐点 + 目检起皱/渗墨"
        "（双击表格行可将该列设为「视觉 TIL」——仪器测不出的问题以目检为准）。"));
    intro->setWordWrap(true);
    intro->setStyleSheet("color:#5D5A51;");
    v->addWidget(intro);

    // ── 第 1 步：单通道墨限（Fiery「Ink Limit per Channel」）──
    auto* chRow = new QHBoxLayout;
    auto* chHead = new QLabel(QStringLiteral("第 1 步 · 单通道墨限"));
    chHead->setStyleSheet("font-weight:700;color:#100A36;");
    chRow->addWidget(chHead);
    static const char* chName[4] = {"C", "M", "Y", "K"};
    for (int i = 0; i < 4; ++i) {
        chRow->addWidget(new QLabel(QString::fromLatin1(chName[i])));
        chLimit_[i] = new QSpinBox;
        chLimit_[i]->setRange(30, 100);
        chLimit_[i]->setValue(100);
        chLimit_[i]->setSuffix("%");
        chRow->addWidget(chLimit_[i]);
        connect(chLimit_[i], &QSpinBox::valueChanged, this,
                [this] { syncRangeToChannels(); });
    }
    chSum_ = new QLabel;
    chSum_->setStyleSheet("font-family:Menlo,monospace;font-weight:700;color:#0A6E8C;");
    chRow->addWidget(chSum_);
    auto* chHint = new QLabel(QStringLiteral("（打单通道渐变靶取最大色度/密度点；很多介质 ~50–80% 即达 Dmax）"));
    chHint->setStyleSheet("font-size:10.5px;color:#8A8779;");
    chRow->addWidget(chHint);
    chRow->addStretch();
    v->addLayout(chRow);

    auto* step2Head = new QLabel(QStringLiteral("第 2 步 · 总墨 TIL 靶"));
    step2Head->setStyleSheet("font-weight:700;color:#100A36;");
    v->addWidget(step2Head);

    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(QStringLiteral("介质类型")));
    mediaType_ = new QComboBox;
    for (const auto& mr : kMediaRefs)
        mediaType_->addItem(QStringLiteral("%1（参考 %2–%3%）")
                                .arg(QString::fromUtf8(mr.name)).arg(mr.lo).arg(mr.hi));
    mediaType_->setCurrentIndex(4);   // 喷墨艺术纸默认
    row->addWidget(mediaType_);
    auto mkSpin = [](int v, int lo, int hi, int step) {
        auto* s = new QSpinBox;
        s->setRange(lo, hi); s->setValue(v); s->setSingleStep(step); s->setSuffix("%");
        return s;
    };
    row->addWidget(new QLabel(QStringLiteral("起始")));
    row->addWidget(fromSpin_ = mkSpin(200, 100, 380, 10));
    row->addWidget(new QLabel(QStringLiteral("结束")));
    row->addWidget(toSpin_ = mkSpin(400, 220, 400, 10));
    row->addWidget(new QLabel(QStringLiteral("步长")));
    row->addWidget(stepSpin_ = mkSpin(20, 5, 50, 5));
    row->addStretch();
    v->addLayout(row);

    table_ = new QTableWidget;
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels(
        {QStringLiteral("列"), QStringLiteral("TAC %"), QStringLiteral("色样"),
         QStringLiteral("配方 C/M/Y/K"), QStringLiteral("实测 L*"),
         QStringLiteral("ΔL"), QStringLiteral("渗墨/晕染")});
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->addWidget(table_, 1);

    verdict_ = new QLabel(QStringLiteral("测量后在此给出建议 TIL。"));
    verdict_->setWordWrap(true);
    verdict_->setStyleSheet(
        "background:#E7F7FD;border:1px solid #A9E4F7;border-radius:6px;"
        "padding:9px 12px;color:#100A36;");
    v->addWidget(verdict_);

    auto* btns = new QHBoxLayout;
    auto* print = new QPushButton(QStringLiteral("打印测试靶"));
    auto* meas = new QPushButton(QStringLiteral("实测下一块 (i1)"));
    meas->setToolTip(QStringLiteral("把 i1 对准打印靶上的下一档色块按测量键；无仪器时用模拟读数演示"));
    applyBtn_ = new QPushButton(QStringLiteral("应用到打印环境"));
    applyBtn_->setEnabled(false);
    auto* close = new QPushButton(QStringLiteral("关闭"));
    btns->addWidget(print);
    btns->addWidget(meas);
    btns->addStretch();
    btns->addWidget(applyBtn_);
    btns->addWidget(close);
    v->addLayout(btns);

    connect(print, &QPushButton::clicked, this, &TILWizardDialog::printTarget);
    connect(meas, &QPushButton::clicked, this, &TILWizardDialog::measureNext);
    connect(applyBtn_, &QPushButton::clicked, this, &TILWizardDialog::applyToEnvironment);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    for (auto* s : {fromSpin_, toSpin_, stepSpin_})
        connect(s, &QSpinBox::valueChanged, this, [this] { rebuildRows(); });
    connect(mediaType_, &QComboBox::currentIndexChanged, this, [this] { analyze(); });
    // 双击行 = 视觉 TIL（Fiery：仪器测不出起皱/渗墨，视觉可否决自动值）
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int r, int) {
        visualCol_ = (visualCol_ == r) ? -1 : r;    // 再次双击取消
        analyze();
    });

    syncRangeToChannels();
}

void TILWizardDialog::syncRangeToChannels() {
    int sum = 0;
    for (auto* s : chLimit_) sum += s->value();
    chSum_->setText(QStringLiteral("Σ = %1%").arg(sum));
    // Fiery 语义：初始 TIL = 通道限之和；靶围绕它展开（0.7Σ … Σ）
    const QSignalBlocker b1(fromSpin_), b2(toSpin_);
    fromSpin_->setValue(qMax(100, int(sum * 0.7) / 10 * 10));
    toSpin_->setValue(qMin(400, sum));
    rebuildRows();
}

void TILWizardDialog::rebuildRows() {
    rows_.clear();
    visualCol_ = -1;
    const int from = fromSpin_->value();
    const int to = qMax(toSpin_->value(), from);
    const int step = qMax(5, stepSpin_->value());
    // 配方受第 1 步通道限约束：CMY 均衡填充到各自限，余量进 K（不超 K 限）
    const double capC = chLimit_[0]->value(), capM = chLimit_[1]->value();
    const double capY = chLimit_[2]->value(), capK = chLimit_[3]->value();
    for (int tac = from; tac <= to; tac += step) {
        Row r;
        r.tac = tac;
        double cmy = tac / 3.0;
        r.c = qMin(capC, cmy); r.m = qMin(capM, cmy); r.y = qMin(capY, cmy);
        r.k = qBound(0.0, double(tac) - (r.c + r.m + r.y), capK);
        rows_.push_back(r);
    }
    table_->setRowCount(rows_.size());
    for (int i = 0; i < rows_.size(); ++i) {
        const Row& r = rows_[i];
        table_->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));   // 列号
        table_->setItem(i, 1, new QTableWidgetItem(QString::number(r.tac)));
        auto* sw = new QLabel;
        sw->setStyleSheet(QStringLiteral("background:%1;margin:3px;border-radius:2px;")
                              .arg(previewColor(r.c, r.m, r.y, r.k).name()));
        table_->setCellWidget(i, 2, sw);
        table_->setItem(i, 3, new QTableWidgetItem(
            QStringLiteral("%1/%2/%3/%4").arg(r.c, 0, 'f', 0).arg(r.m, 0, 'f', 0)
                                          .arg(r.y, 0, 'f', 0).arg(r.k, 0, 'f', 0)));
        auto* lItem = new QTableWidgetItem(QStringLiteral("—"));
        lItem->setFlags(lItem->flags() | Qt::ItemIsEditable);   // 允许手输
        table_->setItem(i, 4, lItem);
        table_->setItem(i, 5, new QTableWidgetItem(QStringLiteral("—")));
        auto* chk = new QCheckBox;
        connect(chk, &QCheckBox::toggled, this, [this, i](bool on) {
            if (i < rows_.size()) { rows_[i].bleed = on; analyze(); }
        });
        table_->setCellWidget(i, 6, chk);
    }
    // 手输 L* 也参与分析
    connect(table_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* it) {
        if (!it || it->column() != 4) return;
        bool ok = false;
        const double L = it->text().toDouble(&ok);
        if (ok && it->row() < rows_.size()) { rows_[it->row()].L = L; analyze(); }
    });
    suggested_ = 0;
    applyBtn_->setEnabled(false);
    verdict_->setText(QStringLiteral("测量（或双击选视觉列）后在此给出建议 TIL。"));
    table_->resizeColumnsToContents();
}

void TILWizardDialog::printTarget() {
    // 关键前提与校准图一致：直通打印、无色彩管理、100% 不缩放。
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted) return;
    QPainter g(&printer);
    const QRectF page = printer.pageRect(QPrinter::DevicePixel);
    QFont f = g.font();
    f.setPointSizeF(13); f.setBold(true); g.setFont(f);
    g.drawText(QRectF(page.x(), page.y(), page.width(), 50), Qt::AlignLeft,
               QStringLiteral("TIL 测试靶 — %1（直通打印 · 无色彩管理）").arg(paperName_));
    f.setPointSizeF(8); f.setBold(false); g.setFont(f);
    const double top = page.y() + 70;
    const double pw = page.width() / rows_.size();
    for (int i = 0; i < rows_.size(); ++i) {
        const Row& r = rows_[i];
        const QRectF patch(page.x() + i * pw + 2, top, pw - 4, page.height() * 0.28);
        g.fillRect(patch, previewColor(r.c, r.m, r.y, r.k));
        g.setPen(Qt::black);
        g.drawText(QRectF(patch.x(), patch.y() - 28, patch.width(), 24),
                   Qt::AlignHCenter, QStringLiteral("列 %1").arg(i + 1));
        g.drawText(QRectF(patch.x(), patch.bottom() + 4, patch.width(), 26),
                   Qt::AlignHCenter, QStringLiteral("%1%").arg(r.tac));
        g.drawText(QRectF(patch.x(), patch.bottom() + 30, patch.width(), 26),
                   Qt::AlignHCenter,
                   QStringLiteral("%1/%2/%3/%4").arg(r.c, 0, 'f', 0).arg(r.m, 0, 'f', 0)
                                                 .arg(r.y, 0, 'f', 0).arg(r.k, 0, 'f', 0));
    }
    g.drawText(QRectF(page.x(), top + page.height() * 0.28 + 70, page.width(), 60),
               Qt::AlignLeft | Qt::TextWordWrap,
               QStringLiteral("晾干后评估：① 从左到右找到第一块「不再明显变暗」的色块；"
                              "② 检查渗墨/晕染/穿透起始块。回软件逐块实测 L* 并勾选。"));
}

void TILWizardDialog::measureNext() {
    int idx = -1;
    for (int i = 0; i < rows_.size(); ++i)
        if (rows_[i].L < 0) { idx = i; break; }
    if (idx < 0) { analyze(); return; }

    const Row target = rows_[idx];
    QPointer<TILWizardDialog> self(this);
    std::thread([self, target, idx] {
        double L = -1;
        bool real = false;
        auto& d = jcolour::i1sdk::I1Pro3Driver::instance();
        if (d.available() && (d.isOpen() || d.openFirst())) {
            d.setOption("MeasurementMode", "ReflectanceSpot");
            if (d.calibrate() && d.triggerMeasurement() && d.availableSamples() > 0) {
                std::array<float, 36> spec{};
                if (d.spectrum(spec, 0)) {
                    jcolour::spectral::Spectrum s{};
                    for (int i = 0; i < 36; ++i) s[size_t(i)] = double(spec[size_t(i)]);
                    L = jcolour::spectral::xyz_to_lab(
                            jcolour::spectral::spectrum_to_xyz(s)).L;
                    real = true;
                }
            }
        }
        if (!real) {
            // 模拟：L* 随 TAC 递降，~310% 后进入平台（Dmax 拐点），再往上微升
            const double tac = target.tac;
            const double base = 6.5 + 16.0 * std::exp(-(tac - 200.0) / 55.0);
            const double plateau = tac > 310 ? (tac - 310) * 0.004 : 0.0;
            L = base + plateau + (QRandomGenerator::global()->generateDouble() - 0.5) * 0.3;
        }
        QMetaObject::invokeMethod(self, [self, idx, L, real] {
            if (!self || idx >= self->rows_.size()) return;
            self->rows_[idx].L = L;
            auto* it = self->table_->item(idx, 4);
            const QSignalBlocker block(self->table_);
            it->setText(QString::number(L, 'f', 2) + (real ? "" : " *"));
            self->analyze();
        }, Qt::QueuedConnection);
    }).detach();
}

void TILWizardDialog::analyze() {
    // 自动 TIL：相邻列 L* 增益 < 0.5 → Dmax 已到，取上一列；渗墨勾选 →
    // 前一列为渗墨上限。视觉 TIL：操作员双击的列（Fiery：视觉可否决自动，
    // 因为仪器测不出起皱/渗墨）。建议 = 视觉优先，否则 min(拐点, 渗墨)。
    constexpr double kDmaxGain = 0.5;
    int kneeTac = 0, bleedTac = 0;
    for (int i = 0; i < rows_.size(); ++i) {
        QString dTxt = QStringLiteral("—");
        if (i > 0 && rows_[i].L >= 0 && rows_[i - 1].L >= 0) {
            const double dL = rows_[i - 1].L - rows_[i].L;   // 正值 = 仍在变暗
            dTxt = QString::number(dL, 'f', 2);
            if (kneeTac == 0 && dL < kDmaxGain)
                kneeTac = rows_[i - 1].tac;
        }
        const QSignalBlocker block(table_);
        table_->item(i, 5)->setText(dTxt);
        // 视觉列高亮（列号加框显示）
        table_->item(i, 0)->setText(i == visualCol_
            ? QStringLiteral("▶ %1").arg(i + 1) : QString::number(i + 1));
        if (bleedTac == 0 && rows_[i].bleed)
            bleedTac = i > 0 ? rows_[i - 1].tac : rows_[i].tac;
    }
    const int visualTac = (visualCol_ >= 0 && visualCol_ < rows_.size())
                              ? rows_[visualCol_].tac : 0;
    if (kneeTac == 0 && bleedTac == 0 && visualTac == 0) {
        verdict_->setText(QStringLiteral("继续实测（或双击选视觉列）：尚未出现拐点/渗墨/视觉选择。"));
        applyBtn_->setEnabled(false);
        suggested_ = 0;
        return;
    }
    const int autoTac = kneeTac && bleedTac ? qMin(kneeTac, bleedTac)
                       : kneeTac ? kneeTac : bleedTac;
    suggested_ = visualTac ? visualTac : autoTac;

    const auto& mr = kMediaRefs[mediaType_->currentIndex()];
    QString refNote;
    if (suggested_ < mr.lo)
        refNote = QStringLiteral("低于该介质参考区间 %1–%2%（正常：具体墨水/环境差异）。").arg(mr.lo).arg(mr.hi);
    else if (suggested_ > mr.hi)
        refNote = QStringLiteral("高于该介质参考区间 %1–%2%，建议复核渗墨与干燥。").arg(mr.lo).arg(mr.hi);
    else
        refNote = QStringLiteral("落在该介质参考区间 %1–%2% 内。").arg(mr.lo).arg(mr.hi);

    QString basis;
    if (visualTac)
        basis = QStringLiteral("视觉 TIL（列 %1 = %2%，优先于自动%3）")
            .arg(visualCol_ + 1).arg(visualTac)
            .arg(autoTac ? QStringLiteral("；自动值 %1%").arg(autoTac) : QString());
    else
        basis = QStringLiteral("%1%2")
            .arg(kneeTac ? QStringLiteral("Dmax 拐点 %1%").arg(kneeTac) : QString())
            .arg(bleedTac ? QStringLiteral("%1渗墨上限 %2%")
                    .arg(kneeTac ? QStringLiteral(" · ") : QString()).arg(bleedTac)
                          : QString());
    verdict_->setText(QStringLiteral("<b>建议 TIL = %1%</b>　依据：%2　%3")
                          .arg(suggested_).arg(basis).arg(refNote));
    applyBtn_->setEnabled(true);
}

void TILWizardDialog::applyToEnvironment() {
    if (suggested_ <= 0) return;
    QSettings s;
    s.setValue(QStringLiteral("til/%1").arg(paperName_), suggested_);
    s.setValue(QStringLiteral("til/%1_at").arg(paperName_),
               QDateTime::currentDateTime().toString(Qt::ISODate));
    verdict_->setText(verdict_->text() +
        QStringLiteral("　<b style='color:#2F9E6B'>✓ 已写入「%1」打印环境</b>").arg(paperName_));
}

}  // namespace jcolour::ui
