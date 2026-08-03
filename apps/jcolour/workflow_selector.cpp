#include "workflow_selector.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QIcon>

namespace jcolour {

WorkflowSelectorDialog::WorkflowSelectorDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("選擇打印流程");
    setModal(true);
    setMinimumSize(600, 400);

    setupUi();
}

void WorkflowSelectorDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    // Title
    auto* title = new QLabel("選擇打印流程");
    title->setStyleSheet("font-size: 16px; font-weight: bold; padding: 8px;");
    layout->addWidget(title);

    // Workflow table
    table_ = new QTableWidget(6, 4);
    table_->setHorizontalHeaderLabels({"#", "流程名稱", "墨水類型", "色彩空間"});
    table_->verticalHeader()->setVisible(false);

    // Workflow data
    const QStringList workflows[] = {
        QStringList{"1", "單黑（照片黑或粗面黑）", "1支黑", "灰度"},
        QStringList{"2", "四色黑（黑+灰+淺灰+粗面黑）", "4支中性墨", "灰度"},
        QStringList{"3", "黑白+彩色墨輔助", "4支中性+部分彩色", "灰度"},
        QStringList{"4", "普通 RGB 打印", "全色", "RGB"},
        // Skip workflow 5 (CMYK)
        QStringList{"6", "ProPhoto RGB 打印", "全色", "RGB寬色域"},
    };

    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 4; ++col) {
            auto* item = new QTableWidgetItem(workflows[row][col]);
            item->setTextAlignment(Qt::AlignCenter);
            table_->setItem(row, col, item);
        }
        // Make entire row selectable
        table_->setRowHeight(row, 40);
    }

    // Adjust column widths
    table_->setColumnWidth(0, 40);
    table_->setColumnWidth(1, 250);
    table_->setColumnWidth(2, 150);
    table_->setColumnWidth(3, 100);
    table_->horizontalHeader()->setStretchLastSection(true);

    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->selectRow(0);

    layout->addWidget(table_);

    // Buttons
    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    auto* cancelBtn = new QPushButton("取消");
    cancelBtn->setFixedWidth(100);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelBtn);

    auto* okBtn = new QPushButton("確定");
    okBtn->setFixedWidth(100);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(okBtn);

    layout->addLayout(buttonLayout);
}

int WorkflowSelectorDialog::selectedWorkflow() const {
    int row = table_->currentRow();
    if (row < 0) return 1;

    // Map table row to workflow number
    // Rows 0-3 map to workflows 1-4
    // Row 4 maps to workflow 6
    if (row < 4) return row + 1;
    return 6;
}

int WorkflowSelectorDialog::selectWorkflow(QWidget* parent) {
    WorkflowSelectorDialog dialog(parent);
    if (dialog.exec() == QDialog::Accepted) {
        return dialog.selectedWorkflow();
    }
    return -1;  // Cancelled
}

}  // namespace jcolour
