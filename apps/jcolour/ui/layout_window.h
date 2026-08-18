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
class QTextEdit;
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

    // 分幅 Tiling（ErgoSoft JobComposer 语义）：超出幅宽的图拆成
    // 行×列 幅，相邻幅之间留重叠搭接；可交替旋转 180° 补偿喷头方向差。
    int     tileCols = 1, tileRows = 1;
    double  tileOverlapMm = 0;
    bool    tileAlternate = false;
    bool    tiled() const { return tileCols > 1 || tileRows > 1; }
};

enum class CanvasTool { Select, Text, Qr, Hand, Measure };

class LayoutCanvas : public QWidget {
    Q_OBJECT
public:
    explicit LayoutCanvas(QWidget* parent = nullptr);

    QVector<CanvasBlock>&       blocks()       { return blocks_; }
    const QVector<CanvasBlock>& blocks() const { return blocks_; }

    int  selected() const { return sel_; }
    void select(int idx);
    const QVector<int>& selectionList() const { return multiSel_; }   // 含主选中

    void setTool(CanvasTool t);          // 切工具（提交进行中的文字编辑并发 toolChanged）
    CanvasTool tool() const { return tool_; }

    void zoomBy(int deltaPercent);   // ±10 …
    void zoomFit();
    int  zoomPercent() const;        // 100 == paper at 80% of viewport width
    void toggleRulers()    { showRulers_ = !showRulers_; update(); }
    void toggleGrid()      { showGrid_ = !showGrid_; update(); }
    void toggleCropMarks() { showCropMarks_ = !showCropMarks_; update(); }

    // 选中块操作（拼版编辑器标配：复制 / 份数 / 旋转 / 删除 / 对齐）
    void duplicateSelected();
    void duplicateSelectedN(int copies);   // ×N 份（阶梯错位，配合自动拼版）
    void rotateSelected90();
    void deleteSelected();
    // InDesign 对齐面板：对齐到 选区/页面 · 六向对齐 · 分布对象 · 分布间距
    enum class Align { Left, HCenter, Right, Top, VCenter, Bottom };
    enum class AlignTo { Selection, Page };
    void setAlignTo(AlignTo t) { alignTo_ = t; }
    AlignTo alignTo() const { return alignTo_; }
    void alignSelected(Align a);
    void distributeSelected(bool horizontal, bool centers);  // centers=居中分布 / 否则等间距

    // InDesign 式标尺：单位 / 参考线
    enum class Unit { MM, CM, Inch };
    void setUnit(Unit u) { unit_ = u; update(); }
    void clearGuides()   { guidesV_.clear(); guidesH_.clear(); update(); }
    void addGuideV(double mm) { guidesV_.push_back(mm); update(); }
    void addGuideH(double mm) { guidesH_.push_back(mm); update(); }
    void setRulerOrigin(double xmm, double ymm) { rulerOrigin_ = {xmm, ymm}; update(); }
    void toggleGuides()  { showGuides_ = !showGuides_; update(); }   // Cmd+;

    // InDesign「编辑 > 多重复制」：以选中块为原点铺 行×列（含间距）
    void stepRepeatGrid(int rows, int cols, double gapXmm, double gapYmm);
    // 光标锚定缩放（Ctrl+滚轮 / 捏合都锚在光标下的点）
    void zoomAtBy(const QPointF& anchorPx, int deltaPercent);
    // InDesign 式文字框编辑（拖框建立 → 框内直接输入；双击再编辑）
    void startTextEdit(int idx);
    void finishTextEdit(bool commit);

signals:
    void selectionChanged(int idx);  // -1 = none
    void blockMoved(int idx);        // moved or resized
    void blockAdded(int idx);        // text / QR tool inserted a block
    void toolFinished();             // insert tool done → revert to select
    void toolChanged(CanvasTool t);  // 键盘 V/T/H/K 或代码切换 → 工具条同步选中态
    void zoomChanged(int percent);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    bool event(QEvent*) override;    // 触控板捏合缩放 (ZoomNativeGesture)
    void resizeEvent(QResizeEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;   // 标尺右键：单位/清参考线
    void keyPressEvent(QKeyEvent*) override;              // V/T/H/K 切工具 · 空格临时抓手 · Delete
    void keyReleaseEvent(QKeyEvent*) override;            // 空格抬起还原工具
    void mouseDoubleClickEvent(QMouseEvent*) override;    // 双击标尺加参考线 · 双击文字块编辑
    bool eventFilter(QObject*, QEvent*) override;         // 文字编辑框失焦/Esc 提交

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
    bool   showRulers_ = true;
    bool   showGrid_ = false;
    bool   showCropMarks_ = false;
    double scrollX_ = 0.0;           // 缩放超视口后的横向平移（抓手/滚轮）

    // InDesign 式标尺
    Unit   unit_ = Unit::MM;
    QPointF rulerOrigin_{0, 0};      // 零位（纸面 mm）；角落方块拖出调整，双击复位
    bool    draggingOrigin_ = false;
    QVector<double> guidesV_, guidesH_;   // 参考线（纸面 mm）
    int    dragGuideKind_ = 0;            // 0 无 · 1 竖(V) · 2 横(H)
    int    dragGuideIdx_  = -1;           // -1 = 正在从标尺拖出新线
    QPointF cursorPx_{-1, -1};            // 标尺光标位置指示

    // 测量工具
    bool    measuring_ = false, hasMeasure_ = false;
    QPointF measA_, measB_;               // mm
    QPointF panLast_;                     // 抓手拖动
    bool    panning_ = false;
    bool    showGuides_ = true;

    // 空格临时抓手（InDesign/Photoshop 通用手势）
    bool       spacePan_ = false;
    CanvasTool prevTool_ = CanvasTool::Select;

    // 多选（InDesign 选择模型：Shift 加选 · 空白处框选 · 群体拖动）
    QVector<int>     multiSel_;      // 含 sel_；单选时 size==1
    QVector<QPointF> dragStart_;     // 群移起始位置 (mm)，与 multiSel_ 对应
    bool    marquee_ = false;
    QPointF marqueeA_, marqueeB_;    // px
    AlignTo alignTo_ = AlignTo::Selection;

    // InDesign 式文字框
    bool    creatingText_ = false;
    QPointF textStartMm_;
    QRectF  textDragMm_;
    QTextEdit* editor_ = nullptr;
    int     editBlock_ = -1;
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
    void openJobCostDialog();   // 作业核算：介质长度/墨耗/成本估算（ErgoSoft CostManager 式）

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
