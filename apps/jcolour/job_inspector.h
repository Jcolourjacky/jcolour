#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include "job.h"
#include "curve_editor.h"

class BWModeWidget;
class SoftProofingWidget;

namespace jcolour {

class Job;

class JobInspector : public QWidget {
    Q_OBJECT
public:
    explicit JobInspector(QWidget* parent = nullptr);

    void clear();
    void setJob(const Job& job);
    Job  buildJob() const;

signals:
    void jobModified();

private:
    QWidget* buildInputTab();
    QWidget* buildColorTab();
    QWidget* buildPrintTab();
    QWidget* buildFinishingTab();
    QWidget* buildSystemTab();

    QTabWidget* topTabs_;

    // Input
    QLabel*    imagePathLabel_;
    QLabel*    imageInfoLabel_;
    QComboBox* sourceProfileCombo_;

    // Color (sub-tabs)
    QTabWidget*   colorSubTabs_;
    BWModeWidget* bwWidget_;
    InkCurveEditor*  curveEditor_;
    QComboBox*    iccProfileCombo_;
    QComboBox*    intentCombo_;
    QCheckBox*    bpcCheck_;
    QSlider* slC_; QSlider* slM_; QSlider* slY_; QSlider* slK_;
    QSlider* slR_; QSlider* slG_; QSlider* slB_;
    QLabel*  lblC_; QLabel* lblM_; QLabel* lblY_; QLabel* lblK_;
    QLabel*  lblR_; QLabel* lblG_; QLabel* lblB_;
    QSlider* slBrightness_;
    QSlider* slContrast_;
    QSlider* slSaturation_;
    QSlider* slSharpen_;
    QLabel*  lblBrightness_;
    QLabel*  lblContrast_;
    QLabel*  lblSaturation_;
    QLabel*  lblSharpen_;
    SoftProofingWidget* softProof_;

    // Print
    QComboBox*       paperCombo_;
    QDoubleSpinBox*  paperW_;
    QDoubleSpinBox*  paperH_;
    QSpinBox*        marginT_;
    QSpinBox*        marginR_;
    QSpinBox*        marginB_;
    QSpinBox*        marginL_;
    QSpinBox*        objX_;
    QSpinBox*        objY_;
    QSpinBox*        objR_;
    QComboBox*       printerCombo_;
    QSpinBox*        copiesSpin_;

    bool blockSignals_ = false;
};

} // namespace jcolour
