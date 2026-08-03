#pragma once

// CalibrationStepper — the 新建校準 flow (design mockup page-calnew, v2).
// New flow (6 steps): 選打印機(型號+序列號) · 建立紙張 · 走紙1米(米尺) ·
//   選測量設備 · 校準集(5 種列印方式自動跑) · 存為標準.
// On finish, writes a PaperConfig into PaperState (engine ParamDb).
// Linearization method per ADR-30: 中性(WF1/2/3) 走 SCTV; 密度只管墨量上限;
//   彩色靠 G7+ICC.

#include <QWidget>
#include "../paper_state.h"

class QLabel;
class QStackedWidget;
class QListWidget;
class QLineEdit;
class QComboBox;
class QPushButton;

namespace jcolour {

class CalibrationStepper : public QWidget {
    Q_OBJECT
public:
    explicit CalibrationStepper(QWidget* parent = nullptr);

    void resetForNew();                       // start a fresh config
    void startFrom(const PaperConfig& base);  // 重新校正: prefill, save as next version

signals:
    void finished();                          // saved → caller reloads library
    void cancelled();                         // back to library

private:
    void buildSteps();
    void showStep(int i);
    void doStepAction();                      // run the current step's action
    void updateStepList();
    void reloadPrinters();                    // load printers from QSettings

    QListWidget*    stepList_ = nullptr;
    QStackedWidget* panels_   = nullptr;
    QLabel*         title_    = nullptr;
    QLabel*         lead_     = nullptr;
    QLabel*         result_   = nullptr;
    QPushButton*    actionBtn_= nullptr;
    QPushButton*    nextBtn_  = nullptr;

    // step 0 — printer
    QComboBox*  printerCombo_  = nullptr;
    QPushButton* addPrinterBtn_= nullptr;
    // step 1 — paper
    QLineEdit*  paperNameEdit_ = nullptr;
    QLineEdit*  paperMetaEdit_ = nullptr;
    QLineEdit*  batchEdit_     = nullptr;
    // step 3 — measurement device
    QComboBox*  deviceCombo_   = nullptr;

    PaperConfig draft_;
    int   step_ = 0;
    bool  isRecal_ = false;
    static constexpr int kSteps = 6;
    bool  stepDone_[kSteps] = {false};
};

}  // namespace jcolour
