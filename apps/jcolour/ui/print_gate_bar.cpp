#include "print_gate_bar.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace jcolour {

namespace {
constexpr const char* kAccent = "#2e7dd7";
const char* dotColor(bool ok, bool warn = false) {
    return ok ? "#2f8c4f" : (warn ? "#c98a17" : "#c0392b");
}
}  // namespace

PrintGateBar::PrintGateBar(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("gateBar");
    setStyleSheet("#gateBar{background:#ffffff; border-top:1px solid #d2d5d9;}"
                  "#gateBar QLabel{background:transparent;}");

    // Single horizontal row (matches design/ui-mockup.html .gatebar):
    //   [ lamps … ] [ banner (stretch) ] [ 重校 ][ 測試列印 ][ 列印 ]
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(16, 8, 16, 8);
    root->setSpacing(14);

    auto* lamps = new QHBoxLayout;
    lamps->setSpacing(16);
    lamps->addWidget(lamp(nozzleDot_, nozzleTxt_, QStringLiteral("噴頭")));
    lamps->addWidget(lamp(feedDot_,   feedTxt_,   QStringLiteral("走紙")));
    lamps->addWidget(lamp(calDot_,    calTxt_,    QStringLiteral("校準")));
    lamps->addWidget(lamp(qcDot_,     qcTxt_,     QStringLiteral("品質")));
    root->addLayout(lamps);

    banner_ = new QLabel;
    banner_->setStyleSheet("border-radius:7px; padding:6px 11px; font-size:12px;");
    root->addWidget(banner_, 1);

    recalBtn_ = new QPushButton(QStringLiteral("重校"));
    recalBtn_->setCursor(Qt::PointingHandCursor);
    recalBtn_->setStyleSheet("QPushButton{background:#fff;border:1px solid #d2d5d9;border-radius:8px;"
                             "padding:8px 14px;font-size:12.5px;font-weight:600;color:#8a6310;}");
    connect(recalBtn_, &QPushButton::clicked, this, &PrintGateBar::recalibrateRequested);
    root->addWidget(recalBtn_);

    testBtn_ = new QPushButton(QStringLiteral("測試列印"));
    testBtn_->setCursor(Qt::PointingHandCursor);
    testBtn_->setStyleSheet("QPushButton{background:#fff;border:1px solid #d2d5d9;border-radius:8px;"
                            "padding:8px 14px;font-size:12.5px;font-weight:600;color:#444;}");
    connect(testBtn_, &QPushButton::clicked, this, &PrintGateBar::testPrintRequested);
    root->addWidget(testBtn_);

    printBtn_ = new QPushButton(QStringLiteral("列印"));
    printBtn_->setCursor(Qt::PointingHandCursor);
    connect(printBtn_, &QPushButton::clicked, this, &PrintGateBar::printRequested);
    root->addWidget(printBtn_);
}

QWidget* PrintGateBar::lamp(QLabel*& dot, QLabel*& text, const QString& name) {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(9);
    dot = new QLabel;
    dot->setFixedSize(9, 9);
    dot->setStyleSheet("border-radius:4px; background:#c4c9d0;");
    auto* nm = new QLabel(name);
    nm->setStyleSheet("font-size:12px;font-weight:600;color:#23262b;");
    text = new QLabel;
    text->setStyleSheet("font-size:11.5px;color:#868c95;");
    text->setMaximumWidth(150);   // keep the single-row bar from pushing buttons off
    h->addWidget(dot);
    h->addWidget(nm);
    h->addWidget(text);
    return w;
}

void PrintGateBar::setGate(const GateResult& g) {
    auto setDot = [](QLabel* d, bool ok, bool warn) {
        d->setStyleSheet(QString("border-radius:4px; background:%1;").arg(dotColor(ok, warn)));
    };
    setDot(nozzleDot_, g.nozzleFresh, true);
    nozzleTxt_->setText(g.nozzleInfo);
    setDot(feedDot_, g.feedStored, false);
    feedTxt_->setText(g.feedInfo);
    setDot(calDot_, g.calOk, g.level == GateLevel::SoftWarn);
    calTxt_->setText(g.calInfo);
    setDot(qcDot_, g.qcOk, !g.qcOk);   // advisory: warn (amber) when not yet checked
    qcTxt_->setText(g.qcInfo);

    QString bg, fg;
    switch (g.level) {
        case GateLevel::Green:    bg = "#e8f5ec"; fg = "#1d6b39"; break;
        case GateLevel::SoftWarn: bg = "#fbf2dd"; fg = "#8a6310"; break;
        case GateLevel::HardBlock:bg = "#fbe9e7"; fg = "#9a2418"; break;
    }
    banner_->setStyleSheet(QString("border-radius:7px;padding:9px 11px;font-size:12px;"
                                   "background:%1;color:%2;").arg(bg, fg));
    banner_->setText(g.summary);

    const bool blocked = (g.level == GateLevel::HardBlock);
    const bool warn    = (g.level == GateLevel::SoftWarn);
    recalBtn_->setVisible(blocked || warn);

    printBtn_->setEnabled(!blocked);
    printBtn_->setStyleSheet(QString(
        "QPushButton{background:%1;color:#fff;border:none;border-radius:8px;"
        "padding:9px 18px;font-size:13px;font-weight:600;}"
        "QPushButton:disabled{background:#bcc3cb;}")
        .arg(blocked ? "#bcc3cb" : kAccent));
    printBtn_->setText(blocked ? QStringLiteral("未校準 · 無法列印") : QStringLiteral("列印"));
}

}  // namespace jcolour
