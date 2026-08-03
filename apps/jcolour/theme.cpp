#include "theme.h"
#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QFont>
#include <QFontInfo>

namespace jcolour::theme {

QString darkStyleSheet() {
    // Name kept for API stability; we now ship the EFI light palette.
    return QStringLiteral(R"qss(
/* ----- Base (Fiery XF JOB EDITOR — flat light palette) ----------------- */
QWidget {
    background-color: #e6e6e6;
    color: #1f1f1f;
    font-family: ".AppleSystemUIFont", "Segoe UI", "Helvetica Neue", sans-serif;
    font-size: 12px;
}
QMainWindow, QDialog { background-color: #e6e6e6; }

QToolTip {
    background: #ffffe1;
    color: #1f1f1f;
    border: 1px solid #909090;
    padding: 3px 6px;
}

/* ----- Menu / Status / ToolBar ----------------------------------------- */
QMenuBar { background: #e6e6e6; color: #1f1f1f; padding: 1px 2px;
           border-bottom: 1px solid #c4c4c4; }
QMenuBar::item { padding: 4px 10px; background: transparent; }
QMenuBar::item:selected { background: #2e7dd7; color: #1f1f1f; }

QMenu { background: #fafafa; color: #1f1f1f; border: 1px solid #a8a8a8; padding: 2px; }
QMenu::item { padding: 5px 22px; }
QMenu::item:selected { background: #2e7dd7; color: #1f1f1f; }
QMenu::separator { height: 1px; background: #c8c8c8; margin: 3px 8px; }

QStatusBar {
    background: #dcdcdc;
    color: #1f1f1f;
    border-top: 1px solid #b0b0b0;
    min-height: 24px;
}
QStatusBar::item { border: none; }

QToolBar {
    background: #e6e6e6;
    border: none;
    border-bottom: 1px solid #b8b8b8;
    spacing: 3px;
    padding: 4px 6px;
}
QToolBar::separator {
    background: #c0c0c0;
    width: 1px;
    margin: 5px 4px;
}
QToolButton {
    background: transparent;
    color: #1f1f1f;
    padding: 4px 6px;
    border: 1px solid transparent;
    border-radius: 3px;
}
QToolButton:hover { background: #d4e2f5; border: 1px solid transparent; }
QToolButton:pressed { background: #b8d0ee; }
QToolButton:checked { background: #cce0f7; border: 1px solid #6a99d4; }

/* ----- Module switcher (Job Explorer / System Manager) ----------------- */
QToolButton#moduleButton {
    padding: 5px 12px;
    font-weight: 500;
    border: 1px solid #b0b0b0;
    background: #f0f0f0;
    border-radius: 3px;
    margin: 0 2px;
}
QToolButton#moduleButton:hover {
    background: #e4ecf7;
    border-color: #6a99d4;
}
QToolButton#moduleButton:checked {
    background: #cce0f7;
    border-color: #2e7dd7;
    color: #1f1f1f;
}

/* ----- Buttons --------------------------------------------------------- */
QPushButton {
    background: #f5f5f5;
    color: #1f1f1f;
    border: 1px solid #a8a8a8;
    padding: 4px 12px;
    border-radius: 3px;
    min-height: 18px;
}
QPushButton:hover {
    background: #e8eef7;
    border-color: #6a99d4;
}
QPushButton:pressed {
    background: #c8d8ee;
    border-color: #2e7dd7;
}
QPushButton:default {
    background: #cce0f7;
    border-color: #2e7dd7;
}
QPushButton:disabled { color: #9a9a9a; background: #ececec; border-color: #c8c8c8; }

QPushButton#linkButton {
    background: #e4ecf7;
    border: 1px solid #6a99d4;
    color: #2e7dd7;
    padding: 2px 4px;
    min-width: 22px;
    min-height: 22px;
    max-width: 22px;
    max-height: 22px;
    border-radius: 3px;
}
QPushButton#linkButton:hover { background: #cce0f7; border-color: #2e7dd7; }
QPushButton#linkButton:checked { background: #b8d0ee; border-color: #2e7dd7; }

/* ----- Inputs ---------------------------------------------------------- */
QLineEdit, QSpinBox, QDoubleSpinBox, QTextEdit, QPlainTextEdit, QComboBox {
    background: #ffffff;
    color: #1f1f1f;
    border: 1px solid #a0a0a0;
    padding: 2px 5px;
    border-radius: 2px;
    selection-background-color: #2e7dd7;
    selection-color: #1f1f1f;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
    border-color: #2e7dd7;
}
QSpinBox, QDoubleSpinBox {
    padding-right: 14px;
}
QSpinBox::up-button, QDoubleSpinBox::up-button {
    subcontrol-origin: border;
    subcontrol-position: top right;
    width: 12px;
    background: #f0f0f0;
    border-left: 1px solid #c0c0c0;
    border-bottom: 1px solid #d0d0d0;
}
QSpinBox::down-button, QDoubleSpinBox::down-button {
    subcontrol-origin: border;
    subcontrol-position: bottom right;
    width: 12px;
    background: #f0f0f0;
    border-left: 1px solid #c0c0c0;
}
QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover { background: #d4e2f5; }
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
    image: none;
    width: 0; height: 0;
    border-left: 3px solid transparent;
    border-right: 3px solid transparent;
    border-bottom: 3px solid #555555;
}
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
    image: none;
    width: 0; height: 0;
    border-left: 3px solid transparent;
    border-right: 3px solid transparent;
    border-top: 3px solid #555555;
}

QComboBox::drop-down { border-left: 1px solid #a0a0a0; width: 16px;
                       background: #f0f0f0; }
QComboBox::drop-down:hover { background: #d4e2f5; }
QComboBox::down-arrow {
    image: none;
    border-left: 3px solid transparent;
    border-right: 3px solid transparent;
    border-top: 4px solid #555555;
    width: 0; height: 0;
    margin-right: 4px;
}
QComboBox QAbstractItemView {
    background: #ffffff;
    color: #1f1f1f;
    border: 1px solid #a0a0a0;
    selection-background-color: #2e7dd7;
    selection-color: #1f1f1f;
    outline: none;
}

QCheckBox { spacing: 6px; color: #1f1f1f; }
QCheckBox::indicator {
    width: 13px; height: 13px;
    background: #ffffff;
    border: 1px solid #767676;
    border-radius: 2px;
}
QCheckBox::indicator:checked { background: #2e7dd7; border-color: #2e7dd7;
                               image: none; }
QCheckBox::indicator:hover { border-color: #2e7dd7; }

QRadioButton { spacing: 6px; color: #1f1f1f; }
QRadioButton::indicator {
    width: 13px; height: 13px;
    background: #ffffff;
    border: 1px solid #767676;
    border-radius: 7px;
}
QRadioButton::indicator:checked {
    background: #2e7dd7;
    border: 4px solid #ffffff;
    border-radius: 7px;
}
QRadioButton::indicator:checked:!enabled { background: #999999; }

/* ----- Slider ---------------------------------------------------------- */
QSlider::groove:horizontal {
    height: 4px;
    background: #d0d0d0;
    border: 1px solid #a8a8a8;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    background: #fafafa;
    border: 1px solid #767676;
    width: 12px; height: 14px;
    margin: -6px 0;
    border-radius: 2px;
}
QSlider::handle:horizontal:hover {
    background: #e4ecf7;
    border-color: #2e7dd7;
}
QSlider::sub-page:horizontal { background: #6a99d4; border-radius: 2px; }

QSlider::groove:vertical {
    width: 4px; background: #d0d0d0; border: 1px solid #a8a8a8; border-radius: 2px;
}
QSlider::handle:vertical {
    background: #fafafa;
    border: 1px solid #767676;
    width: 14px; height: 12px;
    margin: 0 -6px;
    border-radius: 2px;
}

/* ----- Tabs ------------------------------------------------------------ */
QTabWidget::pane {
    border: 1px solid #b8b8b8;
    background: #ececec;
    top: -1px;
}
QTabBar { background: transparent; }
QTabBar::tab {
    background: #dcdcdc;
    color: #1f1f1f;
    padding: 5px 11px;
    border: 1px solid #b8b8b8;
    border-bottom: none;
    border-top-left-radius: 2px;
    border-top-right-radius: 2px;
    margin-right: 1px;
}
QTabBar::scroller { width: 0; }
QTabBar { qproperty-usesScrollButtons: false; }
QTabBar::tab:hover { background: #e4ecf7; }
QTabBar::tab:selected {
    background: #ececec;
    border-bottom: 1px solid #ececec;
    color: #1f1f1f;
    font-weight: 600;
}

/* ----- Group box (used as section heading) ---------------------------- */
QGroupBox {
    background: transparent;
    border: 1px solid #c8c8c8;
    border-radius: 2px;
    margin-top: 14px;
    padding-top: 10px;
    color: #1f1f1f;
    font-size: 11px;
    font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 8px;
    padding: 0 4px;
    background: #e6e6e6;
}

/* ----- ListView (workflow sidebar) ------------------------------------ */
QListView, QListWidget {
    background: #ffffff;
    border: 1px solid #b0b0b0;
    outline: none;
    color: #1f1f1f;
}
QListView::item, QListWidget::item {
    padding: 5px 10px;
    border-bottom: 1px solid #ececec;
}
QListView::item:hover, QListWidget::item:hover {
    background: #e4ecf7;
}
QListView::item:selected, QListWidget::item:selected {
    background: #2e7dd7;
    color: #1f1f1f;
}

/* ----- TableView (job table) ------------------------------------------ */
QTableView, QTreeView {
    background: #ffffff;
    color: #1f1f1f;
    gridline-color: #d6d6d6;
    border: 1px solid #b0b0b0;
    selection-background-color: #cce0f7;
    selection-color: #1f1f1f;
    alternate-background-color: #f5f5f5;
    outline: none;
}
QTableView::item { padding: 3px 6px; border: none; }
QTableView::item:selected { background: #cce0f7; color: #1f1f1f; }
QHeaderView { background: #e6e6e6; }
QHeaderView::section {
    background: #e0e0e0;
    color: #1f1f1f;
    padding: 4px 8px;
    border: none;
    border-right: 1px solid #c0c0c0;
    border-bottom: 1px solid #b0b0b0;
    font-weight: 500;
    text-align: left;
}
QHeaderView::section:last { border-right: none; }
QTableCornerButton::section {
    background: #e6e6e6; border: none; border-right: 1px solid #c0c0c0;
    border-bottom: 1px solid #b0b0b0;
}

/* ----- ScrollBar ------------------------------------------------------- */
QScrollBar:vertical {
    background: #e6e6e6; width: 12px; margin: 0; border-left: 1px solid #c8c8c8;
}
QScrollBar::handle:vertical {
    background: #c0c0c0;
    min-height: 28px;
    border: none;
    border-radius: 2px;
    margin: 1px;
}
QScrollBar::handle:vertical:hover { background: #909090; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal {
    background: #e6e6e6; height: 12px; margin: 0; border-top: 1px solid #c8c8c8;
}
QScrollBar::handle:horizontal {
    background: #c0c0c0;
    min-width: 28px;
    border: none;
    border-radius: 2px;
    margin: 1px;
}
QScrollBar::handle:horizontal:hover { background: #909090; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* ----- Splitter -------------------------------------------------------- */
QSplitter::handle { background: #c8c8c8; }
QSplitter::handle:horizontal { width: 1px; }
QSplitter::handle:vertical { height: 1px; }

/* ----- Frames as cards (objectName="card") ----------------------------- */
QFrame#card {
    background: #f5f5f5;
    border: 1px solid #c8c8c8;
    border-radius: 2px;
}

/* ----- Status badge labels -------------------------------------------- */
QLabel#badgeOk      { color: #2f8c2f; font-weight: 600; }
QLabel#badgeWarn    { color: #c68900; font-weight: 600; }
QLabel#badgeError   { color: #cc0000; font-weight: 600; }
QLabel#badgeNeutral { color: #555555; font-weight: 500; }

/* ----- Section headers / titles --------------------------------------- */
QLabel#sectionHeader {
    color: #1f1f1f;
    font-size: 13px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 1px;
    padding: 8px 12px 4px 12px;
    border-bottom: 1px solid #c8c8c8;
}

QLabel#inspectorSectionTitle {
    color: #444444;
    font-size: 11px;
    font-weight: 500;
    padding: 8px 0 2px 0;
}

QLabel#swatch {
    border: 1px solid #767676;
    border-radius: 2px;
    min-width: 12px; max-width: 12px;
    min-height: 12px; max-height: 12px;
}

/* ----- ProgressBar ---------------------------------------------------- */
QProgressBar {
    background: #ffffff;
    border: 1px solid #a0a0a0;
    border-radius: 2px;
    height: 14px;
    text-align: center;
    color: #1f1f1f;
}
QProgressBar::chunk { background: #2e7dd7; }
)qss");
}

void apply() {
    qApp->setStyle(QStyleFactory::create("Fusion"));

    QPalette p;
    p.setColor(QPalette::Window,        QColor("#e6e6e6"));
    p.setColor(QPalette::WindowText,    QColor("#1f1f1f"));
    p.setColor(QPalette::Base,          QColor("#ffffff"));
    p.setColor(QPalette::AlternateBase, QColor("#f5f5f5"));
    p.setColor(QPalette::ToolTipBase,   QColor("#ffffe1"));
    p.setColor(QPalette::ToolTipText,   QColor("#1f1f1f"));
    p.setColor(QPalette::Text,          QColor("#1f1f1f"));
    p.setColor(QPalette::Button,        QColor("#e6e6e6"));
    p.setColor(QPalette::ButtonText,    QColor("#1f1f1f"));
    p.setColor(QPalette::BrightText,    QColor("#cc0000"));
    p.setColor(QPalette::Highlight,     QColor("#2e7dd7"));
    p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    p.setColor(QPalette::Link,          QColor("#2e7dd7"));
    p.setColor(QPalette::Disabled, QPalette::Text,       QColor("#9a9a9a"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#9a9a9a"));
    qApp->setPalette(p);

    QFont f(".AppleSystemUIFont");
    if (!QFontInfo(f).exactMatch()) f.setFamily("Helvetica Neue");
    f.setPointSize(12);
    qApp->setFont(f);

    qApp->setStyleSheet(darkStyleSheet());
}

QString pageStyle() {
    // Mirrors design/ui-mockup.html so 記錄 / 設定 match 列印 / 建檔校準.
    return QStringLiteral(R"qss(
QComboBox, QDateEdit, QLineEdit, QSpinBox, QDoubleSpinBox {
    background:#ffffff; border:1px solid #d2d5d9; border-radius:7px;
    padding:5px 10px; font-size:12.5px; color:#23262b; min-height:18px;
}
QComboBox:hover, QDateEdit:hover, QLineEdit:hover { border-color:#b9c2cc; }
QComboBox:focus, QDateEdit:focus, QLineEdit:focus,
QSpinBox:focus, QDoubleSpinBox:focus { border-color:#2e7dd7; }
QComboBox::drop-down, QDateEdit::drop-down { border:none; width:18px; }
QComboBox::down-arrow, QDateEdit::down-arrow {
    image:none; width:0; height:0; margin-right:7px;
    border-left:4px solid transparent; border-right:4px solid transparent;
    border-top:5px solid #b0b5bd;
}
QComboBox QAbstractItemView {
    background:#ffffff; border:1px solid #d2d5d9; outline:none;
    selection-background-color:#eaf2fc; selection-color:#1f5fa8;
}

QPushButton {
    background:#ffffff; border:1px solid #d2d5d9; border-radius:8px;
    padding:7px 14px; font-size:12.5px; font-weight:600; color:#3b3f46;
}
QPushButton:hover  { background:#f3f6fa; border-color:#b9c2cc; }
QPushButton:pressed{ background:#eaf2fc; }
QPushButton#primary{ background:#2e7dd7; border:none; color:#ffffff; }
QPushButton#primary:hover { background:#2a72c4; }

QTableWidget, QTableView {
    background:#ffffff; border:1px solid #d2d5d9; border-radius:8px;
    gridline-color:#eceef1; font-size:12.5px; color:#23262b;
    alternate-background-color:#fafbfc; outline:none;
    selection-background-color:#eaf2fc; selection-color:#1f5fa8;
}
QTableView::item { padding:5px 7px; }
QHeaderView::section {
    background:#f3f4f6; color:#5b616b; border:none;
    border-bottom:1px solid #d2d5d9; padding:7px 9px;
    font-weight:600; font-size:12px;
}

QTabWidget::pane { border:none; background:transparent; top:0; }
QTabBar { background:transparent; }
QTabBar::tab {
    background:transparent; color:#868c95; padding:8px 16px; margin-right:2px;
    border:none; border-bottom:2px solid transparent;
    font-size:12.5px; font-weight:600;
}
QTabBar::tab:selected { color:#1f5fa8; border-bottom:2px solid #2e7dd7; }
QTabBar::tab:hover:!selected { color:#3b3f46; }

QScrollArea { background:transparent; border:none; }
QLabel { background:transparent; }
QRadioButton { background:transparent; color:#3b3f46; }
QGroupBox {
    border:1px solid #d2d5d9; border-radius:8px; margin-top:14px;
    background:#ffffff; font-size:12.5px; font-weight:600; color:#23262b;
}
QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 4px; }
QCheckBox { color:#3b3f46; font-size:12.5px; background:transparent; }
)qss");
}

} // namespace jcolour::theme
