#include "calibration_page.h"

#include <QStackedWidget>
#include <QVBoxLayout>

#include "page_calibration_library.h"
#include "calibration_stepper.h"
#include "../paper_state.h"

namespace jcolour {

CalibrationPage::CalibrationPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stack_   = new QStackedWidget;
    library_ = new CalibrationLibraryPage;
    stepper_ = new CalibrationStepper;
    stack_->addWidget(library_);   // index 0: 配置庫 (landing)
    stack_->addWidget(stepper_);   // index 1: 新建/重新校準 stepper
    root->addWidget(stack_);

    // 新建校準 → fresh stepper
    connect(library_, &CalibrationLibraryPage::newCalibrationRequested, this, [this] {
        stepper_->resetForNew();
        stack_->setCurrentWidget(stepper_);
    });

    // 重新校正 → prefill stepper from the chosen config (saved as next version)
    connect(library_, &CalibrationLibraryPage::recalibrateRequested, this,
            [this](const QString& configId) {
        for (const auto& c : PaperState::instance().listConfigs()) {
            if (c.id == configId) { stepper_->startFrom(c); break; }
        }
        stack_->setCurrentWidget(stepper_);
    });

    // saved → reload library and return to it
    connect(stepper_, &CalibrationStepper::finished, this, [this] {
        library_->reload();
        stack_->setCurrentWidget(library_);
    });

    // cancelled → just return to the library
    connect(stepper_, &CalibrationStepper::cancelled, this, [this] {
        stack_->setCurrentWidget(library_);
    });
}

}  // namespace jcolour
