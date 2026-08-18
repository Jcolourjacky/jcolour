#pragma once

#include <QWidget>
#include <QString>
#include <QVector>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QStackedWidget;

namespace jcolour {

struct PrinterEntry {
    QString name;
    QString model;          // "Epson SureColor P9500" etc.
    QString serial;         // 序列號 — distinguishes units of the same model
    // 端口类型（ErgoSoft 四种）："tcp"(RAW 9100) | "lpr" | "usb" | "file"
    QString connection;
    QString host;           // TCP / LPR
    int     port = 9100;    // RAW 默认 9100；LPR 默认 515
    QString lprQueue;       // LPR 队列名（如 "PASSTHRU"）
    QString outputDir;      // 打印到文件：输出目录
    QString defaultInkMode; // "PhotoBlack" | "MatteBlack" | "MultiChannel"
    int     defaultDpi = 1440;
    bool    isDefault = false;
};

// Tabbed printer manager: list on the left, settings form on the right.
// Persists to QSettings under group "printers/<i>".
// Signals connection test results & default-printer changes.
class PrinterManagerWidget : public QWidget {
    Q_OBJECT
public:
    explicit PrinterManagerWidget(QWidget* parent = nullptr);

    QVector<PrinterEntry> printers() const { return printers_; }
    QString defaultPrinterName() const;

signals:
    void defaultPrinterChanged(const QString& name);
    void printersChanged();

private:
    void loadFromSettings();
    void saveToSettings();
    void rebuildList();
    void selectIndex(int i);
    void onListSelectionChanged();
    void readFormIntoCurrent();   // form -> printers_[currentIdx_]
    void writeCurrentIntoForm();  // printers_[currentIdx_] -> form
    void onAdd();
    void onRemove();
    void onDiscover();
    void onTestConnection();
    void onQueryStatus();         // 双向状态：墨量/介质/错误（PJL 查询）
    void onSetDefault();
    void updateConnRows();        // 按端口类型 显示/隐藏 host/port/队列/目录行

    QVector<PrinterEntry> printers_;
    int currentIdx_ = -1;

    QListWidget*   list_       = nullptr;
    QPushButton*   addBtn_     = nullptr;
    QPushButton*   removeBtn_  = nullptr;
    QPushButton*   discoverBtn_ = nullptr;

    QLineEdit*     nameEdit_   = nullptr;
    QComboBox*     modelCombo_ = nullptr;
    QLineEdit*     serialEdit_ = nullptr;
    QComboBox*     connCombo_  = nullptr;
    QLineEdit*     hostEdit_   = nullptr;
    QSpinBox*      portSpin_   = nullptr;
    QLineEdit*     queueEdit_  = nullptr;   // LPR 队列名
    QLineEdit*     dirEdit_    = nullptr;   // 打印到文件目录
    QComboBox*     inkCombo_   = nullptr;
    QComboBox*     dpiCombo_   = nullptr;
    QCheckBox*     defaultChk_ = nullptr;
    QPushButton*   testBtn_    = nullptr;
    QPushButton*   statusBtn_  = nullptr;   // 查询状态（双向）
    QLabel*        statusLbl_  = nullptr;
    class InkLevelsBar* inkBar_ = nullptr;  // 12 通道墨量条（自绘）

    bool blockFormSignals_ = false;
};

} // namespace jcolour
