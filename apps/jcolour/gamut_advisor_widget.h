#pragma once

// GamutAdvisorWidget — UI front for the engine GamutAdvisor (intent_advisor.cpp).
// Analyses an image's chroma / out-of-gamut / memory-colour content against the
// destination profile and recommends a rendering intent + BPC, with a one-click
// "套用" that drives the job inspector's intent control.
//
// Until preview pixel data is piped in, "分析" runs the real engine on a
// synthetic high-chroma demo image vs a reduced-gamut destination — the decision
// logic and analysis are the production code path, only the pixels are stand-in.

#include <QWidget>

class QLabel;
class QPushButton;

namespace jcolour {
namespace ui {

class GamutAdvisorWidget : public QWidget {
    Q_OBJECT
public:
    explicit GamutAdvisorWidget(QWidget* parent = nullptr);

signals:
    // user accepted the top recommendation; host applies it to the job.
    void applyRequested(int intent, bool bpc);

private:
    void runAnalysis();

    QLabel*      analysisBars_ = nullptr;
    QLabel*      primaryCard_  = nullptr;
    QLabel*      altCard_      = nullptr;
    QPushButton* applyBtn_     = nullptr;

    int  recIntent_ = 1;     // top recommendation (LCMS intent index)
    bool recBpc_    = true;
    bool analysed_  = false;
};

}  // namespace ui
}  // namespace jcolour
