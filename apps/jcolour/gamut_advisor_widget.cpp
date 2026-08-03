#include "gamut_advisor_widget.h"
#include "intent_advisor.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <lcms2.h>
#include <vector>
#include <cmath>

namespace jcolour {
namespace ui {

namespace {
constexpr const char* kAccent = "#2e7dd7";

QString intentName(int intent) {
    switch (intent) {
        case INTENT_PERCEPTUAL:            return QStringLiteral("感知 Perceptual");
        case INTENT_RELATIVE_COLORIMETRIC: return QStringLiteral("相對比色 Relative");
        case INTENT_SATURATION:            return QStringLiteral("飽和 Saturation");
        case INTENT_ABSOLUTE_COLORIMETRIC: return QStringLiteral("絕對比色 Absolute");
        default:                           return QStringLiteral("意圖 %1").arg(intent);
    }
}

// Tiny HSV→sRGB (8-bit) for the synthetic demo image.
void hsv2rgb(double h, double s, double v, uint16_t& r, uint16_t& g, uint16_t& b) {
    double c = v * s, x = c * (1 - std::fabs(std::fmod(h / 60.0, 2) - 1)), m = v - c;
    double rr = 0, gg = 0, bb = 0;
    if      (h <  60) { rr = c; gg = x; }
    else if (h < 120) { rr = x; gg = c; }
    else if (h < 180) { gg = c; bb = x; }
    else if (h < 240) { gg = x; bb = c; }
    else if (h < 300) { rr = x; bb = c; }
    else              { rr = c; bb = x; }
    auto q = [&](double f) { return static_cast<uint16_t>((f + m) * 65535.0); };
    r = q(rr); g = q(gg); b = q(bb);
}

QString bar(const QString& label, float ratio, const QString& color) {
    int pct = static_cast<int>(ratio * 100.0f + 0.5f);
    int filled = std::min(20, std::max(0, pct * 20 / 100));
    QString blocks = QString("█").repeated(filled) + QString("░").repeated(20 - filled);
    return QString("<div style='font-size:11px;color:#3b3f46;margin:1px 0;'>"
                   "%1 <span style='color:%2;font-family:monospace;'>%3</span> %4%</div>")
        .arg(label.leftJustified(8, QChar(0x2002)), color, blocks).arg(pct);
}
}  // namespace

GamutAdvisorWidget::GamutAdvisorWidget(QWidget* parent) : QWidget(parent) {
    auto* l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(8);

    auto* analyzeBtn = new QPushButton(QStringLiteral("分析影像並建議意圖"));
    analyzeBtn->setCursor(Qt::PointingHandCursor);
    analyzeBtn->setStyleSheet(QString("QPushButton{background:%1;color:#fff;border:none;border-radius:7px;"
                                      "padding:8px 14px;font-size:12.5px;font-weight:600;}").arg(kAccent));
    connect(analyzeBtn, &QPushButton::clicked, this, &GamutAdvisorWidget::runAnalysis);
    l->addWidget(analyzeBtn);

    analysisBars_ = new QLabel;
    analysisBars_->setTextFormat(Qt::RichText);
    analysisBars_->setVisible(false);
    l->addWidget(analysisBars_);

    primaryCard_ = new QLabel;
    primaryCard_->setWordWrap(true);
    primaryCard_->setTextFormat(Qt::RichText);
    primaryCard_->setVisible(false);
    l->addWidget(primaryCard_);

    altCard_ = new QLabel;
    altCard_->setWordWrap(true);
    altCard_->setTextFormat(Qt::RichText);
    altCard_->setStyleSheet("font-size:11px;color:#868c95;");
    altCard_->setVisible(false);
    l->addWidget(altCard_);

    applyBtn_ = new QPushButton(QStringLiteral("套用最佳建議"));
    applyBtn_->setCursor(Qt::PointingHandCursor);
    applyBtn_->setStyleSheet("QPushButton{background:#fff;border:1px solid #2e7dd7;color:#2e7dd7;"
                             "border-radius:7px;padding:7px 14px;font-size:12px;font-weight:600;}");
    applyBtn_->setVisible(false);
    connect(applyBtn_, &QPushButton::clicked, this, [this] {
        if (analysed_) emit applyRequested(recIntent_, recBpc_);
    });
    l->addWidget(applyBtn_, 0, Qt::AlignLeft);
}

void GamutAdvisorWidget::runAnalysis() {
    // Synthetic high-chroma demo image: a saturated hue sweep.
    const int W = 64, H = 48;
    std::vector<uint16_t> img(static_cast<size_t>(W) * H * 3);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            double h = (static_cast<double>(x) / W) * 360.0;
            double v = 0.4 + 0.6 * (static_cast<double>(y) / H);
            uint16_t* px = &img[(static_cast<size_t>(y) * W + x) * 3];
            hsv2rgb(h, 1.0, v, px[0], px[1], px[2]);
        }
    }

    cmsHPROFILE src = cmsCreate_sRGBProfile();
    // Reduced-gamut destination so saturated pixels read as out-of-gamut.
    cmsCIExyY      wp   = {0.3127, 0.3290, 1.0};
    cmsCIExyYTRIPLE prim = {{0.58, 0.34, 1.0}, {0.30, 0.55, 1.0}, {0.16, 0.10, 1.0}};
    cmsToneCurve*  g    = cmsBuildGamma(nullptr, 2.2);
    cmsToneCurve*  curves[3] = {g, g, g};
    cmsHPROFILE    dest = cmsCreateRGBProfile(&wp, &prim, curves);
    cmsFreeToneCurve(g);

    jcolour::gamut::GamutAdvisor advisor;
    advisor.set_destination_profile(dest);
    auto recs = advisor.recommend(img.data(), W, H, 3, src);
    auto a = advisor.last_analysis();

    cmsCloseProfile(src);
    if (dest) cmsCloseProfile(dest);

    recIntent_ = recs[0].intent;
    recBpc_    = recs[0].bpc;
    analysed_  = true;

    analysisBars_->setText(
        QStringLiteral("<b style='font-size:11px;color:#23262b;'>場景分析（示範影像）</b>") +
        bar(QStringLiteral("高彩度"), a.high_chroma_ratio, "#d4476b") +
        bar(QStringLiteral("超色域"), a.out_of_gamut_ratio, "#b8860b") +
        bar(QStringLiteral("膚色"),   a.skin_ratio,   "#c8825a") +
        bar(QStringLiteral("天空"),   a.sky_ratio,    "#3a7bd5") +
        bar(QStringLiteral("植被"),   a.foliage_ratio,"#3a9d5d"));
    analysisBars_->setVisible(true);

    primaryCard_->setText(QString(
        "<div style='background:#eaf2fc;border-radius:8px;padding:8px 10px;'>"
        "<b style='color:#1f5fa8;'>建議：%1%2</b>"
        "<div style='font-size:11px;color:#3b3f46;margin-top:3px;'>信心 %3% · %4</div></div>")
        .arg(intentName(recs[0].intent),
             recs[0].bpc ? QStringLiteral(" + BPC") : QString())
        .arg(static_cast<int>(recs[0].confidence * 100))
        .arg(QString::fromStdString(recs[0].rationale)));
    primaryCard_->setVisible(true);

    altCard_->setText(QStringLiteral("替代：%1%2（信心 %3%）— %4")
        .arg(intentName(recs[1].intent), recs[1].bpc ? QStringLiteral(" + BPC") : QString())
        .arg(static_cast<int>(recs[1].confidence * 100))
        .arg(QString::fromStdString(recs[1].rationale)));
    altCard_->setVisible(true);

    applyBtn_->setVisible(true);
}

}  // namespace ui
}  // namespace jcolour
