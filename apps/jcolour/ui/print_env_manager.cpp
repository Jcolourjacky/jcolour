#include "print_env_manager.h"

#include "theme.h"
#include "state_machine.h"  // engine: jcolour::calibration
#include "calibration_stepper.h"
#include "../paper_state.h"

#include <QtWidgets>
#include <QDateTime>

namespace jcolour::ui {

namespace cal = jcolour::calibration;

namespace {

struct ModeRow {
    QString name;    // 单黑 / 黑+灰 / ...
    QString depth;   // 线性 / 线性 + G7 / 线性 + G7 + ICC
    QString lin;     // 2880×1440 · 双向  (resolution + direction)
    int     days;    // age of this mode's linearization curve
};

// Map the engine CalibState → (badge text, colour). Driven by the real
// compute_state() thresholds (>30d soft, >90d hard).
QString stateBadge(cal::CalibState s, int days, QString& colour) {
    switch (s) {
        case cal::CalibState::FRESH:      colour = "#2F9E6B"; return QStringLiteral("✓ %1 天").arg(days);
        case cal::CalibState::STALE_SOFT: colour = "#B58900"; return QStringLiteral("%1 天 · 需检查").arg(days);
        case cal::CalibState::STALE_HARD: colour = "#c0392b"; return QStringLiteral("%1 天 · 重做线性").arg(days);
        default:                          colour = "#8A8779"; return QStringLiteral("未校准");
    }
}

QLabel* mk(const QString& text, const QString& css = QString()) {
    auto* l = new QLabel(text);
    if (!css.isEmpty()) l->setStyleSheet(css);
    return l;
}

QFrame* card() {
    auto* f = new QFrame;
    f->setObjectName("card");
    f->setStyleSheet("QFrame#card{background:#fff;border:1px solid #DDD9CF;border-radius:8px;}");
    return f;
}

}  // namespace

PrintEnvironmentManager::PrintEnvironmentManager(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("打印环境管理器"));
    setStyleSheet(jcolour::theme::pageStyle());
    resize(780, 560);

    // ---- engine inputs (the {printer × paper × ink-batch} this env covers) ----
    const cal::Timestamp now = QDateTime::currentSecsSinceEpoch();
    const cal::InkBatch ink{"A7"};
    const cal::Paper    paper{"hahnemuhle_photo_rag_308"};

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- header ----
    auto* hd = mk(QStringLiteral("打印环境管理器   ·   机 + 纸 + 线性化 + ICC + 总墨量 全部内嵌在一个环境里"),
                  "padding:10px 14px;font-weight:700;color:#100A36;"
                  "background:#ececee;border-bottom:1px solid #DDD9CF;");
    root->addWidget(hd);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    root->addLayout(body, 1);

    // ---- left: environment list ----
    auto* left = new QWidget;
    left->setFixedWidth(236);
    left->setStyleSheet("background:#F1EFE8;border-right:1px solid #DDD9CF;");
    auto* ll = new QVBoxLayout(left);
    ll->setContentsMargins(8, 8, 8, 8);
    ll->setSpacing(6);

    auto* newBtn = new QPushButton(QStringLiteral("＋ 新建 / 校准向导"));
    newBtn->setObjectName("primary");
    connect(newBtn, &QPushButton::clicked, this, &PrintEnvironmentManager::startNewCalibration);
    ll->addWidget(newBtn);

    // 环境列表 = 真实配置库（PaperState）；rebuildEnvList() 负责填充
    listLayout_ = new QVBoxLayout;
    listLayout_->setContentsMargins(0, 0, 0, 0);
    listLayout_->setSpacing(6);
    ll->addLayout(listLayout_);
    ll->addStretch(1);
    body->addWidget(left);
    rebuildEnvList();

    // ---- right: detail (base calibration + 5-mode table) ----
    auto* right = new QWidget;
    auto* rl = new QVBoxLayout(right);
    rl->setContentsMargins(16, 14, 16, 14);
    rl->setSpacing(8);
    body->addWidget(right, 1);

    rl->addWidget(mk(QStringLiteral("Epson SC-P9500 · Hahnemühle Photo Rag 308"),
                     "font-size:13px;font-weight:700;color:#100A36;"));
    rl->addWidget(mk(QStringLiteral("墨批 #A7 · 这台机 ＋ 这种纸 的校准"),
                     "font-size:10px;color:#7A776D;"));

    // base calibration card
    {
        auto* c = card();
        auto* cl = new QVBoxLayout(c);
        cl->setContentsMargins(12, 10, 12, 10);
        cl->setSpacing(4);
        cl->addWidget(mk(QStringLiteral("基础校准（打印机 ＋ 纸张 共用）"),
                         "font-weight:700;font-size:11px;border:none;background:transparent;"));
        const char* rows[][2] = {
            {"① 配置打印机", "端口 RAW 9100(PJL) · 墨水分配 ✓（分辨率/方向在线性化时选）"},
            {"② 走纸米尺",   "打米尺量实长 → +0.3mm 已确认 ✓"},
            {"③ 总墨量 TIL", "按本纸 → 260% · 无晕染 ✓"},
        };
        for (auto& r : rows) {
            auto* rw = new QHBoxLayout;
            rw->addWidget(mk(QString::fromUtf8(r[0]), "font-size:10px;color:#7A776D;border:none;background:transparent;"));
            rw->addStretch(1);
            rw->addWidget(mk(QString::fromUtf8(r[1]), "font-size:10px;color:#2F9E6B;border:none;background:transparent;"));
            cl->addLayout(rw);
        }
        rl->addWidget(c);
    }

    // 5-mode table card — freshness from the engine
    {
        auto* c = card();
        auto* cl = new QVBoxLayout(c);
        cl->setContentsMargins(12, 10, 12, 10);
        cl->setSpacing(2);
        cl->addWidget(mk(QStringLiteral("5 种打印模式 · 线性曲线（绑定 分辨率＋方向）"),
                         "font-weight:700;font-size:11px;border:none;background:transparent;"));

        const ModeRow modes[] = {
            {"单黑",    "线性",            "1440×720 · 单向",   9},
            {"黑+灰",   "线性",            "2880×1440 · 双向",  9},
            {"黑+灰+彩", "线性 + G7",       "2880×1440 · 双向",  9},
            {"RGB",     "线性 + G7 + ICC", "2880×1440 · 双向",  35},
            {"16位RGB", "线性 + G7 + ICC", "2880×1440 · 单向",  9},
        };
        for (const auto& m : modes) {
            // Build an engine ParamSet for this mode and ask the engine.
            cal::ParamSet ps;
            ps.state         = cal::CalibState::FRESH;  // a curve exists; freshness is by age
            ps.key           = {ink.id, paper.id};
            ps.calibrated_at = now - static_cast<cal::Timestamp>(m.days) * 86400;
            const cal::CalibState st = cal::compute_state(ps, now, ink, paper);

            QString colour;
            const QString badge = stateBadge(st, m.days, colour);

            auto* rw = new QHBoxLayout;
            rw->setSpacing(8);
            auto* nm = mk(m.name, "font-size:10.5px;font-weight:700;color:#100A36;border:none;background:transparent;");
            nm->setFixedWidth(62);
            rw->addWidget(nm);
            rw->addWidget(mk(QStringLiteral("<b>%1</b> · %2").arg(m.lin, m.depth),
                             "font-size:9.5px;color:#7A776D;border:none;background:transparent;"), 1);
            rw->addWidget(mk(badge, QString("font-size:9px;font-weight:700;color:%1;border:none;background:transparent;").arg(colour)));
            auto* chk = new QPushButton(QStringLiteral("检查"));
            chk->setFixedHeight(20);
            chk->setStyleSheet("font-size:9px;padding:0 8px;");
            rw->addWidget(chk);

            auto* line = new QFrame;
            auto* lw = new QVBoxLayout(line);
            lw->setContentsMargins(0, 5, 0, 5);
            lw->addLayout(rw);
            line->setStyleSheet("QFrame{border:none;border-bottom:1px solid #EAE7DE;}");
            cl->addWidget(line);
        }
        cl->addWidget(mk(QStringLiteral("每条线性曲线<b>绑定分辨率＋方向</b>，写进名字（如 单黑·1440×720·单向）。"
                                        "选中哪条就用那套参数打印。超过 <b>1 个月</b>未用先「检查」（主要查线性），不行就重做线性。"),
                         "font-size:9px;color:#7A776D;border:none;background:transparent;"));
        rl->addWidget(c);
    }

    rl->addStretch(1);

    // footer
    auto* foot = new QHBoxLayout;
    foot->addWidget(new QPushButton(QStringLiteral("全部检查")));
    foot->addWidget(new QPushButton(QStringLiteral("重做线性")));
    foot->addStretch(1);
    auto* ok = new QPushButton(QStringLiteral("套用并关闭"));
    ok->setObjectName("primary");
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    foot->addWidget(ok);
    rl->addLayout(foot);
}

void PrintEnvironmentManager::startNewCalibration() {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("新建打印环境 · 校准向导"));
    dlg.resize(920, 640);
    auto* l = new QVBoxLayout(&dlg);
    l->setContentsMargins(0, 0, 0, 0);
    auto* stepper = new CalibrationStepper(&dlg);
    stepper->resetForNew();
    l->addWidget(stepper);
    connect(stepper, &CalibrationStepper::finished,  &dlg, &QDialog::accept);
    connect(stepper, &CalibrationStepper::cancelled, &dlg, &QDialog::reject);
    dlg.exec();
    rebuildEnvList();   // 存為標準後（PaperState 已落盘）刷新左列
}

void PrintEnvironmentManager::rebuildEnvList() {
    while (auto* item = listLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const auto configs = PaperState::instance().listConfigs();
    if (configs.isEmpty()) {
        listLayout_->addWidget(mk(QStringLiteral("尚无打印环境。\n点上方「新建 / 校准向导」建立第一个。"),
            "font-size:10px;color:#8A8779;border:none;background:transparent;padding:6px 2px;"));
        return;
    }
    const cal::Timestamp now = QDateTime::currentSecsSinceEpoch();
    bool first = true;
    for (const auto& c : configs) {
        // 徽章走真实引擎状态（按天龄 + 墨批变化判定）
        cal::ParamSet ps;
        ps.state         = cal::CalibState::FRESH;
        ps.key           = {c.inkBatch.toStdString(), c.paperId.toStdString()};
        ps.calibrated_at = c.lastCalibrated.isValid() ? c.lastCalibrated.toSecsSinceEpoch() : 0;
        const int days = c.lastCalibrated.isValid()
            ? int(c.lastCalibrated.daysTo(QDateTime::currentDateTime())) : 9999;
        const cal::CalibState st = cal::compute_state(
            ps, now, cal::InkBatch{c.inkBatch.toStdString()}, cal::Paper{c.paperId.toStdString()});
        QString colour;
        const QString badge = stateBadge(st, days, colour);

        auto* it = new QFrame;
        it->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:6px;}")
                              .arg(first ? "#E7F7FD" : "#ffffff", first ? "#bcd4f2" : "#e2e4e7"));
        auto* il = new QVBoxLayout(it);
        il->setContentsMargins(8, 6, 8, 6);
        il->setSpacing(1);
        il->addWidget(mk(QStringLiteral("%1 · %2").arg(c.printer, c.paperName),
            "font-size:11px;font-weight:700;color:#100A36;border:none;background:transparent;"));
        il->addWidget(mk(QStringLiteral("%1 · 墨批 %2 · v%3").arg(c.paperMeta, c.inkBatch).arg(c.version),
            "font-size:9px;color:#7A776D;border:none;background:transparent;"));
        il->addWidget(mk(badge,
            QString("font-size:9px;font-weight:600;color:%1;border:none;background:transparent;").arg(colour)));
        listLayout_->addWidget(it);
        first = false;
    }
}

}  // namespace jcolour::ui
