#include "workflow_sidebar.h"
#include <QVBoxLayout>

namespace jcolour {

WorkflowSidebar::WorkflowSidebar(QWidget* parent)
    : QWidget(parent) {
    setObjectName("workflowSidebar");
    setFixedWidth(168);
    setStyleSheet("QWidget#workflowSidebar { background:#ececec; "
                  "border-right:1px solid #a8a8a8; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    groupCombo_ = new QComboBox;
    groupCombo_->addItems({"Workflow", "Paper", "Printer", "Status"});
    layout->addWidget(groupCombo_);

    list_ = new QListWidget;
    list_->setFrameShape(QFrame::NoFrame);
    list_->setStyleSheet("QListWidget { background:#ffffff; "
                         "border:1px solid #a8a8a8; }");
    list_->addItem("Photo / Fineart");
    list_->addItem("B&W Multi-Channel");
    list_->addItem("B&W Single Black");
    list_->addItem("EFI Linearization");
    list_->addItem("ISO Coated v2");
    list_->addItem("G7 Calibration");
    list_->addItem("Custom");
    list_->setCurrentRow(0);
    layout->addWidget(list_, 1);

    allJobsBtn_ = new QPushButton("All Jobs");
    layout->addWidget(allJobsBtn_);

    connect(list_, &QListWidget::currentRowChanged,
            this, &WorkflowSidebar::onSelectionChanged);
    connect(allJobsBtn_, &QPushButton::clicked,
            this, &WorkflowSidebar::onAllJobsClicked);
    connect(groupCombo_, &QComboBox::currentTextChanged,
            this, &WorkflowSidebar::groupingChanged);
}

QString WorkflowSidebar::currentWorkflow() const {
    auto* item = list_->currentItem();
    return item ? item->text() : QString();
}

void WorkflowSidebar::onSelectionChanged() {
    emit workflowChanged(currentWorkflow());
}

void WorkflowSidebar::onAllJobsClicked() {
    list_->setCurrentRow(-1);
    emit workflowChanged(QString());
}

} // namespace jcolour
