#include "measure_widget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QTimer>

#include <cmath>

namespace jcolour {

namespace {

constexpr int kRowH = 20, kNumW = 30, kStW = 30, kGap = 2;

// 朴素 CMYK→RGB：未测色块的「预期颜色」淡显示（仅界面渲染，非引擎色彩管线）
QColor previewColor(const MeasurePatch& p) {
    auto f = [](float v) { return 1.0f - v / 100.0f; };
    const float kk = f(p.k);
    return QColor::fromRgbF(f(p.c) * kk, f(p.m) * kk, f(p.y) * kk);
}

// Lab(D50) → sRGB：实测颜色上屏
QColor labColor(double L, double a, double b) {
    const double fy = (L + 16) / 116, fx = fy + a / 500, fz = fy - b / 200;
    auto inv = [](double t) { return t * t * t > 0.008856 ? t * t * t : (t - 16.0 / 116) / 7.787; };
    double X = inv(fx) * 0.9642, Y = inv(fy), Z = inv(fz) * 0.8249;
    const double X2 = X * 0.9555766 + Y * -0.0230393 + Z * 0.0631636;
    const double Y2 = X * -0.0282895 + Y * 1.0099416 + Z * 0.0210077;
    const double Z2 = X * 0.0122982 + Y * -0.0204830 + Z * 1.3299098;
    double r  = X2 * 3.2404542 + Y2 * -1.5371385 + Z2 * -0.4985314;
    double g  = X2 * -0.9692660 + Y2 * 1.8760108 + Z2 * 0.0415560;
    double bl = X2 * 0.0556434 + Y2 * -0.2040259 + Z2 * 1.0572252;
    auto gam = [](double v) {
        v = qBound(0.0, v, 1.0);
        return v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1 / 2.4) - 0.055;
    };
    return QColor::fromRgbF(float(gam(r)), float(gam(g)), float(gam(bl)));
}

// Mock 用：预期颜色 → Lab（加噪声前的基准）
QVector3D previewLab(const MeasurePatch& p) {
    const QColor c = previewColor(p);
    auto lin = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    const double R = lin(c.redF()), G = lin(c.greenF()), B = lin(c.blueF());
    const double X = (R * 0.4124 + G * 0.3576 + B * 0.1805) / 0.9505;
    const double Y =  R * 0.2126 + G * 0.7152 + B * 0.0722;
    const double Z = (R * 0.0193 + G * 0.1192 + B * 0.9505) / 1.089;
    auto f = [](double t) { return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116; };
    return { float(116 * f(Y) - 16), float(500 * (f(X) - f(Y))), float(200 * (f(Y) - f(Z))) };
}

}  // namespace

// ── MockMeasureDriver ───────────────────────────────────────────────

void MockMeasureDriver::whiteCal(std::function<void(bool)> done) {
    QTimer::singleShot(900, this, [this, done] { calibrated_ = true; done(true); });
}

void MockMeasureDriver::readStrip(int, const QVector<MeasurePatch>& patches,
                                  std::function<void(QVector<QVector3D>)> ok,
                                  std::function<void(MeasureError)> fail) {
    const int delay = 500 + int(QRandomGenerator::global()->bounded(500));
    QTimer::singleShot(delay, this, [this, patches, ok, fail] {
        auto* rng = QRandomGenerator::global();
        if (!calibrated_) {
            fail({ "NEEDS_CAL", QStringLiteral("仪器尚未白校准"),
                   QStringLiteral("把仪器放到白瓷砖上按「白色校准」") });
            return;
        }
        if (rng->generateDouble() < 0.15) {
            static const MeasureError errs[] = {
                { "TOO_FAST",    QStringLiteral("拉动过快，色块识别失败"),
                                 QStringLiteral("匀速重扫本条（约 3–4 秒扫完）") },
                { "WRONG_STRIP", QStringLiteral("读到的色块数与本条不符"),
                                 QStringLiteral("确认扫的是高亮行，方向从左到右") },
                { "INCOMPLETE",  QStringLiteral("扫描中断，本条数据不完整"),
                                 QStringLiteral("从条头重新扫到条尾") },
            };
            fail(errs[rng->bounded(3)]);
            return;
        }
        QVector<QVector3D> lab;
        lab.reserve(patches.size());
        for (const auto& p : patches) {
            QVector3D v = previewLab(p);
            auto n = [rng] { return float(rng->generateDouble() - 0.5) * 1.6f; };
            lab.push_back({ v.x() + n(), v.y() + n(), v.z() + n() });
        }
        ok(lab);
    });
}

// ── MeasureChartArea ────────────────────────────────────────────────

MeasureChartArea::MeasureChartArea(MeasureWidget* owner) : QWidget(owner), w_(owner) {
    setMinimumHeight(owner->nStrips_ * (kRowH + kGap) + kGap);
}

QSize MeasureChartArea::minimumSizeHint() const {
    return { 400, w_->nStrips_ * (kRowH + kGap) + kGap };
}

void MeasureChartArea::paintEvent(QPaintEvent*) {
    QPainter pt(this);
    pt.fillRect(rect(), QColor("#F1EFE8"));
    const int patchW = (width() - kNumW - kStW - 8) / w_->nPatches_;

    for (int si = 0; si < w_->nStrips_; ++si) {
        const int y = kGap + si * (kRowH + kGap);
        const auto& st = w_->strips_[si];
        QRect row(2, y, width() - 4, kRowH);

        // 行背景：当前条蓝框，失败条红底
        if (st.state == MeasureStripState::S::Failed)
            pt.fillRect(row, QColor("#fbe8e8"));
        if (si == w_->cur_) {
            pt.fillRect(row, QColor("#E7F7FD"));
            pt.setPen(QPen(QColor("#0BBAEE"), 1.5));
            pt.drawRect(row.adjusted(0, 0, -1, -1));
        }

        // 序号
        pt.setPen(si == w_->cur_ ? QColor("#0A6E8C") : QColor("#888"));
        pt.drawText(QRect(4, y, kNumW - 4, kRowH), Qt::AlignVCenter | Qt::AlignRight,
                    QString("%1").arg(si + 1, 2, 10, QChar('0')));

        // 色块：已测 → 实测 Lab 实色；未测 → 预期色 22% 淡显
        const bool ok = st.state == MeasureStripState::S::Ok;
        for (int pi = 0; pi < w_->nPatches_; ++pi) {
            const auto& p = w_->chart_[si][pi];
            QColor c = ok && p.measured ? labColor(p.L, p.a, p.b) : previewColor(p);
            if (!(ok && p.measured)) c.setAlphaF(0.22f);
            pt.fillRect(QRect(kNumW + 4 + pi * patchW, y + 2, patchW - 1, kRowH - 4), c);
        }

        // 状态
        const int stx = width() - kStW;
        pt.setPen(QColor("#2a8c2a"));
        if (st.state == MeasureStripState::S::Ok)
            pt.drawText(QRect(stx, y, kStW - 4, kRowH), Qt::AlignCenter, QStringLiteral("✓"));
        else if (st.state == MeasureStripState::S::Reading) {
            pt.setPen(QColor("#0BBAEE"));
            pt.drawText(QRect(stx, y, kStW - 4, kRowH), Qt::AlignCenter, QStringLiteral("…"));
        } else if (st.state == MeasureStripState::S::Failed) {
            pt.setPen(QColor("#d42020"));
            pt.drawText(QRect(stx, y, kStW - 4, kRowH), Qt::AlignCenter, QStringLiteral("✕"));
        }
    }
}

void MeasureChartArea::mousePressEvent(QMouseEvent* e) {
    const int si = (e->pos().y() - kGap) / (kRowH + kGap);
    if (si < 0 || si >= w_->nStrips_) return;
    w_->cur_ = si;
    // 点击已测色块 → 读数检视
    const int patchW = (width() - kNumW - kStW - 8) / w_->nPatches_;
    const int pi = (e->pos().x() - kNumW - 4) / qMax(1, patchW);
    if (pi >= 0 && pi < w_->nPatches_ && w_->strips_[si].state == MeasureStripState::S::Ok) {
        w_->inspectStrip_ = si;
        w_->inspectPatch_ = pi;
    }
    w_->refreshUi();
    w_->setFocus();
}

// ── MeasureWidget ───────────────────────────────────────────────────

MeasureWidget::MeasureWidget(const QString& chartId, int strips, int patchesPerStrip,
                             MeasureDriver* driver, QWidget* parent)
    : QWidget(parent), chartId_(chartId), nStrips_(strips), nPatches_(patchesPerStrip),
      driver_(driver) {
    setFocusPolicy(Qt::StrongFocus);

    // 确定性伪随机图表（同一 chartId 同一张图；正式版由引擎按校准集下发）
    quint32 seed = 7;
    for (QChar ch : chartId) seed = seed * 31 + ch.unicode();
    QRandomGenerator rng(seed);
    chart_.resize(nStrips_);
    for (auto& row : chart_) {
        row.resize(nPatches_);
        for (auto& p : row) {
            const double r = rng.generateDouble();
            auto rv = [&rng] { return float(rng.bounded(101)); };
            if (r < 0.08) { p.k = rv(); }
            else if (r < 0.16) { p.c = p.m = p.y = rv(); }
            else {
                auto maybe = [&rng, &rv] { return rng.generateDouble() < 0.4 ? 0.0f : rv(); };
                p.c = maybe(); p.m = maybe(); p.y = maybe(); p.k = maybe();
            }
        }
    }
    strips_.resize(nStrips_);

    auto* l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(8);

    // 仪器行
    auto* devRow = new QHBoxLayout;
    devLabel_ = new QLabel(driver_ ? driver_->name() : QStringLiteral("未连接仪器"));
    devLabel_->setStyleSheet("font-size:11.5px;color:#555;background:transparent;");
    calBtn_ = new QPushButton(QStringLiteral("白色校准 (W)"));
    calBtn_->setCursor(Qt::PointingHandCursor);
    connect(calBtn_, &QPushButton::clicked, this, &MeasureWidget::whiteCal);
    devRow->addWidget(devLabel_);
    devRow->addStretch();
    devRow->addWidget(calBtn_);
    l->addLayout(devRow);

    area_ = new MeasureChartArea(this);
    l->addWidget(area_);

    errBanner_ = new QLabel;
    errBanner_->setWordWrap(true);
    errBanner_->setStyleSheet(
        "font-size:11.5px;color:#8a1a1a;background:#fbe8e8;border:1px solid #e8b8b8;"
        "border-radius:6px;padding:7px 10px;");
    errBanner_->setVisible(false);
    l->addWidget(errBanner_);

    inspect_ = new QLabel;
    inspect_->setStyleSheet(
        "font-size:11px;font-family:Menlo,monospace;color:#4A4740;background:#fff;"
        "border:1px solid #DDD9CF;border-radius:6px;padding:5px 10px;");
    inspect_->setVisible(false);
    l->addWidget(inspect_);

    progress_ = new QProgressBar;
    progress_->setRange(0, nStrips_);
    progress_->setTextVisible(false);
    progress_->setFixedHeight(8);
    l->addWidget(progress_);
    progressText_ = new QLabel;
    progressText_->setAlignment(Qt::AlignCenter);
    progressText_->setStyleSheet("font-size:10.5px;color:#888;background:transparent;");
    l->addWidget(progressText_);

    auto* btnRow = new QHBoxLayout;
    readBtn_ = new QPushButton;
    readBtn_->setCursor(Qt::PointingHandCursor);
    readBtn_->setStyleSheet(
        "QPushButton{background:#0BBAEE;color:#fff;border:none;border-radius:7px;"
        "padding:9px 16px;font-size:12.5px;font-weight:600;}"
        "QPushButton:disabled{background:#b8c4d0;}");
    connect(readBtn_, &QPushButton::clicked, this, &MeasureWidget::readCurrent);
    rescanBtn_ = new QPushButton(QStringLiteral("重扫 (R)"));
    skipBtn_   = new QPushButton(QStringLiteral("跳过"));
    for (auto* b : { rescanBtn_, skipBtn_ }) b->setCursor(Qt::PointingHandCursor);
    connect(rescanBtn_, &QPushButton::clicked, this, [this] { rescan(cur_); });
    connect(skipBtn_, &QPushButton::clicked, this, [this] {
        for (int i = cur_ + 1; i < nStrips_; ++i)
            if (strips_[i].state != MeasureStripState::S::Ok) { cur_ = i; break; }
        refreshUi();
    });
    btnRow->addWidget(readBtn_, 1);
    btnRow->addWidget(rescanBtn_);
    btnRow->addWidget(skipBtn_);
    l->addLayout(btnRow);

    auto* hint = new QLabel(QStringLiteral(
        "Enter 读取 · ↑↓ 或 F/B 换条 · R 重扫 · W 白校准 · 点击已测色块看 Lab 读数"));
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("font-size:10px;color:#999;background:transparent;");
    l->addWidget(hint);

    tryResume();
    refreshUi();
}

bool MeasureWidget::complete() const {
    for (const auto& s : strips_)
        if (s.state != MeasureStripState::S::Ok) return false;
    return true;
}

void MeasureWidget::whiteCal() {
    if (!driver_ || busy_) return;
    busy_ = true;
    calBtn_->setText(QStringLiteral("校准中…"));
    calBtn_->setEnabled(false);
    driver_->whiteCal([this](bool ok) {
        busy_ = false;
        calibrated_ = ok;
        calBtn_->setText(ok ? QStringLiteral("✓ 已白校准") : QStringLiteral("白色校准 (W)"));
        calBtn_->setEnabled(!ok);
        refreshUi();
    });
}

void MeasureWidget::readCurrent() {
    if (!driver_ || busy_ || !calibrated_) return;
    const int si = cur_;
    if (strips_[si].state == MeasureStripState::S::Ok) return;  // 已测须显式重扫
    busy_ = true;
    strips_[si].state = MeasureStripState::S::Reading;
    strips_[si].errMsg.clear();
    refreshUi();
    driver_->readStrip(si, chart_[si],
        [this, si](QVector<QVector3D> lab) {
            for (int pi = 0; pi < nPatches_ && pi < lab.size(); ++pi) {
                auto& p = chart_[si][pi];
                p.measured = true;
                p.L = lab[pi].x(); p.a = lab[pi].y(); p.b = lab[pi].z();
            }
            strips_[si].state = MeasureStripState::S::Ok;
            busy_ = false;
            persist();
            // 自动跳到下一条未测的（末尾折返）
            int next = -1;
            for (int i = si + 1; i < nStrips_; ++i)
                if (strips_[i].state != MeasureStripState::S::Ok) { next = i; break; }
            if (next < 0)
                for (int i = 0; i < nStrips_; ++i)
                    if (strips_[i].state != MeasureStripState::S::Ok) { next = i; break; }
            if (next >= 0) cur_ = next;
            refreshUi();
            if (complete()) emit completed();
        },
        [this, si](MeasureError e) {
            strips_[si].state = MeasureStripState::S::Failed;
            strips_[si].errMsg = e.msg;
            strips_[si].errFix = e.fix;
            busy_ = false;
            persist();
            refreshUi();     // 停在本条等重扫
        });
}

void MeasureWidget::rescan(int strip) {
    if (strip < 0 || strip >= nStrips_) return;
    strips_[strip] = {};
    for (auto& p : chart_[strip]) p.measured = false;
    cur_ = strip;
    refreshUi();
}

void MeasureWidget::persist() {
    QSettings s;
    s.beginGroup("measure/" + chartId_);
    for (int si = 0; si < nStrips_; ++si) {
        if (strips_[si].state != MeasureStripState::S::Ok) continue;
        QVariantList lab;
        for (const auto& p : chart_[si]) { lab << p.L << p.a << p.b; }
        s.setValue(QString("strip%1").arg(si), lab);
    }
    s.endGroup();
}

void MeasureWidget::tryResume() {
    QSettings s;
    s.beginGroup("measure/" + chartId_);
    for (int si = 0; si < nStrips_; ++si) {
        const QVariantList lab = s.value(QString("strip%1").arg(si)).toList();
        if (lab.size() != nPatches_ * 3) continue;
        for (int pi = 0; pi < nPatches_; ++pi) {
            auto& p = chart_[si][pi];
            p.measured = true;
            p.L = lab[pi * 3].toDouble();
            p.a = lab[pi * 3 + 1].toDouble();
            p.b = lab[pi * 3 + 2].toDouble();
        }
        strips_[si].state = MeasureStripState::S::Ok;
    }
    s.endGroup();
    for (int i = 0; i < nStrips_; ++i)
        if (strips_[i].state != MeasureStripState::S::Ok) { cur_ = i; break; }
}

void MeasureWidget::refreshUi() {
    int done = 0;
    for (const auto& s : strips_) if (s.state == MeasureStripState::S::Ok) ++done;
    progress_->setValue(done);
    int failed = 0;
    for (const auto& s : strips_) if (s.state == MeasureStripState::S::Failed) ++failed;
    progressText_->setText(failed
        ? QStringLiteral("%1/%2 条完成 · %3 条失败待重扫").arg(done).arg(nStrips_).arg(failed)
        : QStringLiteral("%1/%2 条完成").arg(done).arg(nStrips_));

    const auto& st = strips_[cur_];
    errBanner_->setVisible(st.state == MeasureStripState::S::Failed);
    if (st.state == MeasureStripState::S::Failed)
        errBanner_->setText(QStringLiteral("<b>%1</b>　%2 —— 按 Enter 重扫本条")
                                .arg(st.errMsg, st.errFix));

    if (inspectStrip_ >= 0) {
        const auto& p = chart_[inspectStrip_][inspectPatch_];
        inspect_->setText(QStringLiteral("条 %1 · 块 %2　CMYK %3/%4/%5/%6　→　L* %7　a* %8　b* %9")
            .arg(inspectStrip_ + 1).arg(inspectPatch_ + 1)
            .arg(int(p.c)).arg(int(p.m)).arg(int(p.y)).arg(int(p.k))
            .arg(p.L, 0, 'f', 1).arg(p.a, 0, 'f', 1).arg(p.b, 0, 'f', 1));
        inspect_->setVisible(true);
    }

    readBtn_->setEnabled(calibrated_ && !busy_ && !complete());
    readBtn_->setText(complete() ? QStringLiteral("✓ 全部完成")
                     : busy_     ? QStringLiteral("读取中…")
                                 : QStringLiteral("▶ 读取第 %1 条 (Enter)").arg(cur_ + 1));
    area_->update();
}

void MeasureWidget::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
        case Qt::Key_Return: case Qt::Key_Enter: case Qt::Key_Space:
            readCurrent(); return;
        case Qt::Key_Down: case Qt::Key_F:
            cur_ = qMin(nStrips_ - 1, cur_ + 1); refreshUi(); return;
        case Qt::Key_Up: case Qt::Key_B:
            cur_ = qMax(0, cur_ - 1); refreshUi(); return;
        case Qt::Key_R: rescan(cur_); return;
        case Qt::Key_W: whiteCal(); return;
        default: QWidget::keyPressEvent(e);
    }
}

}  // namespace jcolour
