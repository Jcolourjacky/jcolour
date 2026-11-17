#pragma once

// 每通道墨限 + 线性化向导（建档前置第 1 步，对标 GMG SmartProfiler）。
//
//   1. 打印线性化靶：Epson P 系列 11 个活动通道 × 13 阶单通道渐变（直通、无色彩管理）
//   2. 逐块实测（i1 单点）或「模拟整张」（无仪器演示，结果标「模拟」）
//   3. 引擎自动找每通道墨限（ΔE00 距纸白达峰值 98%；峰后回落 = 青铜化警告），
//      操作员可改；在墨限内按 SCTV（ISO 20654）反解出线性化曲线
//   4. 可选：实测 3 个灰块作为「再校准基准」（换纸 / 换墨批次后拉回用）
//   5. 「完成」→ result()：带墨限 + 曲线的 PrintBase；TIL 清零（须在线性化后重测）

#include "print_base.h"
#include "printer_sim.h"

#include <QDialog>
#include <QMap>
#include <QVector>

class QLabel;
class QPushButton;
class QTableWidget;
class QSpinBox;

namespace jcolour::ui {

class CurvePlot;

class LinearizationDialog : public QDialog {
    Q_OBJECT
public:
    LinearizationDialog(const QString& paperName, const printbase::PrintBase& existing,
                        QWidget* parent = nullptr);

    printbase::PrintBase result() const { return base_; }
    // 模拟后端：按纸名做确定性的差异（线性化与再校准对话框共用同一台「模拟机」）
    static printbase::SimulatedPrinter simulatorForPaper(const QString& paperName);
    bool simulated() const { return simulated_; }

private:
    void buildTable();
    void refreshRow(int row);
    void rebuildChannel(int row);          // 墨限 / 实测变化 → 重建该通道曲线
    void updateVerdict();
    void printTarget();
    void measureNext();
    void simulateAll();
    void measureGrayNext();
    void showCurve(int row);
    int  measuredCount() const;
    void storeMeasurement(int ch, int step, const printbase::Lab& lab, bool real);

    QString paperName_;
    std::vector<std::string> channels_;
    std::vector<double> steps_;
    // 每通道每阶的实测（-1 L = 未测）
    QVector<QVector<printbase::Lab>> meas_;
    QVector<QVector<bool>> measured_;
    printbase::PrintBase base_;
    printbase::SimulatedPrinter sim_;
    bool simulated_ = false;
    bool busy_ = false;

    QTableWidget* table_ = nullptr;
    QVector<QSpinBox*> limitSpins_;
    CurvePlot* plot_ = nullptr;
    QLabel* progress_ = nullptr;
    QLabel* verdict_ = nullptr;
    QLabel* grayInfo_ = nullptr;
    QPushButton* doneBtn_ = nullptr;
};

}  // namespace jcolour::ui
