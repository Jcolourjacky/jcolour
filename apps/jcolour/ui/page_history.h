#pragma once

#include <QWidget>

class QTableWidget;

namespace jcolour {

class PageHistory : public QWidget {
    Q_OBJECT
public:
    explicit PageHistory(QWidget* parent = nullptr);

private:
    QTableWidget* table_ = nullptr;
};

}  // namespace jcolour
