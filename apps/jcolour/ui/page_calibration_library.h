#pragma once

// CalibrationLibraryPage — Path B landing page (建檔校準 配置庫).
// Renders the built paper standards from PaperState (engine ParamDb), matching
// design/ui-mockup.html: per-config card with enable toggle, 5 full-name mode
// chips, status badge, 自動校準 / 重新校正 buttons, plus a "新建校準" row.

#include <QWidget>

class QVBoxLayout;

namespace jcolour {

class CalibrationLibraryPage : public QWidget {
    Q_OBJECT
public:
    explicit CalibrationLibraryPage(QWidget* parent = nullptr);

    void reload();   // re-read PaperState and rebuild the card list

signals:
    void newCalibrationRequested();              // ＋ 新建校準
    void recalibrateRequested(const QString& configId);  // 重新校正 (next version)

private:
    QWidget* buildCard(const struct PaperConfig& c);
    QWidget* buildNewRow();

    QVBoxLayout* listLayout_ = nullptr;
};

}  // namespace jcolour
