#include "workspace_center.h"
#include "preview_widget.h"
#include "job_list_model.h"
#include "job.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QToolBar>

namespace jcolour {

namespace {

class WorkflowFilterProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setWorkflow(const QString& name) {
        workflow_ = name;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex&) const override {
        if (workflow_.isEmpty()) return true;
        auto* m = qobject_cast<JobListModel*>(sourceModel());
        if (!m) return true;
        return m->jobAt(row).workflowName == workflow_;
    }

private:
    QString workflow_;
};

} // namespace

WorkspaceCenter::WorkspaceCenter(JobListModel* model, QWidget* parent)
    : QWidget(parent)
    , model_(model)
    , proxy_(new WorkflowFilterProxy(this)) {
    proxy_->setSourceModel(model_);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    splitter_ = new QSplitter(Qt::Vertical, this);
    splitter_->setChildrenCollapsible(false);
    splitter_->setHandleWidth(1);

    // ---- Top: preview ------------------------------------------------
    auto* previewWrap = new QWidget;
    previewWrap->setStyleSheet("background:#7a7a7a;"); // EFI canvas grey
    auto* previewLay = new QVBoxLayout(previewWrap);
    previewLay->setContentsMargins(0, 0, 0, 0);
    previewLay->setSpacing(0);
    preview_ = new ImagePreviewWidget(previewWrap);
    previewLay->addWidget(preview_, 1);

    // Zoom / pagination strip below preview
    auto* zoomBar = new QWidget;
    zoomBar->setFixedHeight(26);
    zoomBar->setStyleSheet(
        "background:#dcdcdc;"
        "border-top:1px solid #b0b0b0;");
    auto* zoomLay = new QHBoxLayout(zoomBar);
    zoomLay->setContentsMargins(6, 0, 10, 0);
    zoomLay->setSpacing(4);

    auto* fitBtn  = new QToolButton; fitBtn->setText("Fit");
    auto* zoomOut = new QToolButton; zoomOut->setText("−");
    auto* zoomIn  = new QToolButton; zoomIn->setText("+");
    zoomCombo_ = new QComboBox;
    zoomCombo_->setEditable(true);
    zoomCombo_->addItems({"25%", "50%", "75%", "100%", "150%", "200%", "400%"});
    zoomCombo_->setCurrentText("Fit");
    zoomCombo_->setFixedWidth(70);

    pageLabel_ = new QLabel("0 jobs");
    pageLabel_->setStyleSheet("color:#555555;");

    zoomLay->addWidget(fitBtn);
    zoomLay->addWidget(zoomOut);
    zoomLay->addWidget(zoomCombo_);
    zoomLay->addWidget(zoomIn);
    zoomLay->addStretch();
    zoomLay->addWidget(pageLabel_);
    previewLay->addWidget(zoomBar);

    splitter_->addWidget(previewWrap);

    // ---- Bottom: job table ------------------------------------------
    auto* tableWrap = new QWidget;
    auto* tableLay = new QVBoxLayout(tableWrap);
    tableLay->setContentsMargins(0, 0, 0, 0);
    tableLay->setSpacing(0);

    jobTable_ = new QTableView;
    jobTable_->setModel(proxy_);
    jobTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    jobTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    jobTable_->setAlternatingRowColors(true);
    jobTable_->setShowGrid(false);
    jobTable_->setSortingEnabled(true);
    jobTable_->verticalHeader()->setVisible(false);
    jobTable_->verticalHeader()->setDefaultSectionSize(28);
    jobTable_->horizontalHeader()->setStretchLastSection(false);
    jobTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    jobTable_->horizontalHeader()->setHighlightSections(false);

    jobTable_->setColumnWidth(JobListModel::ColStatus,      90);
    jobTable_->setColumnWidth(JobListModel::ColFilename,    220);
    jobTable_->setColumnWidth(JobListModel::ColJobId,       70);
    jobTable_->setColumnWidth(JobListModel::ColThumbnail,   60);
    jobTable_->setColumnWidth(JobListModel::ColFormat,      60);
    jobTable_->setColumnWidth(JobListModel::ColColorAdjust, 100);
    jobTable_->setColumnWidth(JobListModel::ColPositioned,  80);
    jobTable_->setColumnWidth(JobListModel::ColColorSpace,  80);
    jobTable_->setColumnWidth(JobListModel::ColPrinter,     180);
    jobTable_->setColumnWidth(JobListModel::ColPages,       60);
    jobTable_->setColumnWidth(JobListModel::ColCopies,      60);
    jobTable_->setColumnWidth(JobListModel::ColSize,        80);

    tableLay->addWidget(jobTable_);
    splitter_->addWidget(tableWrap);

    splitter_->setStretchFactor(0, 3);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes({600, 220});

    root->addWidget(splitter_, 1);

    connect(jobTable_->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &WorkspaceCenter::onSelectionChanged);
    connect(model_, &JobListModel::jobAdded, this, &WorkspaceCenter::onJobsChanged);
    connect(model_, &JobListModel::jobChanged, this, &WorkspaceCenter::onJobsChanged);
}

int WorkspaceCenter::currentJobRow() const {
    auto idx = jobTable_->currentIndex();
    if (!idx.isValid()) return -1;
    return proxy_->mapToSource(idx).row();
}

void WorkspaceCenter::selectJobRow(int row) {
    if (row < 0) { jobTable_->clearSelection(); return; }
    auto srcIdx = model_->index(row, 0);
    auto proxyIdx = proxy_->mapFromSource(srcIdx);
    jobTable_->setCurrentIndex(proxyIdx);
}

void WorkspaceCenter::setWorkflowFilter(const QString& name) {
    static_cast<WorkflowFilterProxy*>(proxy_)->setWorkflow(name);
}

void WorkspaceCenter::onSelectionChanged() {
    int row = currentJobRow();
    if (row >= 0) {
        const auto& job = model_->jobAt(row);
        if (!job.imagePath.isEmpty() && job.imagePath != preview_->currentImagePath()) {
            preview_->loadImage(job.imagePath);
        }
    }
    emit jobSelected(row);
}

void WorkspaceCenter::onJobsChanged() {
    pageLabel_->setText(QString("%1 jobs").arg(model_->jobCount()));
}

} // namespace jcolour
