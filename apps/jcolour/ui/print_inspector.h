#pragma once

// PrintInspector — the right-hand inspector for the Print page, matching
// design/ui-mockup.html: an ACCORDION of sections (only one open at a time) —
// 紙張與色彩 / 黑白色調 / 紙張 / 影像尺寸 / 邊界 / 旋轉·裁切 / 軟打樣 /
// 拼版·版次·工具. The pre-print status gate is no longer hosted here; it lives
// as a pinned full-width bar at the bottom of the Print page (MainWindow).

#include <QWidget>
#include <QVector>

class SoftProofingWidget;
class BWModeWidget;
class QPushButton;

namespace jcolour {

namespace ui {
class GamutAdvisorWidget;

class PrintInspector : public QWidget {
    Q_OBJECT
public:
    explicit PrintInspector(QWidget* parent = nullptr);

    SoftProofingWidget* softProof() const { return softProof_; }

signals:
    void jobModified();

private:
    struct Section { QPushButton* header; QWidget* body; QString title; };

    // Adds an accordion section; `open` makes it the initially-expanded one.
    QWidget* makeSection(const QString& title, QWidget* content, bool open);
    void setOpen(int idx);  // accordion: open idx, collapse the rest

    QVector<Section>    sections_;
    SoftProofingWidget* softProof_ = nullptr;
    BWModeWidget*       bw_        = nullptr;
    GamutAdvisorWidget* gamut_     = nullptr;
};

}  // namespace ui
}  // namespace jcolour
