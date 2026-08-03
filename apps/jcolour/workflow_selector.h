#pragma once

#include <QDialog>
#include <QTableWidget>

namespace jcolour {

/**
 * Workflow selector dialog
 * Allows user to choose from 6 printing workflows:
 * 1. Single black (PK or MK)
 * 2. Four-tone gray (PK + MK + GY + LGY)
 * 3. Gray + color split-tone
 * 4. RGB printing
 * 5. (Skipped - CMYK, reserved for gcolour)
 * 6. ProPhoto RGB with gamut compression
 */
class WorkflowSelectorDialog : public QDialog {
    Q_OBJECT
public:
    explicit WorkflowSelectorDialog(QWidget* parent = nullptr);
    ~WorkflowSelectorDialog() override = default;

    int selectedWorkflow() const;

    // Static helper to show dialog and return selected workflow
    static int selectWorkflow(QWidget* parent);

signals:
    void workflowSelected(int workflow);

private:
    void setupUi();
    QTableWidget* table_ = nullptr;
};

}  // namespace jcolour
