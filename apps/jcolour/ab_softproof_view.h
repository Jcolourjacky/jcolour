#pragma once

// ABSoftProofView — 左右並排軟打樣比較. Left pane shows the source as the display
// would render it; right pane shows the soft-proof simulation (paper-white +
// gamut-warning). A linked split lets the photographer judge the proof against
// the original side by side.
//
// Proofing here is a QImage-level approximation matching the existing preview
// path; the engine SoftproofEngine::render hookup (uint16 + printer/display ICC)
// lands with the real colour pipeline.

#include <QWidget>
#include <QImage>

class QCheckBox;
class QLabel;

namespace jcolour {
namespace ui {

class ImagePane;   // defined in the .cpp

class ABSoftProofView : public QWidget {
    Q_OBJECT
public:
    explicit ABSoftProofView(QWidget* parent = nullptr);

    void loadImage(const QString& path);
    void setSource(const QImage& img);

    // paper white in CIELAB; defaults to a neutral baryta-ish white.
    void setPaperWhite(double L, double a, double b);

private:
    void rebuildProof();

    QImage      source_;
    ImagePane*  leftPane_  = nullptr;
    ImagePane*  rightPane_ = nullptr;
    QCheckBox*  paperWhiteChk_ = nullptr;
    QCheckBox*  heatmapChk_    = nullptr;
    QLabel*     stats_     = nullptr;

    double pwL_ = 95.0, pwA_ = 0.0, pwB_ = 0.0;
    double chromaThreshold_ = 0.62;   // saturation above which a pixel is flagged OOG
};

}  // namespace ui
}  // namespace jcolour
