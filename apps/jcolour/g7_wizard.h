#pragma once

// G7Wizard — the G7+ 灰階向導. A four-step flow that drives the engine's
// g7_validation module: 列印 G7 靶 → 量測 → 結果分析 → 匯出報告.
//
// The measurement step synthesises a demo G7CalibrationResult until the i1Pro3
// backend is wired (Spike-3); everything downstream (compliance grading via
// G7ComplianceChecker, the per-patch table, and report export via
// G7ValidationReport) runs the real engine code.

#include <QWidget>

namespace jcolour { namespace calibration { class G7ValidationReport; } }

class QStackedWidget;
class QLabel;
class QPushButton;
class QTableWidget;

namespace jcolour {
namespace ui {

class G7Wizard : public QWidget {
    Q_OBJECT
public:
    explicit G7Wizard(QWidget* parent = nullptr);

signals:
    // emitted once results are computed; pass = G7 conformance achieved.
    void validated(bool pass, double avgDeltaE);

private:
    void buildPrintPage();
    void buildMeasurePage();
    void buildResultPage();
    void buildReportPage();
    void runMeasurement();     // synth result + engine grading → fills the table
    void exportReport();
    bool exportPdf(jcolour::calibration::G7ValidationReport& report, const QString& path);
    void goTo(int page);

    QStackedWidget* stack_ = nullptr;
    QPushButton*    backBtn_ = nullptr;
    QPushButton*    nextBtn_ = nullptr;

    QLabel*         resultBadge_  = nullptr;
    QLabel*         resultMetrics_ = nullptr;
    QTableWidget*   patchTable_   = nullptr;
    QLabel*         reportStatus_ = nullptr;

    bool   measured_ = false;
    bool   passed_   = false;
    double avgDeltaE_ = 0.0;

    struct Held;          // pimpl for the engine result (keeps engine types out of the header)
    Held* held_ = nullptr;
};

}  // namespace ui
}  // namespace jcolour
