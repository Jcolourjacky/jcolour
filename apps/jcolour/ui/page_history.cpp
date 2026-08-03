#include "page_history.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDateEdit>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>

#include "../translations.h"
#include "../theme.h"

namespace jcolour {

PageHistory::PageHistory(QWidget* parent) : QWidget(parent) {
    setObjectName("pageHistory");
    setStyleSheet("#pageHistory{background:#f6f7f9;}" + jcolour::theme::pageStyle());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(14);

    auto* title = new QLabel(TR("History"));
    title->setStyleSheet("font-size:21px;font-weight:700;color:#23262b;");
    root->addWidget(title);

    auto* filterRow = new QHBoxLayout;
    filterRow->setSpacing(8);

    filterRow->addWidget(new QLabel(TR("From")));
    auto* from = new QDateEdit;  from->setCalendarPopup(true);  filterRow->addWidget(from);
    filterRow->addWidget(new QLabel(TR("To")));
    auto* to   = new QDateEdit;  to->setCalendarPopup(true);    filterRow->addWidget(to);

    filterRow->addSpacing(16);
    filterRow->addWidget(new QLabel(TR("Printer")));
    auto* printerCb = new QComboBox;  printerCb->addItem(TR("All"));  filterRow->addWidget(printerCb);

    filterRow->addWidget(new QLabel(TR("Media")));
    auto* mediaCb = new QComboBox;    mediaCb->addItem(TR("All"));   filterRow->addWidget(mediaCb);

    filterRow->addStretch(1);

    auto* exportBtn = new QPushButton(TR("Export CSV"));
    filterRow->addWidget(exportBtn);

    root->addLayout(filterRow);

    table_ = new QTableWidget(0, 6);
    table_->setHorizontalHeaderLabels({
        TR("Date"), TR("Filename"), TR("Copies"),
        TR("Color Management"), TR("Ink Used (ml)"), TR("Status")
    });
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setAlternatingRowColors(true);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(table_, 1);
}

}  // namespace jcolour
