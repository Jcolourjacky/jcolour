#pragma once

#include <QWidget>

class QTreeWidget;

namespace jcolour {

class PageInkManagement : public QWidget {
    Q_OBJECT
public:
    explicit PageInkManagement(QWidget* parent = nullptr);

private:
    QTreeWidget* tree_ = nullptr;
};

}  // namespace jcolour
