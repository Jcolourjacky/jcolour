#include "job_list_model.h"
#include <QFileInfo>
#include <QColor>
#include <QBrush>
#include <QPixmap>

namespace jcolour {

JobListModel::JobListModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int JobListModel::rowCount(const QModelIndex&) const {
    return jobs_.size();
}

int JobListModel::columnCount(const QModelIndex&) const {
    return ColumnCount;
}

static QString humanSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024);
    return QString("%1.%2 MB").arg(bytes / (1024 * 1024))
        .arg((bytes % (1024 * 1024)) * 10 / (1024 * 1024));
}

QVariant JobListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= jobs_.size()) return {};
    const Job& j = jobs_[index.row()];

    if (role == Qt::DecorationRole && index.column() == ColThumbnail) {
        if (!j.thumbnail.isNull()) return QPixmap::fromImage(j.thumbnail);
    }
    if (role == Qt::ForegroundRole && index.column() == ColStatus) {
        switch (j.status) {
            case JobStatus::Spooled:    return QBrush(QColor("#ff9f0a"));
            case JobStatus::Processing: return QBrush(QColor("#0a84ff"));
            case JobStatus::Printing:   return QBrush(QColor("#30d158"));
            case JobStatus::Completed:  return QBrush(QColor("#98989d"));
            case JobStatus::Failed:     return QBrush(QColor("#ff453a"));
            case JobStatus::Held:       return QBrush(QColor("#ff9f0a"));
        }
    }
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColStatus:      return jobStatusName(j.status);
            case ColFilename:    return QFileInfo(j.imagePath).fileName();
            case ColJobId:       return j.id;
            case ColThumbnail:   return {};
            case ColFormat:      return QFileInfo(j.imagePath).suffix().toUpper();
            case ColColorAdjust: {
                bool changed = j.corrC || j.corrM || j.corrY || j.corrK
                            || j.corrR || j.corrG || j.corrB
                            || j.brightness || j.contrast || j.saturation;
                return changed ? "modified" : "unchanged";
            }
            case ColPositioned:  return j.positioned ? "Yes" : "";
            case ColColorSpace:  return j.colorSpace;
            case ColPrinter:     return j.printerName;
            case ColPages:       return j.pageCount;
            case ColCopies:      return j.copies;
            case ColSize:        return humanSize(j.fileSizeBytes);
        }
    }
    if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColPages || index.column() == ColCopies
            || index.column() == ColSize) {
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    return {};
}

QVariant JobListModel::headerData(int section, Qt::Orientation o, int role) const {
    if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case ColStatus:      return "Status";
        case ColFilename:    return "Filename";
        case ColJobId:       return "Job ID";
        case ColThumbnail:   return "Preview";
        case ColFormat:      return "Format";
        case ColColorAdjust: return "Color Adjust";
        case ColPositioned:  return "Positioned";
        case ColColorSpace:  return "Color Space";
        case ColPrinter:     return "Printer";
        case ColPages:       return "Pages";
        case ColCopies:      return "Copies";
        case ColSize:        return "Size";
    }
    return {};
}

bool JobListModel::matchesFilter(const Job& job) const {
    if (workflowFilter_.isEmpty()) return true;
    return job.workflowName == workflowFilter_;
}

void JobListModel::addJob(const Job& job) {
    beginInsertRows({}, jobs_.size(), jobs_.size());
    jobs_.append(job);
    endInsertRows();
    emit jobAdded(jobs_.size() - 1);
}

void JobListModel::removeJob(int row) {
    if (row < 0 || row >= jobs_.size()) return;
    beginRemoveRows({}, row, row);
    jobs_.remove(row);
    endRemoveRows();
}

void JobListModel::updateJob(int row, const Job& job) {
    if (row < 0 || row >= jobs_.size()) return;
    jobs_[row] = job;
    emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    emit jobChanged(row);
}

void JobListModel::setWorkflowFilter(const QString& w) {
    workflowFilter_ = w;
    // The filter is informational here; the view uses a QSortFilterProxyModel.
}

int JobListModel::countByStatus(JobStatus s) const {
    int n = 0;
    for (const auto& j : jobs_) if (j.status == s) ++n;
    return n;
}

} // namespace jcolour
