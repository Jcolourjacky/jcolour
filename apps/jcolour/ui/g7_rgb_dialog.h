#pragma once

// G7(+) for RGB Printer 校正向导（Print By Number 方法论，引擎见
// engine/src/g7_rgb）：
//   参数（纸白/黑点 Lab、NPDC Gamma 0.9、36/23 格导具）→ 打印导具 →
//   测量（i1 逐块 / 一键模拟）→ Gray Finder 生成 R/G/B 校正曲线 →
//   Verifier 25 点验证（校正前 vs 校正后）→ 应用到打印环境。

#include <QDialog>
#include <QVector>
#include "../../../engine/src/g7_rgb.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace jcolour::ui {

class G7RgbCurveView;   // 三通道曲线图（cpp 内实现）

class G7RgbDialog : public QDialog {
    Q_OBJECT
public:
    explicit G7RgbDialog(const QString& paperName, QWidget* parent = nullptr);

private:
    void rebuildModel();     // 参数变化 → 重解 NPDC + 重建导具
    void printChart();
    void measureNext();      // i1 实测下一块（无仪器→该块模拟）
    void simulateAll();      // 一键模拟全部测量（演示闭环）
    void runPredict();       // Gray Finder → 曲线 + Verifier 前后对比
    void applyToEnvironment();

    // 演示用模拟打印机（带系统性灰漂与版调偏差）
    g7rgb::Lab simulatedPrint(int r, int g, int b) const;

    QString paperName_;
    g7rgb::NpdcModel model_;
    std::vector<g7rgb::Patch> chart_;
    QVector<g7rgb::Lab> measured_;   // 与 chart_ 对齐；L<0 = 未测
    g7rgb::Curves curves_;
    bool hasCurves_ = false;

    QDoubleSpinBox* paperL_ = nullptr; QDoubleSpinBox* paperA_ = nullptr;
    QDoubleSpinBox* paperB_ = nullptr; QDoubleSpinBox* blackL_ = nullptr;
    QDoubleSpinBox* gammaSpin_ = nullptr;
    QComboBox* chartKind_ = nullptr;
    QLabel* modelInfo_ = nullptr;
    QLabel* progress_ = nullptr;
    G7RgbCurveView* curveView_ = nullptr;
    QTableWidget* verifyTable_ = nullptr;
    QLabel* verdict_ = nullptr;
    QPushButton* applyBtn_ = nullptr;
};

}  // namespace jcolour::ui
