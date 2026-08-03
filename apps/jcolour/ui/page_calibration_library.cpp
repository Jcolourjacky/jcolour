#include "page_calibration_library.h"
#include "../paper_state.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QDateTime>
#include <QToolButton>
#include <QMessageBox>

namespace jcolour {

namespace {
constexpr const char* kAccent = "#2e7dd7";
constexpr const char* kOk      = "#2f8c4f";

QString daysAgo(const QDateTime& dt) {
    if (!dt.isValid()) return QStringLiteral("—");
    int d = dt.daysTo(QDateTime::currentDateTime());
    if (d <= 0) return QStringLiteral("今天");
    return QStringLiteral("%1 天前").arg(d);
}

// status badge text + colors, driven by config freshness / batch.
struct Badge { QString text; QString bg; QString fg; };
Badge badgeFor(const PaperConfig& c) {
    int days = c.lastCalibrated.daysTo(QDateTime::currentDateTime());
    if (!c.enabled)            return {QStringLiteral("⊘ 已停用"), "#eceef0", "#9aa0a8"};
    if (c.inkBatch == "#A6")   return {QStringLiteral("● 墨批已變"), "#fbe9e7", "#9a2418"};
    if (days >= 7)             return {QStringLiteral("● 略過期"),  "#fbf2dd", "#8a6310"};
    return {QStringLiteral("● READY"), "#e8f5ec", "#1d6b39"};
}
}  // namespace

CalibrationLibraryPage::CalibrationLibraryPage(QWidget* parent) : QWidget(parent) {
    setStyleSheet("background:#f6f7f9;");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // header
    auto* head = new QWidget;
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(30, 22, 30, 14);
    auto* titleBox = new QVBoxLayout;
    auto* title = new QLabel(QStringLiteral("建檔校準"));
    title->setStyleSheet("font-size:21px;font-weight:700;color:#23262b;");
    auto* sub = new QLabel(QStringLiteral("已建好的紙張標準 · 列印時向這些標準對齊 · 開關控制哪些配置起作用"));
    sub->setStyleSheet("font-size:12px;color:#868c95;");
    titleBox->addWidget(title); titleBox->addWidget(sub);
    hl->addLayout(titleBox);
    hl->addStretch();
    auto* newBtn = new QPushButton(QStringLiteral("＋ 新建校準"));
    newBtn->setCursor(Qt::PointingHandCursor);
    newBtn->setStyleSheet(QString("QPushButton{background:%1;color:#fff;border:none;border-radius:8px;"
                                  "padding:9px 16px;font-size:13px;font-weight:600;}").arg(kAccent));
    connect(newBtn, &QPushButton::clicked, this, &CalibrationLibraryPage::newCalibrationRequested);
    hl->addWidget(newBtn);
    root->addWidget(head);

    // scrollable card list
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea{background:#f6f7f9;}");
    auto* listHost = new QWidget;
    listHost->setStyleSheet("background:#f6f7f9;");
    listLayout_ = new QVBoxLayout(listHost);
    listLayout_->setContentsMargins(30, 4, 30, 26);
    listLayout_->setSpacing(9);
    scroll->setWidget(listHost);
    root->addWidget(scroll, 1);

    reload();
}

void CalibrationLibraryPage::reload() {
    // clear
    while (auto* it = listLayout_->takeAt(0)) {
        if (auto* w = it->widget()) w->deleteLater();
        delete it;
    }
    for (const auto& c : PaperState::instance().listConfigs())
        listLayout_->addWidget(buildCard(c));
    listLayout_->addWidget(buildNewRow());
    listLayout_->addStretch();
}

QWidget* CalibrationLibraryPage::buildCard(const PaperConfig& c) {
    auto* card = new QFrame;
    card->setStyleSheet(QString("QFrame{background:%1;border:1px solid #d2d5d9;border-radius:11px;}")
                            .arg(c.enabled ? "#ffffff" : "#f4f5f7"));
    auto* col = new QVBoxLayout(card);
    col->setContentsMargins(16, 14, 16, 14);
    col->setSpacing(0);

    // top row: toggle · name · status · actions
    auto* top = new QHBoxLayout;
    top->setSpacing(13);

    auto* toggle = new QToolButton;
    toggle->setCheckable(true);
    toggle->setChecked(c.enabled);
    toggle->setCursor(Qt::PointingHandCursor);
    toggle->setFixedSize(38, 22);
    toggle->setText(c.enabled ? "開" : "關");
    toggle->setStyleSheet(QString(
        "QToolButton{border:none;border-radius:11px;font-size:10px;color:#fff;background:%1;}")
        .arg(c.enabled ? kOk : "#c4c9d0"));
    QString cid = c.id;
    connect(toggle, &QToolButton::toggled, this, [this, cid](bool on) {
        PaperState::instance().setEnabled(cid, on);
        reload();
    });
    top->addWidget(toggle);

    auto* nameBox = new QVBoxLayout;
    nameBox->setSpacing(2);
    auto* cn = new QLabel(QStringLiteral("%1 v%2").arg(c.configName).arg(c.version));
    cn->setStyleSheet("font-size:14px;font-weight:700;color:#23262b;border:none;");
    auto* cp = new QLabel(QStringLiteral("%1 · %2 · %3 · 墨批 %4 · %5")
        .arg(c.paperName, c.paperMeta, c.printer, c.inkBatch, daysAgo(c.lastCalibrated)));
    cp->setStyleSheet("font-size:11px;color:#868c95;border:none;");
    nameBox->addWidget(cn); nameBox->addWidget(cp);
    top->addLayout(nameBox, 1);

    Badge b = badgeFor(c);
    auto* badge = new QLabel(b.text);
    badge->setStyleSheet(QString("background:%1;color:%2;border-radius:20px;padding:4px 10px;"
                                 "font-size:11px;font-weight:700;").arg(b.bg, b.fg));
    top->addWidget(badge);

    if (c.enabled) {
        const bool batchChanged = (c.inkBatch == "#A6");
        auto* autoBtn = new QPushButton(QStringLiteral("自動校驗"));
        autoBtn->setCursor(Qt::PointingHandCursor);
        autoBtn->setEnabled(!batchChanged);
        autoBtn->setToolTip(batchChanged
            ? QStringLiteral("墨批已換，線性偏移過大，自動校驗不適用，需重新校正")
            : QStringLiteral("重測並比對建檔時的線性化(SCTV)＋G7 基準；只在線性化超標時依 SCTV 微調，不動 ICC"));
        autoBtn->setStyleSheet("QPushButton{background:#e8f5ec;color:#1d6b39;border:1px solid #bfe3c9;"
                               "border-radius:7px;padding:6px 11px;font-size:11.5px;font-weight:600;}"
                               "QPushButton:disabled{color:#aeb3b9;background:#f1f2f4;border-color:#e2e4e7;}");
        connect(autoBtn, &QPushButton::clicked, this, [this, cid] {
            PaperConfig cfg; bool found = false;
            for (const auto& x : PaperState::instance().listConfigs())
                if (x.id == cid) { cfg = x; found = true; break; }
            if (!found) return;
            const int days = qMax(0, cfg.lastCalibrated.daysTo(QDateTime::currentDateTime()));
            const double tol = 1.0;                          // 線性化容差 ΔE
            const double curLin = cfg.linBaselineDe + days * 0.03;  // 模擬隨時間漂移
            QString msg = QStringLiteral("紙張：%1 v%2\n量測設備自動讀數 → 比對建檔基準\n\n")
                              .arg(cfg.configName).arg(cfg.version);
            msg += QStringLiteral("• 線性化 (SCTV)：現測 ΔE %1，基準 %2，容差 %3\n")
                       .arg(curLin, 0, 'f', 2).arg(cfg.linBaselineDe, 0, 'f', 2).arg(tol, 0, 'f', 1);
            msg += QStringLiteral("• G7 平均 ΔE 基準 %1（彩色，僅參考不調整）\n\n")
                       .arg(cfg.g7BaselineDe, 0, 'f', 2);
            if (curLin <= tol) {
                msg += QStringLiteral("結果：在容差內，無需調整。");
                QMessageBox::information(this, QStringLiteral("自動校驗"), msg);
            } else {
                cfg.lastCalibrated = QDateTime::currentDateTime();  // 重建線性化 → 刷新標準
                PaperState::instance().saveConfig(cfg);
                msg += QStringLiteral("結果：線性化漂移超標，已依 SCTV 重建線性化並刷新標準；G7 / ICC 不動。");
                QMessageBox::information(this, QStringLiteral("自動校驗"), msg);
                reload();
            }
        });
        top->addWidget(autoBtn);

        auto* recalBtn = new QPushButton(QStringLiteral("重新校正"));
        recalBtn->setCursor(Qt::PointingHandCursor);
        recalBtn->setToolTip(QStringLiteral("從頭重建完整配置，自動命名為下一版（v2 / v3…），不覆蓋舊配置"));
        recalBtn->setStyleSheet(QString("QPushButton{background:#fff;color:%1;border:1px solid #cfe0f4;"
                                        "border-radius:7px;padding:6px 11px;font-size:11.5px;font-weight:600;}")
                                    .arg(kAccent));
        connect(recalBtn, &QPushButton::clicked, this, [this, cid] { emit recalibrateRequested(cid); });
        top->addWidget(recalBtn);
    } else {
        auto* enBtn = new QPushButton(QStringLiteral("啟用"));
        enBtn->setStyleSheet("QPushButton{background:#fff;color:#666;border:1px solid #d2d5d9;"
                             "border-radius:7px;padding:6px 11px;font-size:11.5px;font-weight:600;}");
        connect(enBtn, &QPushButton::clicked, this, [this, cid] {
            PaperState::instance().setEnabled(cid, true); reload();
        });
        top->addWidget(enBtn);
        auto* delBtn = new QPushButton(QStringLiteral("刪除"));
        delBtn->setStyleSheet("QPushButton{background:#fff;color:#666;border:1px solid #d2d5d9;"
                              "border-radius:7px;padding:6px 11px;font-size:11.5px;font-weight:600;}");
        connect(delBtn, &QPushButton::clicked, this, [this, cid] {
            PaperState::instance().removeConfig(cid); reload();
        });
        top->addWidget(delBtn);
    }
    col->addLayout(top);

    // mode chips (full names) — only for enabled configs
    if (c.enabled) {
        auto* chips = new QHBoxLayout;
        chips->setContentsMargins(51, 12, 0, 0);
        chips->setSpacing(7);
        for (PrintMode m : allModes()) {
            bool on = c.modeBuilt[modeColumn(m)];
            auto* chip = new QLabel(on ? QStringLiteral("✓ %1").arg(modeFullName(m))
                                       : QStringLiteral("＋ %1 · 未建").arg(modeFullName(m)));
            chip->setStyleSheet(on
                ? "background:#e8f5ec;color:#1d6b39;border:1px solid #bfe3c9;border-radius:7px;"
                  "padding:5px 10px;font-size:11.5px;font-weight:600;"
                : "background:#f1f2f4;color:#aeb3b9;border:1px solid #e2e4e7;border-radius:7px;"
                  "padding:5px 10px;font-size:11.5px;font-weight:600;");
            chips->addWidget(chip);
        }
        chips->addStretch();
        col->addLayout(chips);
    }
    return card;
}

QWidget* CalibrationLibraryPage::buildNewRow() {
    auto* row = new QFrame;
    row->setCursor(Qt::PointingHandCursor);
    row->setStyleSheet("QFrame{background:#fff;border:1px dashed #c2cdd9;border-radius:10px;}");
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(16, 16, 16, 16);
    h->setSpacing(11);
    auto* plus = new QLabel("＋");
    plus->setFixedSize(30, 30);
    plus->setAlignment(Qt::AlignCenter);
    plus->setStyleSheet("background:#eaf2fc;color:#2e7dd7;border-radius:15px;font-size:18px;font-weight:700;");
    h->addWidget(plus);
    auto* box = new QVBoxLayout; box->setSpacing(2);
    auto* t = new QLabel(QStringLiteral("新建校準"));
    t->setStyleSheet("font-weight:700;font-size:13.5px;color:#23262b;border:none;");
    auto* s = new QLabel(QStringLiteral("選打印機 → 建紙張 → 走紙米尺 → 選儀器 → 5 種列印方式自動校準 → 存標準"));
    s->setStyleSheet("font-size:11.5px;color:#868c95;border:none;");
    box->addWidget(t); box->addWidget(s);
    h->addLayout(box); h->addStretch();

    // clickable whole row
    auto* overlay = new QPushButton(row);
    overlay->setFlat(true);
    overlay->setStyleSheet("background:transparent;border:none;");
    overlay->setCursor(Qt::PointingHandCursor);
    overlay->setGeometry(0, 0, 10000, 80);
    connect(overlay, &QPushButton::clicked, this, &CalibrationLibraryPage::newCalibrationRequested);
    return row;
}

}  // namespace jcolour
