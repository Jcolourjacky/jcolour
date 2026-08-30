#pragma once

// 总墨量上限 TIL（TAC）测定向导 —— 对齐 GMG ColorProof / EFI Fiery XF
// 的两步墨限工作流：
//
//   第 1 步 单通道墨限（Fiery XF「Ink Limit per Channel」/ GMG 校准首步）
//     C/M/Y/K 各打渐变靶，取「达到最大色度/密度」的点为该通道墨限
//     （Fiery 文档：很多介质 ~50% 即达最大密度）。可实测或手输。
//
//   第 2 步 总墨 TIL（Fiery XF「Reduce the total ink limit」）
//     初始 TIL = 各通道墨限之和（Fiery 语义）；围绕它打多列递增靶
//     （列号 1..N），干燥后测量 + 目检。两种判定并行：
//       · 自动 TIL —— 测量拐点：相邻列 L* 增益 < 阈值（Dmax 已到）
//       · 视觉 TIL —— 操作员点选「最佳列号」（仪器测不出起皱/渗墨，
//         Fiery 明确视觉可否决自动值）
//     建议 = 视觉优先，否则自动；对照介质参考区间提示
//     （涂布 300–320 / 未涂布 220–240 / 新闻纸 200–220 …）。
//
//   应用：写入该纸的打印环境（QSettings til/<介质>）。

#include <QDialog>
#include <QVector>

class QComboBox;
class QSpinBox;
class QTableWidget;
class QLabel;
class QPushButton;

namespace jcolour::ui {

class TILWizardDialog : public QDialog {
    Q_OBJECT
public:
    explicit TILWizardDialog(const QString& paperName, QWidget* parent = nullptr);

private:
    struct Row {
        int    tac = 0;
        double c = 0, m = 0, y = 0, k = 0;   // 该档的组合配方
        double L = -1;                        // 实测 L*（-1 = 未测）
        bool   bleed = false;                 // 渗墨/晕染勾选
    };

    void rebuildRows();          // 范围/步长/通道限变化 → 重建测试靶
    void syncRangeToChannels();  // Σ通道限 → TIL 靶默认范围（Fiery 初始 TIL）
    void printTarget();          // 打印测试靶（列号 + TAC 标号 + 页眉）
    void measureNext();          // i1 实测下一未测列（无仪器→模拟）
    void analyze();              // 自动拐点 + 渗墨 + 视觉列 → 建议 TIL
    void applyToEnvironment();   // 写入 QSettings til/<paper>

    QString paperName_;
    QVector<Row> rows_;
    QComboBox* mediaType_ = nullptr;
    QSpinBox*  chLimit_[4] = {};        // 第 1 步：C/M/Y/K 单通道墨限 %
    QLabel*    chSum_ = nullptr;        // Σ通道限（= Fiery 初始 TIL）
    QSpinBox*  fromSpin_ = nullptr;
    QSpinBox*  toSpin_ = nullptr;
    QSpinBox*  stepSpin_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* verdict_ = nullptr;
    QPushButton* applyBtn_ = nullptr;
    int suggested_ = 0;          // 0 = 尚无建议
    int visualCol_ = -1;         // 视觉 TIL：操作员点选的最佳列（-1 未选）
};

}  // namespace jcolour::ui
