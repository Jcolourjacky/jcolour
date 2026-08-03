#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QTabWidget>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDockWidget>
#include <QFileDialog>
#include <QImage>
#include <QPixmap>
#include <QLabel>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

namespace jcolour {
namespace ui {

// ============================================================================
// Main Window
// ============================================================================

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void on_open_file();
    void on_print();
    void on_self_test();
    void on_calibrate();
    void on_settings();
    void on_tab_changed(int index);

private:
    void setup_ui();
    void setup_menubar();
    void setup_tabs();
    void setup_status_bar();
    void update_calibration_status();

    // Central widget
    QWidget* central_widget_;
    QVBoxLayout* central_layout_;

    // Tabs
    QTabWidget* tab_widget_;
    QWidget* bw_tab_;
    QWidget* color_tab_;
    QWidget* proof_tab_;

    // Image preview
    QLabel* image_preview_;
    QLabel* paper_label_;
    QLabel* ink_label_;
    QLabel* status_label_;

    // Buttons
    QPushButton* print_button_;
    QPushButton* self_test_button_;
    QPushButton* calibrate_button_;

    // Status bar
    QLabel* status_bar_label_;
};

// ============================================================================
// B&W Tab (流程选择)
// ============================================================================

class BWTab : public QWidget {
    Q_OBJECT

public:
    explicit BWTab(QWidget* parent = nullptr);

signals:
    void workflow_selected(int workflow);

private slots:
    void on_single_black();
    void on_multi_gray();
    void on_gray_tone();

private:
    QLabel* description_label_;
};

// ============================================================================
// Color Tab
// ============================================================================

class ColorTab : public QWidget {
    Q_OBJECT

public:
    explicit ColorTab(QWidget* parent = nullptr);

signals:
    void workflow_selected(int workflow);

private slots:
    void on_rgb_12ch();
    void on_prophoto();

private:
    QLabel* description_label_;
};

// ============================================================================
// Implementation
// ============================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , central_widget_(nullptr)
    , central_layout_(nullptr)
    , tab_widget_(nullptr)
    , image_preview_(nullptr)
    , status_label_(nullptr) {

    setup_ui();
    setup_menubar();
    setup_tabs();
    setup_status_bar();

    setWindowTitle("jcolour");
    setMinimumSize(800, 600);
    resize(1024, 768);

    update_calibration_status();
}

MainWindow::~MainWindow() = default;

void MainWindow::setup_ui() {
    central_widget_ = new QWidget(this);
    central_layout_ = new QVBoxLayout(central_widget_);

    // Image preview (drop zone)
    image_preview_ = new QLabel("拖入照片 / Drop image here\n\n支持 TIFF, JPEG, PSD", central_widget_);
    image_preview_->setAlignment(Qt::AlignCenter);
    image_preview_->setMinimumSize(400, 300);
    image_preview_->setStyleSheet(R"(
        QLabel {
            border: 2px dashed #888;
            border-radius: 8px;
            background: #f5f5f5;
            color: #666;
            font-size: 14px;
        }
    )");
    central_layout_->addWidget(image_preview_, 1);

    // Paper/ink info
    QHBoxLayout* info_layout = new QHBoxLayout();

    paper_label_ = new QLabel("纸张: <未选择>");
    ink_label_ = new QLabel("墨批: <未检测>");
    status_label_ = new QLabel("状态: ● UNC");

    info_layout->addWidget(paper_label_);
    info_layout->addWidget(ink_label_);
    info_layout->addWidget(status_label_);
    info_layout->addStretch();

    central_layout_->addLayout(info_layout);

    // Action buttons
    QHBoxLayout* button_layout = new QHBoxLayout();

    print_button_ = new QPushButton("开始打印 / Print");
    self_test_button_ = new QPushButton("自检 / Self-Test");
    calibrate_button_ = new QPushButton("校准 / Calibrate");

    print_button_->setEnabled(false);  // Disabled until image loaded
    calibrate_button_->setEnabled(false);  // Disabled until printer connected

    connect(print_button_, &QPushButton::clicked, this, &MainWindow::on_print);
    connect(self_test_button_, &QPushButton::clicked, this, &MainWindow::on_self_test);
    connect(calibrate_button_, &QPushButton::clicked, this, &MainWindow::on_calibrate);

    button_layout->addWidget(print_button_);
    button_layout->addWidget(self_test_button_);
    button_layout->addWidget(calibrate_button_);
    button_layout->addStretch();

    central_layout_->addLayout(button_layout);

    setCentralWidget(central_widget_);

    // Accept drops
    setAcceptDrops(true);
}

void MainWindow::setup_menubar() {
    QMenuBar* menubar = menuBar();

    // File menu
    QMenu* file_menu = menubar->addMenu("文件 / File");

    QAction* open_action = new QAction("打开 / Open...", this);
    open_action->setShortcut(QKeySequence::Open);
    connect(open_action, &QAction::triggered, this, &MainWindow::on_open_file);
    file_menu->addAction(open_action);

    file_menu->addSeparator();

    QAction* quit_action = new QAction("退出 / Quit", this);
    quit_action->setShortcut(QKeySequence::Quit);
    connect(quit_action, &QAction::triggered, qApp, &QApplication::quit);
    file_menu->addAction(quit_action);

    // Print menu
    QMenu* print_menu = menubar->addMenu("打印 / Print");

    QAction* print_action = new QAction("打印 / Print", this);
    print_action->setShortcut(QKeySequence::Print);
    connect(print_action, &QAction::triggered, this, &MainWindow::on_print);
    print_menu->addAction(print_action);

    // Calibration menu
    QMenu* calib_menu = menubar->addMenu("校准 / Calibration");

    QAction* self_test_action = new QAction("自检 / Self-Test", this);
    connect(self_test_action, &QAction::triggered, this, &MainWindow::on_self_test);
    calib_menu->addAction(self_test_action);

    QAction* calibrate_action = new QAction("校准向导 / Calibration Wizard", this);
    connect(calibrate_action, &QAction::triggered, this, &MainWindow::on_calibrate);
    calib_menu->addAction(calibrate_action);

    // Settings menu
    QMenu* settings_menu = menubar->addMenu("设置 / Settings");
    QAction* settings_action = new QAction("偏好设置 / Preferences", this);
    connect(settings_action, &QAction::triggered, this, &MainWindow::on_settings);
    settings_menu->addAction(settings_action);
}

void MainWindow::setup_tabs() {
    tab_widget_ = new QTabWidget(this);

    // B&W tab
    bw_tab_ = new BWTab(this);
    connect(bw_tab_, &BWTab::workflow_selected, this, &MainWindow::on_tab_changed);
    tab_widget_->addTab(bw_tab_, "黑白 / B&W");

    // Color tab
    color_tab_ = new ColorTab(this);
    tab_widget_->addTab(color_tab_, "彩色 / Color");

    // Proof tab (placeholder)
    proof_tab_ = new QWidget(this);
    proof_tab_->setLayout(new QVBoxLayout());
    proof_tab_->layout()->addWidget(new QLabel("打样 / Soft Proof", proof_tab_));
    tab_widget_->addTab(proof_tab_, "打样 / Proof");

    // Side panel (could be dock widget)
    // For now, just use tabs at top
    (void)tab_widget_;  // Placeholder
}

void MainWindow::setup_status_bar() {
    status_bar_label_ = new QLabel("就绪 / Ready");
    statusBar()->addWidget(status_bar_label_);
}

void MainWindow::update_calibration_status() {
    // TODO: Load from calibration DB
    status_label_->setText("状态: ● UNC");
    status_label_->setStyleSheet("color: gray;");
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mime_data = event->mimeData();
    if (mime_data->hasUrls()) {
        QUrl url = mime_data->urls().first();
        QString file_path = url.toLocalFile();

        QImage image(file_path);
        if (!image.isNull()) {
            image_preview_->setPixmap(QPixmap::fromImage(image.scaled(
                400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            image_preview_->setText("");
            print_button_->setEnabled(true);
            status_bar_label_->setText("已加载: " + file_path);
        }
    }
}

void MainWindow::on_open_file() {
    QString file = QFileDialog::getOpenFileName(this,
        "打开图片 / Open Image",
        "",
        "Images (*.tiff *.tif *.jpg *.jpeg *.png *.psd)");
    if (!file.isEmpty()) {
        QImage image(file);
        if (!image.isNull()) {
            image_preview_->setPixmap(QPixmap::fromImage(image.scaled(
                400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            image_preview_->setText("");
            print_button_->setEnabled(true);
            status_bar_label_->setText("已加载: " + file);
        }
    }
}

void MainWindow::on_print() {
    status_bar_label_->setText("正在打印... / Printing...");
    // TODO: Implement print pipeline
}

void MainWindow::on_self_test() {
    status_bar_label_->setText("正在自检... / Running self-test...");
    // TODO: Implement self-test
}

void MainWindow::on_calibrate() {
    status_bar_label_->setText("校准向导 / Calibration Wizard");
    // TODO: Implement calibration wizard
}

void MainWindow::on_settings() {
    status_bar_label_->setText("设置 / Settings");
    // TODO: Implement settings dialog
}

void MainWindow::on_tab_changed(int index) {
    (void)index;
    // Update UI based on selected workflow
}

// ============================================================================
// BWTab Implementation
// ============================================================================

BWTab::BWTab(QWidget* parent)
    : QWidget(parent) {

    QVBoxLayout* layout = new QVBoxLayout(this);

    description_label_ = new QLabel("选择黑白打印流程 / Select B&W workflow:", this);
    layout->addWidget(description_label_);

    QVBoxLayout* workflow_layout = new QVBoxLayout();

    // Single black
    QPushButton* single_black = new QPushButton(
        "单黑 (PK) / Single Black\n极致中性 · Extreme neutral", this);
    connect(single_black, &QPushButton::clicked, this, &BWTab::on_single_black);
    workflow_layout->addWidget(single_black);

    // Multi gray
    QPushButton* multi_gray = new QPushButton(
        "多级灰 (PK+MK+GY+LGY) / Multi Gray ★\n平滑层次 · Smooth gradients", this);
    connect(multi_gray, &QPushButton::clicked, this, &BWTab::on_multi_gray);
    workflow_layout->addWidget(multi_gray);

    // Gray + tone
    QPushButton* gray_tone = new QPushButton(
        "灰+调色 / Gray + Tone\n暖/冷/分裂调色 · Warm/Cold/Split-tone", this);
    connect(gray_tone, &QPushButton::clicked, this, &BWTab::on_gray_tone);
    workflow_layout->addWidget(gray_tone);

    layout->addLayout(workflow_layout);
    layout->addStretch();
}

void BWTab::on_single_black() {
    emit workflow_selected(1);
}

void BWTab::on_multi_gray() {
    emit workflow_selected(2);
}

void BWTab::on_gray_tone() {
    emit workflow_selected(3);
}

// ============================================================================
// ColorTab Implementation
// ============================================================================

ColorTab::ColorTab(QWidget* parent)
    : QWidget(parent) {

    QVBoxLayout* layout = new QVBoxLayout(this);

    description_label_ = new QLabel("选择彩色打印流程 / Select Color workflow:", this);
    layout->addWidget(description_label_);

    QVBoxLayout* workflow_layout = new QVBoxLayout();

    // RGB 12ch
    QPushButton* rgb_12ch = new QPushButton(
        "RGB 12通道 / RGB 12-channel\n大色域照片 · Wide gamut photos", this);
    connect(rgb_12ch, &QPushButton::clicked, this, &ColorTab::on_rgb_12ch);
    workflow_layout->addWidget(rgb_12ch);

    // ProPhoto
    QPushButton* prophoto = new QPushButton(
        "ProPhoto RGB\n极大色域 + Gamut Compression", this);
    connect(prophoto, &QPushButton::clicked, this, &ColorTab::on_prophoto);
    workflow_layout->addWidget(prophoto);

    layout->addLayout(workflow_layout);
    layout->addStretch();
}

void ColorTab::on_rgb_12ch() {
    emit workflow_selected(4);
}

void ColorTab::on_prophoto() {
    emit workflow_selected(6);
}

}  // namespace ui
}  // namespace jcolour

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("jcolour");
    app.setApplicationVersion("0.1.0");

    jcolour::ui::MainWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
