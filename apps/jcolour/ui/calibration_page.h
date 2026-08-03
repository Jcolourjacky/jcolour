#pragma once

// CalibrationPage — Path B container. Stacks the config library (landing) and
// the new-calibration stepper, toggling between them like the design mockup's
// page-cal / page-calnew.

#include <QWidget>

class QStackedWidget;

namespace jcolour {

class CalibrationLibraryPage;
class CalibrationStepper;

class CalibrationPage : public QWidget {
    Q_OBJECT
public:
    explicit CalibrationPage(QWidget* parent = nullptr);

private:
    QStackedWidget*          stack_   = nullptr;
    CalibrationLibraryPage*  library_ = nullptr;
    CalibrationStepper*      stepper_ = nullptr;
};

}  // namespace jcolour
