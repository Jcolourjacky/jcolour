#pragma once

#include <QWidget>
#include <QString>

class QLabel;
class QProgressBar;
class QPushButton;

namespace jcolour {

// Frameless splash window: shows the Jcolour wordmark, version, a feature
// matrix grouped by module, and a progress bar that updates as the app
// initializes.
class SplashScreen : public QWidget {
    Q_OBJECT
public:
    explicit SplashScreen(QWidget* parent = nullptr);

    // Update progress + status line. percent is clamped 0..100.
    void setStatus(const QString& message, int percent);

    // Show the continue button
    void showContinueButton();

    // Get the continue button
    QPushButton* continueButton();

    // Convenience: run a sequence of init steps in the GUI thread, calling
    // processEvents between them so the splash repaints. Closes when done.
    void runInitSequence();

private:
    QLabel*       statusLabel_   = nullptr;
    QProgressBar* progress_      = nullptr;
    QPushButton*  continueBtn_  = nullptr;
};

} // namespace jcolour
