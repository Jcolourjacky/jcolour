#include "mainwindow.h"

#include "theme.h"
#include "job_list_model.h"
#include "ui/print_inspector.h"
#include "preview_widget.h"
#include "ui/print_gate_bar.h"
#include "job_inspector.h"
#include "status_indicators.h"
#include "preview_widget.h"
#include "translations.h"
#include "curve_editor.h"

#include "calibration.h"
#include "ink_lot_tracker.h"
#include "monitor_calibration.h"
#include "printer_manager.h"
#include "profile_drag_drop.h"
#include "queue.h"
#include "storekit_manager.h"
#include "pjl.h"  // jcolour::pjl::EpsonPrinter — real transport to the printer

#include "ui/main_sidebar.h"
#include "ui/print_type_bar.h"
#include "ui/page_dashboard.h"
#include "ui/page_history.h"
#include "ui/page_color_management.h"
#include "ui/calibration_page.h"
#include "ui/print_gate_bar.h"
#include "paper_state.h"
#include "ui/page_ink_management.h"
#include "ui/page_measurement_devices.h"
#include "ui/print_env_manager.h"

#include <QApplication>
#include <QStyle>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QDateTime>
#include <QStackedWidget>
#include <QTabWidget>
#include <QDialog>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSet>
#include <QSettings>
#include <QImage>
#include <algorithm>
#include <vector>
#include <cstdint>

using jcolour::Job;
using jcolour::JobListModel;
using jcolour::JobStatus;
using jcolour::PrintTypeBar;
using jcolour::WorkspaceCenter;
using jcolour::JobInspector;
using jcolour::ConnectionBadge;

namespace {

// Resolve a configured printer to a transport URI the PJL engine understands.
// Reads the same QSettings schema PrinterManagerWidget writes. Prefers an
// exact name match; otherwise falls back to the default printer. Returns an
// empty string and fills *err when nothing usable is configured (or the match
// is USB, which the raw-socket transport can't drive).
QString resolvePrinterUri(const QString& wantName, QString* err) {
    QSettings s;
    const int n = s.beginReadArray("printers");
    QString matchUri, defaultUri;
    bool matchUsb = false, anyConfigured = (n > 0);
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        const QString name = s.value("name").toString();
        const QString conn = s.value("connection", "tcp").toString();
        const QString host = s.value("host").toString();
        const int     port = s.value("port", 9100).toInt();
        const bool    isDefault = s.value("isDefault", false).toBool();

        QString uri;
        const bool usb = (conn == "usb");
        if (!usb && !host.isEmpty())
            uri = QStringLiteral("socket://%1:%2").arg(host).arg(port);

        if (isDefault && !uri.isEmpty()) defaultUri = uri;
        if (!wantName.isEmpty() && name == wantName) {
            matchUri = uri;
            matchUsb = usb;
        }
    }
    s.endArray();

    const QString uri = !matchUri.isEmpty() ? matchUri : defaultUri;
    if (uri.isEmpty()) {
        if (err) {
            *err = matchUsb
                ? QStringLiteral("Selected printer is USB; raw-socket transport "
                                 "needs a TCP/IP host. Switch it to TCP/IP in "
                                 "Printer settings.")
                : (anyConfigured
                       ? QStringLiteral("No reachable printer: set a host/IP "
                                        "for the default printer in settings.")
                       : QStringLiteral("No printer configured. Add one in "
                                        "Printer settings first."));
        }
        return {};
    }
    return uri;
}

// Tiny helper: draw a printer glyph since QStyle lacks one.
QIcon makePrintIcon() {
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen pen(QColor("#ffffff"));
    pen.setWidth(1);
    p.setPen(pen);
    // Body
    p.setBrush(QColor("#d6d6d6"));
    p.drawRect(2, 6, 12, 6);
    // Paper top sticking out
    p.setBrush(QColor("#ffffff"));
    p.drawRect(4, 2, 8, 5);
    // Output tray
    p.setBrush(QColor("#ffffff"));
    p.drawRect(4, 10, 8, 4);
    // LED
    p.setBrush(QColor("#2f8c2f"));
    p.setPen(Qt::NoPen);
    p.drawRect(11, 8, 2, 1);
    return QIcon(pm);
}

QIcon makeAddIcon() {
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor("#1a8c2f"));
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.drawLine(8, 3, 8, 13);
    p.drawLine(3, 8, 13, 8);
    return QIcon(pm);
}

} // anon

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("jcolour");
    resize(1360, 860);
    setMinimumSize(1120, 720);   // keep canvas + 300px inspector from overlapping

    model_ = new JobListModel(this);

    // Build sidebar + stacked pages laid out side-by-side as central widget
    mainSidebar_ = new jcolour::MainSidebar;

    // Stack order must match MainSidebar::Page enum (4 items).
    moduleStack_ = new QStackedWidget;
    moduleStack_->addWidget(buildJobsPage());                       // 0 工作
    moduleStack_->addWidget(new jcolour::PageHistory);              // 1 工作記錄
    moduleStack_->addWidget(new QWidget);                           // 2 設定（佔位：實際走模態對話框）
    moduleStack_->addWidget(new jcolour::CalibrationPage);          // 3 建檔校準（配置庫 + 校準流程）

    auto* central = new QWidget;
    auto* shell = new QHBoxLayout(central);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);
    shell->addWidget(mainSidebar_);
    shell->addWidget(moduleStack_, 1);
    setCentralWidget(central);

    connect(mainSidebar_, &jcolour::MainSidebar::pageRequested, this, [this](int page) {
        if (page == jcolour::MainSidebar::PageSettings) {
            openSettingsDialog();
            mainSidebar_->setActive(static_cast<jcolour::MainSidebar::Page>(lastPage_));
            return;
        }
        lastPage_ = page;
        moduleStack_->setCurrentIndex(page);
    });

    mainSidebar_->setActive(jcolour::MainSidebar::PageJobs);
    moduleStack_->setCurrentIndex(jcolour::MainSidebar::PageJobs);

    buildMenus();
    buildStatusBar();

    connect(printTypeBar_, &PrintTypeBar::typeSelected,
            this, &MainWindow::onPrintTypeSelected);
    connect(printInspector_, &jcolour::ui::PrintInspector::jobModified,
            this, &MainWindow::onInspectorModified);
    connect(model_, &JobListModel::jobAdded, this, [this](int) {
        statTotal_->setText(QString(TR("Total") + " %1").arg(model_->jobCount()));
    });

    connect(&jcolour::Translator::instance(), &jcolour::Translator::languageChanged,
            this, &MainWindow::onLanguageChanged);

    statTotal_->setText(TR("Total") + " 0");
}

MainWindow::~MainWindow() = default;

void MainWindow::selectPageForShot(int page) {
    if (page == jcolour::MainSidebar::PageSettings) { openSettingsDialog(); return; }
    lastPage_ = page;
    mainSidebar_->setActive(static_cast<jcolour::MainSidebar::Page>(page));
    moduleStack_->setCurrentIndex(page);
}

QWidget* MainWindow::buildJobsPage() {
    auto* page = new QWidget;
    auto* col = new QVBoxLayout(page);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    // Top: the 5 print-type cards (Style A · Mirage).
    printTypeBar_ = new PrintTypeBar;

    // Context bar: printer + paper pickers styled as pills (design mockup .ctxbar).
    auto* ctx = new QWidget;
    ctx->setObjectName("ctxBar");
    ctx->setStyleSheet(
        "#ctxBar{background:#fafafb;border-bottom:1px solid #d2d5d9;}"
        "#ctxBar QLabel{color:#868c95;font-size:12px;background:transparent;}"
        "#ctxBar QComboBox{background:#fff;border:1px solid #d2d5d9;border-radius:7px;"
        "  padding:5px 11px;font-size:12.5px;color:#23262b;}"
        "#ctxBar QComboBox::drop-down{border:none;width:16px;}"
        "#ctxBar QComboBox::down-arrow{image:none;border-left:4px solid transparent;"
        "  border-right:4px solid transparent;border-top:5px solid #b0b5bd;margin-right:6px;}");
    auto* ctxLay = new QHBoxLayout(ctx);
    ctxLay->setContentsMargins(16, 9, 16, 9);
    ctxLay->setSpacing(8);
    auto* pl = new QLabel(TR("Printer"));
    printerCombo_ = new QComboBox; printerCombo_->addItem("P9500");
    auto* ql = new QLabel(TR("Paper"));
    paperCombo_ = new QComboBox; paperCombo_->setMinimumWidth(220);
    ctxLay->addWidget(pl); ctxLay->addWidget(printerCombo_);
    ctxLay->addSpacing(8);
    ctxLay->addWidget(ql); ctxLay->addWidget(paperCombo_);
    ctxLay->addStretch();
    auto* spPill = new QLabel(QStringLiteral("輸出  24″ × 36″  ▾"));
    spPill->setStyleSheet("background:#fff;border:1px solid #d2d5d9;color:#23262b;"
                          "border-radius:7px;padding:5px 11px;font-size:12.5px;");
    ctxLay->addWidget(spPill);

    // Native shell rewrite (increment 1): entry to the print-environment
    // manager — the hub that connects printer + linearization + colour mgmt.
    auto* envBtn = new QPushButton(QStringLiteral("打印环境 / 校准…"));
    envBtn->setStyleSheet("background:#eaf2fc;border:1px solid #bcd4f2;color:#1f5fa8;"
                          "border-radius:7px;padding:5px 11px;font-size:12.5px;font-weight:600;");
    connect(envBtn, &QPushButton::clicked, this, [this] {
        jcolour::ui::PrintEnvironmentManager dlg(this);
        dlg.exec();
    });
    ctxLay->addWidget(envBtn);

    // Work area: a single image canvas (checkerboard, drag-drop) + collapsible
    // inspector — matching design/ui-mockup.html. No job table.
    preview_ = new ImagePreviewWidget;

    // Preview toolbar: soft-proof / paper-white / gamut / info toggles (Mirage).
    auto* ptools = new QWidget;
    ptools->setObjectName("ptools");
    ptools->setStyleSheet("#ptools{background:#f4f5f7;border-bottom:1px solid #d2d5d9;}"
                          "#ptools QCheckBox{font-size:11.5px;color:#4a4e55;background:transparent;}"
                          "#ptools QLabel{font-size:11.5px;color:#868c95;background:transparent;}");
    auto* ptLay = new QHBoxLayout(ptools);
    ptLay->setContentsMargins(14, 6, 14, 6);
    ptLay->setSpacing(16);
    for (const QString& nm : {QStringLiteral("軟打樣"), QStringLiteral("紙白模擬"),
                              QStringLiteral("色域警告"), QStringLiteral("顯示資訊")}) {
        auto* cb = new QCheckBox(nm);
        cb->setChecked(nm != QStringLiteral("色域警告"));
        ptLay->addWidget(cb);
    }
    ptLay->addStretch();
    ptLay->addWidget(new QLabel(QStringLiteral("⊟ 符合視窗 · 100%")));

    auto* canvas = new QWidget;
    canvas->setStyleSheet("background:#7a7a7a;");
    auto* canvasLay = new QVBoxLayout(canvas);
    canvasLay->setContentsMargins(0, 0, 0, 0);
    canvasLay->setSpacing(0);
    canvasLay->addWidget(ptools);
    canvasLay->addWidget(preview_, 1);

    printInspector_ = new jcolour::ui::PrintInspector;
    preview_->setSoftProofingWidget(printInspector_->softProof());

    auto* row = new QWidget;
    auto* rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(0);
    rowLay->addWidget(canvas, 1);
    rowLay->addWidget(printInspector_);

    // Pre-print status gate — pinned as a full-width bar at the bottom of the
    // Print page (spanning canvas + inspector), separate from the params.
    gateBar_ = new jcolour::PrintGateBar;
    connect(gateBar_, &jcolour::PrintGateBar::printRequested, this, &MainWindow::onPrint);
    connect(gateBar_, &jcolour::PrintGateBar::recalibrateRequested, this, [this] {
        mainSidebar_->setActive(jcolour::MainSidebar::PageColorManagement);
        moduleStack_->setCurrentIndex(jcolour::MainSidebar::PageColorManagement);
    });

    col->addWidget(printTypeBar_);
    col->addWidget(ctx);
    col->addWidget(row, 1);
    col->addWidget(gateBar_);

    reloadPaperCombo();
    connect(paperCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ updateGate(); });
    connect(printerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ updateGate(); });
    updateGate();
    return page;
}

void MainWindow::reloadPaperCombo() {
    if (!paperCombo_) return;
    paperCombo_->blockSignals(true);
    paperCombo_->clear();
    QSet<QString> seen;
    for (const auto& c : jcolour::PaperState::instance().listConfigs()) {
        if (!c.enabled || seen.contains(c.paperId)) continue;
        seen.insert(c.paperId);
        paperCombo_->addItem(c.paperName, c.paperId);  // data = paperId
    }
    paperCombo_->blockSignals(false);
}

void MainWindow::updateGate() {
    if (!gateBar_ || !paperCombo_ || !printTypeBar_) return;
    const QString paperId = paperCombo_->currentData().toString();
    const QString printer = printerCombo_ ? printerCombo_->currentText() : "P9500";
    int wf = printTypeBar_->currentTypeId();
    if (wf < 0) wf = 1;
    auto mode = static_cast<jcolour::PrintMode>(wf);  // workflow id == PrintMode value
    // ink batch: take from the matching config so demo batch (#A6) flows through
    QString inkBatch = "#A7";
    for (const auto& c : jcolour::PaperState::instance().listConfigs())
        if (c.enabled && c.paperId == paperId) { inkBatch = c.inkBatch; break; }
    auto g = jcolour::PaperState::instance().evaluateGate(printer, paperId, inkBatch, mode);
    gateBar_->setGate(g);
}

QWidget* MainWindow::buildSettingsTabs() {
    // 「設定」收納印表機設定 / 儀表板 / 墨水管理 / 量測設備四個分頁。
    // Hosted inside a modal dialog (not the sidebar stack) so the main window
    // stays single-purpose; the shared pageStyle() cascades into all sub-pages.
    auto* host = new QWidget;
    host->setObjectName("pageSettings");
    host->setStyleSheet("#pageSettings{background:#f6f7f9;}" + jcolour::theme::pageStyle());
    auto* col = new QVBoxLayout(host);
    col->setContentsMargins(22, 18, 22, 18);
    col->setSpacing(12);

    auto* tabs = new QTabWidget;
    tabs->setDocumentMode(true);
    tabs->addTab(new jcolour::PrinterManagerWidget,    TR("Printer Settings"));
    tabs->addTab(new jcolour::PageDashboard,           TR("Dashboard"));
    tabs->addTab(new jcolour::PageInkManagement,       TR("Printer Ink Management"));
    tabs->addTab(new jcolour::PageMeasurementDevices,  TR("Measurement Devices"));
    col->addWidget(tabs, 1);
    return host;
}

void MainWindow::openSettingsDialog() {
    if (!settingsModal_) {
        settingsModal_ = new QDialog(this);
        settingsModal_->setWindowTitle(TR("Settings"));
        settingsModal_->resize(920, 640);
        auto* dl = new QVBoxLayout(settingsModal_);
        dl->setContentsMargins(0, 0, 0, 0);
        dl->addWidget(buildSettingsTabs());
    }
    settingsModal_->show();
    settingsModal_->raise();
    settingsModal_->activateWindow();
}

void MainWindow::buildMenus() {
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("Open Image…", QKeySequence::Open, this, &MainWindow::onOpenImage);
    fileMenu->addSeparator();
    fileMenu->addAction("Print…", QKeySequence::Print, this, &MainWindow::onPrint);
    fileMenu->addSeparator();
    fileMenu->addAction("&Quit", qApp, &QApplication::quit, QKeySequence::Quit)
        ->setMenuRole(QAction::QuitRole);

    auto* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction("Undo", QKeySequence::Undo, [] {});
    editMenu->addAction("Redo", QKeySequence::Redo, [] {});

    auto* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction("Toggle Inspector", [this] {
        if (printInspector_) printInspector_->setVisible(!printInspector_->isVisible());
    });
    viewMenu->addAction("Toggle Print-Type Bar", [this] {
        if (printTypeBar_) printTypeBar_->setVisible(!printTypeBar_->isVisible());
    });

    auto* toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->addAction("Measure Target (i1Pro3)…", this, &MainWindow::onMeasure);
    toolsMenu->addSeparator();
    toolsMenu->addAction(TR("Settings…"), this, &MainWindow::onSettings);

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("About jcolour", [this] {
        QMessageBox::about(this, "About jcolour",
            "jcolour 0.1.0\nB&W & RGB color-managed print workstation\n"
            "Epson SureColor P-series");
    });
}

void MainWindow::buildToolbar() {
    auto* tb = addToolBar("Main");
    tb->setMovable(false);
    tb->setFloatable(false);
    tb->setIconSize({16, 16});
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    QStyle* st = style();
    auto add = [&](const QIcon& icon, const QString& text, auto slot) {
        auto* a = tb->addAction(icon, text);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };

    add(st->standardIcon(QStyle::SP_DirOpenIcon), "Open",  &MainWindow::onOpenImage);
    add(makeAddIcon(),                            "Add",   [this]{ onOpenImage(); });
    tb->addSeparator();
    add(makePrintIcon(),                          "Print", &MainWindow::onPrint);
    add(st->standardIcon(QStyle::SP_MediaPause),  "Hold",  [this]{
        if (currentJobRow_ < 0) return;
        auto j = model_->jobAt(currentJobRow_);
        j.status = JobStatus::Held;
        model_->updateJob(currentJobRow_, j);
        statusBar()->showMessage("Held " + j.id, 3000);
    });
    add(st->standardIcon(QStyle::SP_BrowserStop), "Cancel", [this]{
        if (currentJobRow_ < 0) return;
        auto j = model_->jobAt(currentJobRow_);
        j.status = JobStatus::Failed;
        model_->updateJob(currentJobRow_, j);
        statusBar()->showMessage("Cancelled " + j.id, 3000);
    });
    tb->addSeparator();
    add(st->standardIcon(QStyle::SP_DialogApplyButton), "Approve", [this]{
        if (currentJobRow_ < 0) return;
        auto j = model_->jobAt(currentJobRow_);
        j.status = JobStatus::Completed;
        model_->updateJob(currentJobRow_, j);
        statusBar()->showMessage("Approved " + j.id, 3000);
    });
    add(st->standardIcon(QStyle::SP_DialogCancelButton), "Reject", [this]{
        if (currentJobRow_ < 0) return;
        auto j = model_->jobAt(currentJobRow_);
        j.status = JobStatus::Held;
        model_->updateJob(currentJobRow_, j);
        statusBar()->showMessage("Rejected " + j.id, 3000);
    });

    auto* spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);
}

void MainWindow::buildStatusBar() {
    auto* bar = statusBar();
    bar->setSizeGripEnabled(false);

    auto badge = [](const QString& key, const QString& initial) {
        auto* l = new QLabel(initial);
        l->setObjectName(key);
        return l;
    };
    statTotal_    = badge("badgeNeutral", "Total 0");
    statHeld_     = badge("badgeWarn",    "Held 0");
    statPrinting_ = badge("badgeOk",      "Printing 0");

    auto sep = []() {
        auto* l = new QLabel("·");
        l->setStyleSheet("color:#a8a8a8;");
        return l;
    };

    bar->addWidget(statTotal_);
    bar->addWidget(sep());
    bar->addWidget(statHeld_);
    bar->addWidget(sep());
    bar->addWidget(statPrinting_);

    printerBadge_ = new ConnectionBadge("Printer");
    printerBadge_->setSubtitle("Epson SureColor P9500");
    printerBadge_->setState(ConnectionBadge::Disconnected);
    bar->addPermanentWidget(printerBadge_);
}

// ----------------- Slots -----------------

void MainWindow::onOpenImage() {
    QString path = QFileDialog::getOpenFileName(this,
        "Open Image", QString(),
        "Images (*.tif *.tiff *.jpg *.jpeg *.png);;All Files (*)");
    if (path.isEmpty()) return;

    Job j;
    QFileInfo fi(path);
    j.id            = QString::number(model_->jobCount() + 1).rightJustified(5, '0');
    j.imagePath     = path;
    j.workflowName  = printTypeBar_->currentTypeName();
    j.fileSizeBytes = fi.size();
    j.colorSpace    = "RGB";
    j.printerName   = "Epson SureColor P9500";
    j.status        = JobStatus::Spooled;
    j.submittedAt   = QDateTime::currentDateTime();

    QImage thumb(path);
    if (!thumb.isNull()) {
        j.thumbnail = thumb.scaled(48, 32, Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
    }

    model_->addJob(j);
    currentJobRow_ = model_->jobCount() - 1;
    if (preview_) preview_->loadImage(path);
    statusBar()->showMessage("Image loaded: " + fi.fileName(), 4000);
}

void MainWindow::onPrint() {
    if (currentJobRow_ < 0) {
        QMessageBox::information(this, "No Job Selected",
            "Select a job in the table below before printing.");
        return;
    }
    auto j = model_->jobAt(currentJobRow_);
    j.status = JobStatus::Printing;
    model_->updateJob(currentJobRow_, j);
    statPrinting_->setText(QString("Printing %1")
        .arg(model_->countByStatus(JobStatus::Printing)));
    statusBar()->showMessage("Sent " + j.id + " to " + j.printerName, 4000);
}

void MainWindow::onMeasure() {
    statusBar()->showMessage(TR("Waiting for i1Pro3…"), 4000);
}

void MainWindow::onSettings() {
    if (!settingsDlg_) settingsDlg_ = new jcolour::SettingsDialog(this);
    settingsDlg_->show();
    settingsDlg_->raise();
    settingsDlg_->activateWindow();
}

void MainWindow::onLanguageChanged() {
    setWindowTitle("jcolour"); // TODO: translate
    buildMenus();
    buildStatusBar();
}

void MainWindow::onJobSelected(int row) {
    currentJobRow_ = row;
}

void MainWindow::onInspectorModified() {
    // Inspector edits affect colour/soft-proof settings; refresh the gate.
    updateGate();
}

void MainWindow::onPrintTypeSelected(int /*workflowId*/, const QString& name) {
    Q_UNUSED(name);
    // Selecting a print type starts an import so the user can drop a file
    // straight into the chosen workflow.
    updateGate();
    onOpenImage();
}

