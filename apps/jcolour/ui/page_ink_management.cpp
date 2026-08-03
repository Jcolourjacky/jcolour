#include "page_ink_management.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QHeaderView>

#include "../translations.h"

namespace jcolour {

PageInkManagement::PageInkManagement(QWidget* parent) : QWidget(parent) {
    setObjectName("pageInkMgmt");
    setStyleSheet("#pageInkMgmt { background:#f8f9fa; }");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(12);

    auto* headerRow = new QHBoxLayout;
    auto* title = new QLabel(TR("Printer Ink Management"));
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf);
    headerRow->addWidget(title);
    headerRow->addStretch(1);

    headerRow->addWidget(new QPushButton(TR("Monthly Summary")));
    headerRow->addWidget(new QPushButton(TR("Export Report")));
    root->addLayout(headerRow);

    tree_ = new QTreeWidget;
    tree_->setColumnCount(3);
    tree_->setHeaderLabels({
        TR("Printer Model / Cartridge / Job"),
        TR("Cartridge ID"),
        TR("Ink Used (ml)")
    });
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->setAlternatingRowColors(true);
    root->addWidget(tree_, 1);
}

}  // namespace jcolour
