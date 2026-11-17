#pragma once

// 色域外处理 A/B 对比（ADR-29 L3：jcolour 不替用户决定，只给两个候选并排看）。
// 左：视觉保真（保外观）；右：色度最近（最小 ΔE00）。两边都是真实 LCMS 变换：
//   源（影像内嵌 ICC，无则 sRGB；内置测试图为 ProPhoto）→ 色域外映射 → 打印机 ICC
//   → 屏幕 sRGB（软打样）。
// 没选打印机 ICC 时用内置「模拟喷墨纸色域」（演示，标明）。

#include <QDialog>
#include <QImage>
#include <QString>

#include <vector>
#include <cstdint>

class QLabel;

namespace jcolour::ui {

class OogPane;

class OogCompareDialog : public QDialog {
    Q_OBJECT
public:
    explicit OogCompareDialog(QWidget* parent = nullptr);

    void setPrinterProfile(const QString& iccPath);

signals:
    void strategyChosen(int renderIndex);   // 0 = 视觉保真，1 = 色度最近（render_intent_options.h）

private:
    void loadImage(const QString& path);
    void useTestImage();
    void rebuild();

    // 源：16 位 RGB 交错 + 源配置文件（ICC 字节；空 = sRGB）
    std::vector<uint16_t> src16_;
    int w_ = 0, h_ = 0;
    QByteArray srcIcc_;
    bool srcIsProPhoto_ = false;
    QString srcName_;
    QString printerIcc_;

    OogPane* left_ = nullptr;
    OogPane* right_ = nullptr;
    QLabel* srcInfo_ = nullptr;
    QLabel* stats_ = nullptr;
};

}  // namespace jcolour::ui
