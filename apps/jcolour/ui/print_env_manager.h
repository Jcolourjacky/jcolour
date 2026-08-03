#pragma once

#include <QDialog>

class QVBoxLayout;

namespace jcolour::ui {

// Native "打印环境管理器" — the hub that ties printer + paper + linearization +
// ICC + total-ink into one named print environment. The environment list is
// the real PaperState config library; "新建 / 校准向导" embeds the full
// CalibrationStepper (printer → paper → feed → device → auto-cal with
// strip measurement → save) and the list refreshes on completion. The
// 5-print-mode table computes each mode's calibration freshness through the
// real engine (calibration::compute_state).
class PrintEnvironmentManager : public QDialog {
    Q_OBJECT
public:
    explicit PrintEnvironmentManager(QWidget* parent = nullptr);

private:
    void startNewCalibration();   // modal CalibrationStepper wizard
    void rebuildEnvList();        // re-read PaperState configs

    QVBoxLayout* listLayout_ = nullptr;   // left column items live here
};

}  // namespace jcolour::ui
