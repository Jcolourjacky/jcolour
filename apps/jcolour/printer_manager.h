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
    QString connection;     // "tcp" | "usb"
    QString host;           // e.g. "192.168.1.100" — TCP only
    int     port = 9100;
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
    void onSetDefault();

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
    QComboBox*     inkCombo_   = nullptr;
    QComboBox*     dpiCombo_   = nullptr;
    QCheckBox*     defaultChk_ = nullptr;
    QPushButton*   testBtn_    = nullptr;
    QLabel*        statusLbl_  = nullptr;

    bool blockFormSignals_ = false;
};

} // namespace jcolour
