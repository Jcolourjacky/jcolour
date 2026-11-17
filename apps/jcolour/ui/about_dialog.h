#pragma once

// 关于 jcolour：版本 · 软件物料清单（SBOM，CycloneDX 1.5 可导出）· 第三方声明。

#include <QDialog>

namespace jcolour::ui {

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);
};

}  // namespace jcolour::ui
