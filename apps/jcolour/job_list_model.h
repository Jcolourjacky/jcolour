#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include "job.h"

namespace jcolour {

class JobListModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColStatus = 0,
        ColFilename,
        ColJobId,
        ColThumbnail,
        ColFormat,
        ColColorAdjust,
        ColPositioned,
        ColColorSpace,
        ColPrinter,
        ColPages,
        ColCopies,
        ColSize,
        ColumnCount
    };

    explicit JobListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation o, int role = Qt::DisplayRole) const override;

    void addJob(const Job& job);
    void removeJob(int row);
    void updateJob(int row, const Job& job);
    const Job& jobAt(int row) const { return jobs_[row]; }
    Job& jobRefAt(int row) { return jobs_[row]; }
    int  jobCount() const { return jobs_.size(); }

    void setWorkflowFilter(const QString& workflowName); // empty = all
    QString workflowFilter() const { return workflowFilter_; }

    int countByStatus(JobStatus s) const;

signals:
    void jobAdded(int row);
    void jobChanged(int row);

private:
    bool matchesFilter(const Job& job) const;

    QVector<Job> jobs_;
    QString workflowFilter_;
};

} // namespace jcolour
