#pragma once

// 可视化专色优化（CGS ORIS Color Tuner「Visual Spot Color Optimization」
// 同款手调方法）：
//   中心 = 当前设备配方（CMYK）。六个方向各绑定一个通道增/减（默认
//   1=C+ 2=M+ 3=Y+ 4=C− 5=M− 6=Y−）与步长；蜂巢按外圈数展开，环 r 上
//   两相邻方向的组合块 = 中心 + a·Δk + b·Δk+1。
//   屏幕预览是近似色；正式评估靠「打印」蜂巢图后目测选块，行列标号
//   （左侧字母行 · 顶部数字列）用于纸上定位。点选色块 → 左侧读数更新；
//   「设为中心」以该块为新中心继续细调；「保存」入专色库；「确定」返回
//   最终配方。

#include <QDialog>
#include <QVector>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QLabel;

namespace jcolour::ui {

struct SpotRecipe {
    double c = 0, m = 0, y = 0, k = 0;   // 0..100
    bool clipped() const {
        auto bad = [](double v) { return v < -0.001 || v > 100.001; };
        return bad(c) || bad(m) || bad(y) || bad(k);
    }
};

class SpotHexChart;   // 蜂巢绘制/命中控件（.cpp 内实现）

class VisualSpotTunerDialog : public QDialog {
    Q_OBJECT
public:
    explicit VisualSpotTunerDialog(QWidget* parent = nullptr);

    void setSpotName(const QString& name);
    void setCenterRecipe(const SpotRecipe& r);
    SpotRecipe selectedRecipe() const;

signals:
    /// 确定：最终采用的专色配方。
    void recipeAccepted(const QString& name, const SpotRecipe& recipe);

private:
    void rebuildChart();               // 参数变化 → 重生成蜂巢
    void onPatchSelected(int index);   // 点选色块 → 更新读数
    void recenterOnSelection();        // 设为中心（迭代细调）
    void saveToLibrary();
    void printChart();

    QLineEdit*  nameEdit_ = nullptr;
    QLabel*     labL_ = nullptr; QLabel* labA_ = nullptr; QLabel* labB_ = nullptr;
    QDoubleSpinBox* devC_ = nullptr; QDoubleSpinBox* devM_ = nullptr;
    QDoubleSpinBox* devY_ = nullptr; QDoubleSpinBox* devK_ = nullptr;
    QVector<QComboBox*> dirChannel_;   // 6 个方向的通道选择
    QVector<QComboBox*> dirStep_;      // 6 个方向的步长
    QComboBox* shells_ = nullptr;
    QComboBox* pageSize_ = nullptr;
    QCheckBox* sameSteps_ = nullptr;
    QCheckBox* showClipped_ = nullptr;
    QCheckBox* outlines_ = nullptr;
    SpotHexChart* chart_ = nullptr;
};

}  // namespace jcolour::ui
