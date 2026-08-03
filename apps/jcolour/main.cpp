#include "mainwindow.h"
#include "ui/page_measurement_devices.h"
#include "theme.h"
#include "storekit_manager.h"
#include "splash_screen.h"
#include "ui/print_env_manager.h"
#include "ui/layout_window.h"
#include <QApplication>
#include <QThread>
#include <QPushButton>
#include <QEventLoop>
#include <QTimer>
#include <QPixmap>
#include <QDialog>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("jcolour");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("jcolour");
    // Reverse-DNS domain matches the MAS bundle id (com.jcolour.app) so
    // QStandardPaths derives the correct sandbox container paths.
    app.setOrganizationDomain("com.jcolour");

    jcolour::SplashScreen splash;
    splash.show();
    splash.setStatus(QStringLiteral("Loading theme…"), 10);
    app.processEvents();

    jcolour::theme::apply();
    splash.setStatus(QStringLiteral("Initializing spectral engine…"), 30);
    QThread::msleep(120);

    auto& storekit = jcolour::store::StoreKitManager::instance();
    storekit.initialize();
    splash.setStatus(QStringLiteral("Connecting StoreKit…"), 55);
    QThread::msleep(80);
#if JCOLOUR_DEBUG_UNLOCK
    // Debug: unlock everything so all features are reachable while testing.
    storekit.setSubscriptionTier(jcolour::store::SubscriptionTier::Pro);
#endif

    splash.setStatus(QStringLiteral("Probing monitor calibration…"), 75);
    QThread::msleep(80);

    splash.setStatus(QStringLiteral("Discovering Epson printers…"), 90);
    QThread::msleep(80);

    // i1 接入方式：设置里选（X-Rite SDK / Argyll / 自动），启动时套用
    jcolour::PageMeasurementDevices::applySavedBackend();

    // New CorelDraw/ErgoSoft-style shell is the default; the previous
    // sidebar shell stays reachable via JCOLOUR_CLASSIC=1 during migration.
    const bool classic = qEnvironmentVariableIntValue("JCOLOUR_CLASSIC") == 1;
    MainWindow classicWindow;
    jcolour::ui::LayoutWindow layoutWindow;
    QWidget& window = classic ? static_cast<QWidget&>(classicWindow)
                              : static_cast<QWidget&>(layoutWindow);
    splash.setStatus(QStringLiteral("Ready."), 100);
    QThread::msleep(120);

    window.show();

    // Screenshot mode (no screen-recording permission needed): JCOLOUR_SHOT=path
    // grabs the window to a PNG and quits. JCOLOUR_SHOT_PAGE picks the classic
    // sidebar page; JCOLOUR_SHOT_TAB picks the new shell's right-panel tab.
    const QByteArray shotPath = qgetenv("JCOLOUR_SHOT");
    if (!shotPath.isEmpty()) {
        splash.close();
        window.resize(1360, 880);
        const int page = qEnvironmentVariableIntValue("JCOLOUR_SHOT_PAGE");
        if (page == 99) {  // shot the native 打印环境管理器 dialog
            auto* dlg = new jcolour::ui::PrintEnvironmentManager(&window);
            dlg->setModal(false);
            dlg->show();
            QTimer::singleShot(1400, [&, dlg] {
                dlg->grab().save(QString::fromLocal8Bit(shotPath));
                app.quit();
            });
            return app.exec();
        }
        if (classic && page != 0) classicWindow.selectPageForShot(page);
        if (!classic) {
            const QString act = qEnvironmentVariable("JCOLOUR_SHOT_ACTION");
            if (!act.isEmpty()) layoutWindow.runActionForShot(act);
            layoutWindow.selectTabForShot(qEnvironmentVariableIntValue("JCOLOUR_SHOT_TAB"));
        }
        QTimer::singleShot(1400, [&] {
            // an action may have opened a dialog — prefer grabbing it
            QWidget* target = &window;
            for (QWidget* w : QApplication::topLevelWidgets())
                if (w->isVisible() && qobject_cast<QDialog*>(w)) { target = w; break; }
            target->grab().save(QString::fromLocal8Bit(shotPath));
            app.quit();
        });
        return app.exec();
    }

    // Show continue button
    splash.showContinueButton();

    // Wait for user click
    QEventLoop loop;
    QObject::connect(splash.continueButton(), &QPushButton::clicked, &loop, &QEventLoop::quit);
    loop.exec();

    splash.close();

    return app.exec();
}
