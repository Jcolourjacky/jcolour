#pragma once

// PrintGateBar — the graded pre-print status gate (列印前狀態確認).
// Bottom bar of the print page. Reads a GateResult from PaperState::evaluateGate
// and renders: 4 lamps (噴頭 / 走紙 / 色彩校準 / 品質檢查), a graded banner, and
// the print buttons — with 列印 disabled on a HardBlock per the project decision.

#include <QWidget>
#include "../paper_state.h"

class QLabel;
class QPushButton;

namespace jcolour {

class PrintGateBar : public QWidget {
    Q_OBJECT
public:
    explicit PrintGateBar(QWidget* parent = nullptr);

    void setGate(const GateResult& g);

signals:
    void printRequested();
    void testPrintRequested();
    void recalibrateRequested();

private:
    QWidget* lamp(QLabel*& dot, QLabel*& text, const QString& name);

    QLabel* nozzleDot_ = nullptr;  QLabel* nozzleTxt_ = nullptr;
    QLabel* feedDot_   = nullptr;  QLabel* feedTxt_   = nullptr;
    QLabel* calDot_    = nullptr;  QLabel* calTxt_    = nullptr;
    QLabel* qcDot_     = nullptr;  QLabel* qcTxt_     = nullptr;
    QLabel* banner_    = nullptr;
    QPushButton* recalBtn_ = nullptr;
    QPushButton* testBtn_  = nullptr;
    QPushButton* printBtn_ = nullptr;
};

}  // namespace jcolour
