#pragma once

// 打印机再校准对话框（对标 GMG ColorServer「印刷机再校准」）。
//
// 换纸批次或换墨批次后，打一张 48 块的小靶（11 通道 × 4 阶 + 纸白 + 3 灰），
// 实测 → 引擎 recal::compute_recalibration 算出每通道新线性化 → 复测验证 →
// 「应用」只写回该打印环境的线性化曲线，ICC / G7 / 墨限 / TIL / 版本号都不动，
// 并在再校准记录里留下校准前后偏差。

#include "../paper_state.h"
#include "printer_sim.h"
#include "recalibration.h"

#include <QDialog>
#include <QVector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace jcolour::ui {

class RecalibrationDialog : public QDialog {
    Q_OBJECT
public:
    explicit RecalibrationDialog(QWidget* parent = nullptr, const QString& configId = {});

signals:
    void applied(const QString& configId);

private:
    enum class Phase { Before, After };

    void loadConfig(int index);
    void resetMeasurements();
    void printChart();
    void measureNext();
    void simulateAll();
    void compute();
    void applyToEnvironment();
    void refreshTables();
    void refreshSummary();
    bool allMeasured() const;
    printbase::PrintBase currentPrintBase() const;   // 打印用：Before = 现曲线，After = 新曲线

    QVector<PaperConfig> configs_;
    PaperConfig config_;
    bool demoBase_ = false;            // 旧环境没有地基 → 模拟基准演示，不允许应用
    printbase::PrintBase base_;
    recal::ChartSpec spec_;
    std::vector<recal::ChartPatch> chart_;
    recal::Reference ref_;

    Phase phase_ = Phase::Before;
    std::vector<printbase::Lab> before_, after_;
    std::vector<bool> beforeDone_, afterDone_;
    bool simulated_ = false;
    bool busy_ = false;
    printbase::SimulatedPrinter driftedSim_;   // 「现在的机器」（模拟）
    bool haveDrift_ = false;

    recal::RecalResult result_;
    bool computed_ = false;
    recal::Evaluation afterEval_;

    QComboBox* envCombo_ = nullptr;
    QComboBox* reasonCombo_ = nullptr;
    QLineEdit* batchEdit_ = nullptr;
    QLabel* envInfo_ = nullptr;
    QLabel* progress_ = nullptr;
    QLabel* summary_ = nullptr;
    QTableWidget* chTable_ = nullptr;
    QTableWidget* patchTable_ = nullptr;
    QPushButton* computeBtn_ = nullptr;
    QPushButton* verifyBtn_ = nullptr;
    QPushButton* applyBtn_ = nullptr;
};

}  // namespace jcolour::ui
