#pragma once

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QRectF>

namespace jcolour {
namespace nesting {

struct PrintItem {
    QString id;
    QString imagePath;
    QRectF bounds; // in mm
    double rotation; // degrees
    int copies;
    QString notes;
};

struct NestingResult {
    QVector<PrintItem> placedItems;
    QVector<QRectF> unusedAreas;
    double utilization; // 0-1
    int pagesRequired;
};

enum class NestingAlgorithm {
    Simple,
    RowMajor,
    TrueShape
};

}
}

class NestingWidget : public QWidget {
    Q_OBJECT
public:
    explicit NestingWidget(QWidget* parent = nullptr);
    ~NestingWidget() override;

    void addItem(const jcolour::nesting::PrintItem& item);
    void removeItem(const QString& id);
    void clearItems();
    QVector<jcolour::nesting::PrintItem> getItems() const;

    void setPaperSize(double widthMm, double heightMm);
    QSizeF getPaperSize() const { return paperSize_; }

    void setMargins(double top, double right, double bottom, double left);
    void setGutter(double gutterMm);

    jcolour::nesting::NestingResult nest();

signals:
    void itemsChanged();
    void nestingComplete(const jcolour::nesting::NestingResult& result);

private slots:
    void onAddItem();
    void onRemoveSelected();
    void onRunNesting();
    void onAlgorithmChanged(int index);

private:
    void drawItems(QPainter* painter);
    void drawPaper(QPainter* painter);
    QRectF getPrintableArea() const;
    void paintEvent(QPaintEvent* event) override;

    QVector<jcolour::nesting::PrintItem> items_;
    QSizeF paperSize_;
    double margins_[4];
    double gutter_;
    jcolour::nesting::NestingResult lastResult_;
    jcolour::nesting::NestingAlgorithm algorithm_;
};

class SimpleNester {
public:
    static jcolour::nesting::NestingResult nest(
        const QVector<jcolour::nesting::PrintItem>& items,
        const QSizeF& sheetSize,
        const QRectF& printableArea,
        double gutter);

    static QRectF findBestPosition(
        const jcolour::nesting::PrintItem& item,
        const QVector<jcolour::nesting::PrintItem>& placed,
        const QRectF& bounds,
        double gutter);

    static bool canPlace(const jcolour::nesting::PrintItem& item,
                        const QPointF& position,
                        const QVector<jcolour::nesting::PrintItem>& placed,
                        const QRectF& bounds,
                        double gutter);
};

class TrueShapeNester {
public:
    static jcolour::nesting::NestingResult nest(
        const QVector<jcolour::nesting::PrintItem>& items,
        const QSizeF& sheetSize,
        const QRectF& printableArea,
        double gutter);

private:
    static void rotateConvexHull(QVector<QPointF>& points, double angle);
    static double calculateUtilization(const QVector<jcolour::nesting::PrintItem>& items,
                                       const QRectF& bounds);
};