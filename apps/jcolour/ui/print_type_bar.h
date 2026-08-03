#pragma once

#include <QWidget>
#include <QButtonGroup>
#include <QVector>

namespace jcolour {

// Horizontal bar of 5 selectable "print type" cards shown at the top of the
// Jobs page (Style A · Mirage 專業淺色). The 5 types mirror the print
// workflows defined in workflow_selector.cpp (ink / colour-space variants).
// Selecting a card sets the active workflow and asks the host to import a file.
class PrintTypeBar : public QWidget {
    Q_OBJECT
public:
    struct PrintType {
        int     id;          // canonical workflow id (1-4, 6)
        QString name;        // short title, also used as Job::workflowName
        QString subtitle;    // ink / colour-space hint
    };

    explicit PrintTypeBar(QWidget* parent = nullptr);

    QString currentTypeName() const;   // empty if nothing checked
    int     currentTypeId() const;     // -1 if nothing checked

signals:
    void typeSelected(int workflowId, const QString& name);

private:
    QButtonGroup*      group_ = nullptr;
    QVector<PrintType> types_;
};

}  // namespace jcolour
