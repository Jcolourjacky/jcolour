#pragma once

#include <QWidget>
#include <QListWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

namespace jcolour {

class WorkflowSidebar : public QWidget {
    Q_OBJECT
public:
    explicit WorkflowSidebar(QWidget* parent = nullptr);

    QString currentWorkflow() const;       // empty = "All Jobs"

signals:
    void workflowChanged(const QString& name); // empty = all
    void groupingChanged(const QString& key);

private slots:
    void onSelectionChanged();
    void onAllJobsClicked();

private:
    QComboBox*   groupCombo_;
    QListWidget* list_;
    QPushButton* allJobsBtn_;
};

} // namespace jcolour
