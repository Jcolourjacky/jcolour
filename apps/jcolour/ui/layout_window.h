#pragma once

// LayoutWindow — the CorelDraw/ErgoSoft-style native shell (design truth:
// design/jcolour_optimized.html). One window: menubar → std toolbar
// (printer → paper → size + env manager + print) → dual-row property bar
// (object / text, switches with selection) → toolbox | canvas (mm rulers,
// zoom, draggable + handle-resizable blocks, text/QR insert tools) | right
// panel (图片/环境/预检/队列 with the real PrintQueueWidget) → job strip
// (thumbs + traffic light + print) → status bar.
//
// Engine wiring: per-mode calibration freshness via
// calibration::compute_state(); paper list via PaperState::listConfigs();
// auto-nest via NestingWidget::nest(); 打印 submits real queue::PrintJobs.

#include <QMainWindow>
#include <QVector>
#include <QColor>
#include <QString>
#include <QPointF>
#include <QRectF>

class QComboBox;
class QLabel;
class QStackedWidget;
class QTabWidget;
class QDoubleSpinBox;
class QPushButton;
class QToolButton;
class PrintQueueWidget;

namespace jcolour::ui {

// ---- canvas ---------------------------------------------------------------

struct CanvasBlock {
    double  x = 0, y = 0, w = 0, h = 0;   // mm on paper
    QColor  color;
    QString name;
    QString modeKey;                       // mono/gray/tone/rgb/rgb16 ("" for text)
    bool    isText = false;
    QString text;                          // text blocks only
    bool    isQr = false;
    QString qrData;                        // QR blocks only
};

enum class CanvasTool { Select, Text, Qr };

class LayoutCanvas : public QWidget {
    Q_OBJECT
public:
    explicit LayoutCanvas(QWidget* parent = nullptr);

    QVector<CanvasBlock>&       blocks()       { return blocks_; }
    const QVector<CanvasBlock>& blocks() const { return blocks_; }

    int  selected() const { return sel_; }
    void select(int idx);

    void setTool(CanvasTool t) { tool_ = t; }

    void zoomBy(int deltaPercent);   // ±10 …
    void zoomFit();
    int  zoomPercent() const;        // 100 == paper at 80% of viewport width

signals:
    void selectionChanged(int idx);  // -1 = none
    void blockMoved(int idx);        // moved or resized
    void blockAdded(int idx);        // text / QR tool inserted a block
    void toolFinished();             // insert tool done → revert to select
    void zoomChanged(int percent);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    QRectF  paperRectPx() const;
    QRectF  blockRectPx(const CanvasBlock& b) const;
    QPointF pxToMm(const QPointF& px) const;
    int     handleAt(const QPointF& px) const;   // 0..7 on selection, -1 none
    void    clampScroll();

    QVector<CanvasBlock> blocks_;
    int    sel_   = 0;
    CanvasTool tool_ = CanvasTool::Select;
    double zoom_  = 0.0;     // paper width as fraction of viewport width; 0 = fit on first paint
    double scroll_ = 0.0;    // px
    bool   dragging_ = false;
    int    resizing_ = -1;   // active handle index, -1 = not resizing
    QRectF startMm_;         // block rect (mm) when resize started
    QPointF dragOffsetMm_;
};

// ---- window ---------------------------------------------------------------

class LayoutWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit LayoutWindow(QWidget* parent = nullptr);

    // Screenshot helper: 0..3 = right-panel tab (图片/环境/预检/队列).
    void selectTabForShot(int tab);
    // Screenshot/automation helper: "submit" = 打印整版, "nest" = 自动拼版.
    void runActionForShot(const QString& action);

private:
    // builders
    void buildMenus();
    QWidget* buildToolbar();
    QWidget* buildPropBars();
    QWidget* buildToolbox();
    QWidget* buildRightPanel();
    QWidget* buildJobStrip();
    void buildStatusBar();
    QWidget* buildTrafficLight();

    // sync
    void onSelectionChanged(int idx);
    void syncPropBarFromBlock(int idx);
    void onModeChosen(const QString& key); // per-image mode (5 选 1)
    void updatePreflight();
    void openEnvManager();

    // real-module wiring
    void submitLayoutToQueue();            // 打印 / 打印整版 → queue::PrintJob × N
    void runAutoNest();                    // 自动拼版 → NestingWidget::nest()

    // helpers
    int  modeDays(const QString& key) const;
    bool modeStale(const QString& key) const;

    LayoutCanvas*  canvas_      = nullptr;
    QStackedWidget* propStack_  = nullptr;     // 0 = object row, 1 = text row
    QTabWidget*    rightTabs_   = nullptr;
    PrintQueueWidget* queueWidget_ = nullptr;  // the real queue module, embedded
    QToolButton*   selectToolBtn_ = nullptr;   // toolbox 选择 (revert target)

    // toolbar combos
    QComboBox* printerCombo_ = nullptr;
    QComboBox* paperCombo_   = nullptr;
    QComboBox* sizeCombo_    = nullptr;

    // object prop bar fields
    QDoubleSpinBox *px_ = nullptr, *py_ = nullptr, *pw_ = nullptr, *ph_ = nullptr;
    QPushButton* warnChip_ = nullptr;

    // 图片 tab
    QLabel *imgHd_ = nullptr, *imgSize_ = nullptr, *imgIcc_ = nullptr,
           *imgDepth_ = nullptr, *imgDpi_ = nullptr, *modeHint_ = nullptr;
    QVector<QPushButton*> modeBtns_;
    QStackedWidget* adjustStack_ = nullptr;

    // 预检 tab
    QLabel *pfBanner_ = nullptr, *pfCal_ = nullptr;
    QPushButton* pfCalFix_ = nullptr;

    // status bar
    QLabel *sbSel_ = nullptr, *sbCal_ = nullptr;

    bool syncing_ = false;
};

}  // namespace jcolour::ui
