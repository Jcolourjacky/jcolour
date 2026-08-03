#include "page_color_management.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QGroupBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QSpinBox>
#include <QInputDialog>

#include "../translations.h"

namespace jcolour {

namespace {

QWidget* makeMediaSection() {
    auto* box = new QGroupBox(TR("Media"));
    auto* lay = new QVBoxLayout(box);
    lay->setSpacing(8);

    auto* row = new QHBoxLayout;
    auto* addBtn = new QToolButton;
    addBtn->setText(TR("Add Media") + QStringLiteral("  ▾"));
    addBtn->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(addBtn);
    menu->addAction(TR("Add Brand Media"));
    menu->addAction(TR("Add Custom Media"));
    addBtn->setMenu(menu);

    auto* importBtn = new QPushButton(TR("Import Media"));

    row->addWidget(addBtn);
    row->addWidget(importBtn);
    row->addStretch(1);
    lay->addLayout(row);

    auto* table = new QTableWidget(0, 2);
    table->setHorizontalHeaderLabels({TR("Media Name"), TR("Available Sizes")});
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    lay->addWidget(table);

    return box;
}

QWidget* makeCalibrationSection() {
    auto* box = new QGroupBox(TR("Calibration / Color Profile"));
    auto* lay = new QGridLayout(box);
    lay->setHorizontalSpacing(8);
    lay->setVerticalSpacing(6);

    int row = 0;
    lay->addWidget(new QLabel(TR("Calibration Bar")), row, 0);
    lay->addWidget(new QLabel(TR("Profile Workflow")), row, 1);
    lay->addWidget(new QLabel(TR("Strip")),            row, 2);
    lay->addWidget(new QLabel(TR("Parameter Set")),    row, 3);
    ++row;

    const QStringList labels = {"A", "B", "C", "D", "E", "F"};
    for (const auto& lbl : labels) {
        auto* tag = new QLabel(lbl);
        tag->setAlignment(Qt::AlignCenter);
        tag->setStyleSheet(
            "background:#7d3a8e; color:white; "
            "border-radius:10px; min-width:24px; max-width:24px; "
            "min-height:24px; max-height:24px; font-weight:600;"
        );
        lay->addWidget(tag, row, 0);

        for (int c = 1; c <= 3; ++c) {
            auto* line = new QLabel(QStringLiteral("—"));
            line->setStyleSheet("color:#aaa;");
            lay->addWidget(line, row, c);
        }
        ++row;
    }

    auto* startBtn = new QPushButton(TR("Start Optimization"));
    lay->addWidget(startBtn, row, 3, 1, 1, Qt::AlignRight);

    return box;
}

QWidget* makeMediaDefaultsSection() {
    auto* box = new QGroupBox(TR("Per-Media Printer Defaults"));
    auto* lay = new QGridLayout(box);

    int r = 0;
    lay->addWidget(new QCheckBox(TR("Enable Auxiliary Drying")), r, 0, 1, 2); ++r;

    lay->addWidget(new QLabel(TR("Drying Time (s)")), r, 0);
    auto* dry = new QSpinBox;
    dry->setRange(0, 600);
    dry->setValue(30);
    lay->addWidget(dry, r, 1);
    ++r;

    return box;
}

}  // anon

PageColorManagement::PageColorManagement(QWidget* parent) : QWidget(parent) {
    setObjectName("pageColorMgmt");
    setStyleSheet("#pageColorMgmt { background:#f8f9fa; }");

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);

    // Left: printer list
    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(4);
    auto* leftHeader = new QLabel(TR("Printers"));
    QFont hf = leftHeader->font(); hf.setBold(true); leftHeader->setFont(hf);
    leftCol->addWidget(leftHeader);

    printerList_ = new QListWidget;
    printerList_->setMinimumWidth(180);
    leftCol->addWidget(printerList_, 1);

    auto* btnRow = new QHBoxLayout;
    auto* addBtn = new QToolButton;     addBtn->setText("+");
    auto* rmBtn  = new QToolButton;     rmBtn->setText("−");
    addBtn->setFixedSize(28, 24);
    rmBtn->setFixedSize(28, 24);
    btnRow->addWidget(addBtn);
    btnRow->addWidget(rmBtn);
    btnRow->addStretch(1);
    leftCol->addLayout(btnRow);

    connect(addBtn, &QToolButton::clicked, this, &PageColorManagement::onAddPrinter);
    connect(rmBtn,  &QToolButton::clicked, this, &PageColorManagement::onRemovePrinter);

    auto* leftWrap = new QWidget;
    leftWrap->setLayout(leftCol);
    leftWrap->setMaximumWidth(220);
    root->addWidget(leftWrap);

    // Right: detail stack
    detailStack_ = new QStackedWidget;
    detailStack_->addWidget(buildPrinterDetail());
    root->addWidget(detailStack_, 1);
}

QWidget* PageColorManagement::buildPrinterDetail() {
    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(12);

    lay->addWidget(makeMediaSection());
    lay->addWidget(makeCalibrationSection());
    lay->addWidget(makeMediaDefaultsSection());
    lay->addStretch(1);
    return w;
}

void PageColorManagement::onAddPrinter() {
    bool ok = false;
    QString name = QInputDialog::getText(this, TR("Add Printer"),
                                         TR("Printer Name") + ":",
                                         QLineEdit::Normal, "", &ok);
    if (ok && !name.isEmpty()) {
        printerList_->addItem(name);
    }
}

void PageColorManagement::onRemovePrinter() {
    int row = printerList_->currentRow();
    if (row >= 0) delete printerList_->takeItem(row);
}

}  // namespace jcolour
