#pragma once

#include <QDialog>

namespace jcolour::ui {

// Native "打印环境管理器" — the hub that ties printer + paper + linearization +
// ICC + total-ink into one named print environment. The 5-print-mode table
// computes each mode's calibration freshness through the real engine
// (calibration::compute_state), so the UI badges are driven by engine state,
// not hard-coded. First increment of the CorelDraw-style shell rewrite.
class PrintEnvironmentManager : public QDialog {
    Q_OBJECT
public:
    explicit PrintEnvironmentManager(QWidget* parent = nullptr);
};

}  // namespace jcolour::ui
