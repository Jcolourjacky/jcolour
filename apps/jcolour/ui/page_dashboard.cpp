#include "page_dashboard.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>

#include "../translations.h"

namespace jcolour {

namespace {

QFrame* makeCard(const QString& title, const QString& bigValue, const QString& sub) {
    auto* card = new QFrame;
    card->setObjectName("dashCard");
    card->setStyleSheet(
        "#dashCard { background:white; border:1px solid #e1e3e6; border-radius:6px; }"
        "QLabel#cardTitle { color:#777; font-size:12px; }"
        "QLabel#cardValue { color:#222; font-size:24px; font-weight:600; }"
        "QLabel#cardSub   { color:#999; font-size:11px; }"
    );
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(16, 14, 16, 14);
    lay->setSpacing(4);

    auto* l1 = new QLabel(title);            l1->setObjectName("cardTitle");
    auto* l2 = new QLabel(bigValue);         l2->setObjectName("cardValue");
    auto* l3 = new QLabel(sub);              l3->setObjectName("cardSub");

    lay->addWidget(l1);
    lay->addWidget(l2);
    lay->addWidget(l3);
    lay->addStretch(1);
    return card;
}

}  // anon

PageDashboard::PageDashboard(QWidget* parent) : QWidget(parent) {
    setObjectName("pageDashboard");
    setStyleSheet("#pageDashboard { background:#f8f9fa; }");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);

    auto* title = new QLabel(TR("Dashboard"));
    QFont tf = title->font();
    tf.setPointSize(18);
    tf.setBold(true);
    title->setFont(tf);
    root->addWidget(title);

    auto* grid = new QGridLayout;
    grid->setSpacing(12);

    grid->addWidget(makeCard(TR("Printer Status"),  TR("Connected"), TR("Last sync just now")),       0, 0);
    grid->addWidget(makeCard(TR("Active Jobs"),     "0",             TR("0 completed today")),        0, 1);
    grid->addWidget(makeCard(TR("Failed Jobs"),     "0",             TR("Last 7 days")),              0, 2);
    grid->addWidget(makeCard(TR("Ink Warning"),     TR("All OK"),    TR("No cartridge below 10%")),   1, 0);
    grid->addWidget(makeCard(TR("Last Calibration"), "—",            TR("Run calibration to update")), 1, 1);
    grid->addWidget(makeCard(TR("Active Profiles"), "0",             TR("Across all printers")),      1, 2);

    root->addLayout(grid);

    auto* shortcuts = new QHBoxLayout;
    shortcuts->setSpacing(8);
    auto mkBtn = [](const QString& t){
        auto* b = new QPushButton(t);
        b->setMinimumHeight(32);
        return b;
    };
    shortcuts->addWidget(mkBtn(TR("New Job")));
    shortcuts->addWidget(mkBtn(TR("Add Media")));
    shortcuts->addWidget(mkBtn(TR("Run Calibration")));
    shortcuts->addStretch(1);
    root->addLayout(shortcuts);

    root->addStretch(1);
}

}  // namespace jcolour
