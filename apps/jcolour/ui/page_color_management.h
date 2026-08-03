#pragma once

#include <QWidget>

class QListWidget;
class QStackedWidget;

namespace jcolour {

class PageColorManagement : public QWidget {
    Q_OBJECT
public:
    explicit PageColorManagement(QWidget* parent = nullptr);

private slots:
    void onAddPrinter();
    void onRemovePrinter();

private:
    QWidget* buildPrinterDetail();

    QListWidget*    printerList_   = nullptr;
    QStackedWidget* detailStack_   = nullptr;
};

}  // namespace jcolour
