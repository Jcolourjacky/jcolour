#pragma once

#include <QMainWindow>
#include <QToolButton>
#include <QStackedWidget>
#include <QLabel>

#include "settings_dialog.h"

class QComboBox;
class QDialog;

class ImagePreviewWidget;

namespace jcolour {
class JobListModel;
class PrintTypeBar;
class WorkspaceCenter;
class JobInspector;
class ConnectionBadge;
class MainSidebar;
class PrintGateBar;
namespace ui { class PrintInspector; }
} // namespace jcolour

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Screenshot helper: switch the sidebar page without user interaction.
    void selectPageForShot(int page);

private slots:
    void onOpenImage();
    void onPrint();
    void onMeasure();
    void onSettings();
    void onJobSelected(int row);
    void onInspectorModified();
    void onPrintTypeSelected(int workflowId, const QString& name);
    void onLanguageChanged();

private:
    void buildMenus();
    void buildToolbar();
    void buildStatusBar();
    QWidget* buildJobsPage();
    QWidget* buildSettingsTabs();   // the 4-tab settings content (hosted in a modal)
    void openSettingsDialog();      // 設定 is a modal dialog, not a sidebar page
    void updateGate();          // recompute the pre-print status gate
    void reloadPaperCombo();

    jcolour::JobListModel*        model_        = nullptr;
    jcolour::PrintTypeBar*        printTypeBar_ = nullptr;
    ImagePreviewWidget*           preview_      = nullptr;   // print-page canvas
    jcolour::ui::PrintInspector*  printInspector_ = nullptr; // collapsible inspector
    jcolour::MainSidebar*         mainSidebar_  = nullptr;
    jcolour::PrintGateBar*        gateBar_      = nullptr;   // == printInspector_->gate()
    QComboBox*                    printerCombo_ = nullptr;
    QComboBox*                    paperCombo_   = nullptr;

    QStackedWidget* moduleStack_ = nullptr;

    QLabel*                    statTotal_      = nullptr;
    QLabel*                    statHeld_       = nullptr;
    QLabel*                    statPrinting_   = nullptr;
    jcolour::ConnectionBadge*  printerBadge_   = nullptr;
    jcolour::SettingsDialog*   settingsDlg_    = nullptr;
    QDialog*                   settingsModal_  = nullptr;  // sidebar 設定 → modal tabs

    int currentJobRow_ = -1;
    int lastPage_      = 0;     // last non-settings sidebar page (for highlight restore)
};
