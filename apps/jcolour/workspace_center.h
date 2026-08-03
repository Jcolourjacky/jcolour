#pragma once

#include <QWidget>
#include <QSplitter>
#include <QTableView>
#include <QSortFilterProxyModel>
#include <QToolButton>
#include <QComboBox>
#include <QLabel>

class ImagePreviewWidget;

namespace jcolour {

class JobListModel;
class Job;

class WorkspaceCenter : public QWidget {
    Q_OBJECT
public:
    explicit WorkspaceCenter(JobListModel* model, QWidget* parent = nullptr);

    ImagePreviewWidget* preview() { return preview_; }

    int  currentJobRow() const;
    void selectJobRow(int row);
    void setWorkflowFilter(const QString& workflowName);

signals:
    void jobSelected(int row);

private slots:
    void onSelectionChanged();
    void onJobsChanged();

private:
    JobListModel*           model_;
    QSortFilterProxyModel*  proxy_;
    QSplitter*              splitter_;
    ImagePreviewWidget*     preview_;
    QTableView*             jobTable_;
    QComboBox*              zoomCombo_;
    QLabel*                 pageLabel_;
};

} // namespace jcolour
