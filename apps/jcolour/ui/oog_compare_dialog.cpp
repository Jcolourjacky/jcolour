#include "oog_compare_dialog.h"

#include "delta_e.h"
#include "oog_mapping.h"

#include <QApplication>
#include <QColorSpace>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include <lcms2.h>

#include <algorithm>
#include <cmath>

namespace jcolour::ui {

using jcolour::gamut::OogStrategy;
using jcolour::spectral::Lab;

class OogPane : public QWidget {
public:
    explicit OogPane(const QString& caption, QWidget* parent = nullptr)
        : QWidget(parent), caption_(caption) { setMinimumSize(300, 240); }
    void setImage(const QImage& img) { img_ = img; update(); }
    void setFooter(const QString& f) { footer_ = f; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor("#2b2d31"));
        const QRect area = rect().adjusted(6, 24, -6, -40);
        p.setPen(QColor("#e6e8eb"));
        QFont f = p.font(); f.setPointSizeF(10); f.setBold(true); p.setFont(f);
        p.drawText(QRect(8, 4, width() - 16, 18), Qt::AlignLeft | Qt::AlignVCenter, caption_);
        if (!img_.isNull()) {
            const QImage s = img_.scaled(area.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            p.drawImage(QPoint(area.x() + (area.width() - s.width()) / 2,
                               area.y() + (area.height() - s.height()) / 2), s);
        }
        f.setBold(false); f.setPointSizeF(9); p.setFont(f);
        p.setPen(QColor("#c9ccd1"));
        p.drawText(QRect(8, height() - 38, width() - 16, 36), Qt::AlignLeft | Qt::TextWordWrap, footer_);
    }

private:
    QString caption_, footer_;
    QImage img_;
};

namespace {

// 内置「模拟喷墨纸色域」：比 sRGB 窄的红/蓝、偏青绿的矩阵配置文件（演示用）
cmsHPROFILE createSimPrinterProfile() {
    cmsCIExyYTRIPLE prim = {{0.62, 0.33, 1.0}, {0.26, 0.60, 1.0}, {0.16, 0.09, 1.0}};
    cmsToneCurve* g = cmsBuildGamma(nullptr, 2.2);
    cmsToneCurve* c[3] = {g, g, g};
    cmsHPROFILE p = cmsCreateRGBProfile(cmsD50_xyY(), &prim, c);
    cmsFreeToneCurve(g);
    return p;
}

void hsvToRgb(double h, double s, double v, double& r, double& g, double& b) {
    const double c = v * s, x = c * (1 - std::fabs(std::fmod(h / 60.0, 2) - 1)), m = v - c;
    double rr = 0, gg = 0, bb = 0;
    if (h < 60) { rr = c; gg = x; } else if (h < 120) { rr = x; gg = c; }
    else if (h < 180) { gg = c; bb = x; } else if (h < 240) { gg = x; bb = c; }
    else if (h < 300) { rr = x; bb = c; } else { rr = c; bb = x; }
    r = rr + m; g = gg + m; b = bb + m;
}

}  // namespace

OogCompareDialog::OogCompareDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("色域外处理 A/B 对比"));
    resize(1080, 620);
    auto* v = new QVBoxLayout(this);

    auto* bar = new QHBoxLayout;
    auto* loadBtn = new QPushButton(QStringLiteral("载入影像…"));
    auto* testBtn = new QPushButton(QStringLiteral("内置测试图（ProPhoto 饱和渐变）"));
    auto* iccBtn = new QPushButton(QStringLiteral("打印机 ICC…"));
    srcInfo_ = new QLabel;
    srcInfo_->setStyleSheet("color:#7A776D;font-size:11px;");
    for (auto* b : {loadBtn, testBtn, iccBtn}) bar->addWidget(b);
    bar->addWidget(srcInfo_, 1);
    v->addLayout(bar);

    auto* split = new QSplitter(Qt::Horizontal);
    left_ = new OogPane(QStringLiteral("A · 视觉保真（保外观）· 摄影默认"));
    right_ = new OogPane(QStringLiteral("B · 色度最近（最小 ΔE00）"));
    split->addWidget(left_);
    split->addWidget(right_);
    v->addWidget(split, 1);

    stats_ = new QLabel;
    stats_->setWordWrap(true);
    stats_->setStyleSheet("background:#E7F7FD;border:1px solid #A9E4F7;border-radius:6px;padding:8px 12px;color:#100A36;");
    v->addWidget(stats_);

    auto* btns = new QHBoxLayout;
    btns->addStretch();
    auto* useA = new QPushButton(QStringLiteral("采用 A · 视觉保真"));
    useA->setObjectName("primary");
    auto* useB = new QPushButton(QStringLiteral("采用 B · 色度最近"));
    auto* close = new QPushButton(QStringLiteral("关闭"));
    btns->addWidget(useA);
    btns->addWidget(useB);
    btns->addWidget(close);
    v->addLayout(btns);

    connect(loadBtn, &QPushButton::clicked, this, [this] {
        const QString p = QFileDialog::getOpenFileName(this, QStringLiteral("载入影像"), QString(),
                                                       QStringLiteral("影像 (*.tif *.tiff *.png *.jpg *.jpeg)"));
        if (!p.isEmpty()) loadImage(p);
    });
    connect(testBtn, &QPushButton::clicked, this, &OogCompareDialog::useTestImage);
    connect(iccBtn, &QPushButton::clicked, this, [this] {
        const QString p = QFileDialog::getOpenFileName(this, QStringLiteral("打印机 ICC"), QString(),
                                                       QStringLiteral("ICC (*.icc *.icm)"));
        if (!p.isEmpty()) setPrinterProfile(p);
    });
    connect(useA, &QPushButton::clicked, this, [this] { emit strategyChosen(0); accept(); });
    connect(useB, &QPushButton::clicked, this, [this] { emit strategyChosen(1); accept(); });
    connect(close, &QPushButton::clicked, this, &QDialog::reject);

    useTestImage();
}

void OogCompareDialog::setPrinterProfile(const QString& iccPath) {
    printerIcc_ = iccPath;
    rebuild();
}

void OogCompareDialog::useTestImage() {
    // ProPhoto 16 位：上半 6 条色相带（饱和度递增），下半三原色 / 二次色明度渐变
    w_ = 720; h_ = 360;
    src16_.assign(size_t(w_) * h_ * 3, 0);
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) {
            double r, g, b;
            if (y < h_ / 2) {
                const int band = y * 6 / (h_ / 2);
                hsvToRgb(360.0 * x / w_, 0.45 + 0.11 * band, 0.95, r, g, b);
            } else {
                const int band = (y - h_ / 2) * 6 / (h_ / 2);
                hsvToRgb(band * 60.0, 1.0, 0.15 + 0.85 * double(x) / w_, r, g, b);
            }
            uint16_t* px = &src16_[(size_t(y) * w_ + x) * 3];
            px[0] = uint16_t(std::lround(r * 65535));
            px[1] = uint16_t(std::lround(g * 65535));
            px[2] = uint16_t(std::lround(b * 65535));
        }
    srcIcc_.clear();
    srcIsProPhoto_ = true;
    srcName_ = QStringLiteral("内置测试图 · ProPhoto RGB 16 位");
    rebuild();
}

void OogCompareDialog::loadImage(const QString& path) {
    QImage img(path);
    if (img.isNull()) { stats_->setText(QStringLiteral("无法载入：%1").arg(path)); return; }
    srcIcc_ = img.colorSpace().isValid() ? img.colorSpace().iccProfile() : QByteArray();
    if (img.width() > 900) img = img.scaledToWidth(900, Qt::SmoothTransformation);
    img = img.convertToFormat(QImage::Format_RGBX64);
    w_ = img.width(); h_ = img.height();
    src16_.assign(size_t(w_) * h_ * 3, 0);
    for (int y = 0; y < h_; ++y) {
        const auto* line = reinterpret_cast<const quint16*>(img.constScanLine(y));
        for (int x = 0; x < w_; ++x)
            for (int c = 0; c < 3; ++c) src16_[(size_t(y) * w_ + x) * 3 + c] = line[x * 4 + c];
    }
    srcIsProPhoto_ = false;
    srcName_ = QStringLiteral("%1 · %2").arg(QFileInfo(path).fileName(),
                                             srcIcc_.isEmpty() ? QStringLiteral("无内嵌 ICC，按 sRGB")
                                                               : QStringLiteral("内嵌 ICC"));
    rebuild();
}

void OogCompareDialog::rebuild() {
    if (src16_.empty()) return;
    QApplication::setOverrideCursor(Qt::WaitCursor);

    cmsHPROFILE src = srcIsProPhoto_ ? jcolour::gamut::create_prophoto_profile()
                    : !srcIcc_.isEmpty() ? cmsOpenProfileFromMem(srcIcc_.constData(), cmsUInt32Number(srcIcc_.size()))
                                         : cmsCreate_sRGBProfile();
    if (!src || cmsGetColorSpace(src) != cmsSigRgbData) {
        if (src) cmsCloseProfile(src);
        src = cmsCreate_sRGBProfile();
    }
    bool simPrinter = printerIcc_.isEmpty();
    cmsHPROFILE printer = simPrinter ? createSimPrinterProfile()
                                     : cmsOpenProfileFromFile(QFile::encodeName(printerIcc_).constData(), "r");
    if (!printer) { simPrinter = true; printer = createSimPrinterProfile(); }
    cmsHPROFILE srgb = cmsCreate_sRGBProfile();
    cmsHPROFILE lab = cmsCreateLab4Profile(nullptr);
    const cmsUInt32Number pFmt = cmsFormatterForColorspaceOfProfile(printer, 2, FALSE);
    const int pCh = int(T_CHANNELS(pFmt));
    const size_t n = size_t(w_) * h_;

    // 源 Lab（统计用）
    std::vector<cmsCIELab> srcLab(n);
    if (cmsHTRANSFORM t = cmsCreateTransform(src, TYPE_RGB_16, lab, TYPE_Lab_DBL, INTENT_RELATIVE_COLORIMETRIC, 0)) {
        cmsDoTransform(t, src16_.data(), srcLab.data(), cmsUInt32Number(n));
        cmsDeleteTransform(t);
    }
    cmsHTRANSFORM toDisp = cmsCreateTransform(printer, pFmt, srgb, TYPE_RGB_8, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsHTRANSFORM toLab = cmsCreateTransform(printer, pFmt, lab, TYPE_Lab_DBL, INTENT_RELATIVE_COLORIMETRIC, 0);

    struct Out { QImage img; double meanDe = 0; double gradKeep = 0; int oog = 0; bool ok = false; };
    auto render = [&](OogStrategy s) {
        Out o;
        jcolour::gamut::OogTransform xf;
        if (!xf.create(src, printer, s, TYPE_RGB_16, pFmt) || !toDisp || !toLab) return o;
        std::vector<uint16_t> dev(n * size_t(pCh));
        xf.apply(src16_.data(), dev.data(), cmsUInt32Number(n));
        o.img = QImage(w_, h_, QImage::Format_RGB888);
        for (int y = 0; y < h_; ++y)
            cmsDoTransform(toDisp, &dev[size_t(y) * w_ * pCh], o.img.scanLine(y), cmsUInt32Number(w_));
        std::vector<cmsCIELab> outLab(n);
        cmsDoTransform(toLab, dev.data(), outLab.data(), cmsUInt32Number(n));
        const auto& b = xf.destinationBoundary();
        double sumDe = 0, gSrc = 0, gOut = 0;
        for (int y = 0; y < h_; y += 2)
            for (int x = 0; x + 1 < w_; x += 2) {
                const size_t i = size_t(y) * w_ + x;
                const Lab s0{srcLab[i].L, srcLab[i].a, srcLab[i].b};
                if (b.contains(s0, 0.5)) continue;
                ++o.oog;
                const Lab o0{outLab[i].L, outLab[i].a, outLab[i].b};
                sumDe += jcolour::delta_e::delta_e_2000(s0, o0);
                const Lab s1{srcLab[i + 1].L, srcLab[i + 1].a, srcLab[i + 1].b};
                const Lab o1{outLab[i + 1].L, outLab[i + 1].a, outLab[i + 1].b};
                gSrc += jcolour::delta_e::delta_e_2000(s0, s1);
                gOut += jcolour::delta_e::delta_e_2000(o0, o1);
            }
        if (o.oog) o.meanDe = sumDe / o.oog;
        o.gradKeep = gSrc > 1e-9 ? 100.0 * gOut / gSrc : 100.0;
        o.ok = true;
        return o;
    };
    const Out a = render(OogStrategy::VisualFidelity);
    const Out b = render(OogStrategy::ClosestColorimetric);
    const int sampled = ((h_ + 1) / 2) * (w_ / 2);

    if (toDisp) cmsDeleteTransform(toDisp);
    if (toLab) cmsDeleteTransform(toLab);
    cmsCloseProfile(lab);
    cmsCloseProfile(srgb);
    cmsCloseProfile(printer);
    cmsCloseProfile(src);
    QApplication::restoreOverrideCursor();

    srcInfo_->setText(QStringLiteral("源：%1　打印机：%2").arg(srcName_,
        simPrinter ? QStringLiteral("内置模拟喷墨纸色域（演示）") : QFileInfo(printerIcc_).fileName()));
    if (!a.ok || !b.ok) {
        stats_->setText(QStringLiteral("变换失败：打印机 ICC 不支持（需 RGB / CMYK / 灰度输出配置文件）。"));
        return;
    }
    left_->setImage(a.img);
    right_->setImage(b.img);
    left_->setFooter(QStringLiteral("色域外 ΔE00 平均 %1 · 渐变保留 %2%").arg(a.meanDe, 0, 'f', 1).arg(a.gradKeep, 0, 'f', 0));
    right_->setFooter(QStringLiteral("色域外 ΔE00 平均 %1 · 渐变保留 %2%").arg(b.meanDe, 0, 'f', 1).arg(b.gradKeep, 0, 'f', 0));
    const QString gradTxt = a.gradKeep > b.gradKeep + 1.0
        ? QStringLiteral("A 保留的层次更多（渐变保留 %1% vs %2%）").arg(a.gradKeep, 0, 'f', 0).arg(b.gradKeep, 0, 'f', 0)
        : b.gradKeep > a.gradKeep + 1.0
            ? QStringLiteral("这张图上 B 的相邻像素差反而更大（渐变保留 %1% vs %2%）——多半是色度最近把不同颜色推向边界不同位置，留意是否出现色相跳变")
                  .arg(b.gradKeep, 0, 'f', 0).arg(a.gradKeep, 0, 'f', 0)
            : QStringLiteral("两者层次保留相近（%1% / %2%）").arg(a.gradKeep, 0, 'f', 0).arg(b.gradKeep, 0, 'f', 0);
    stats_->setText(QStringLiteral(
        "超出打印机色域的像素约 <b>%1%</b>。色域外平均 ΔE00：A 视觉保真 %2，B 色度最近 %3（B 单色更接近原色）。%4。"
        "<br>A 保色相、把饱和区按比例压进色域；B 只动色域外的颜色，取最近的可打印色。"
        "两边都是真实 LCMS 变换（源 → Lab 映射 → 打印机 ICC → 屏幕），以眼睛判断为准。")
        .arg(sampled ? 100.0 * a.oog / sampled : 0.0, 0, 'f', 1)
        .arg(a.meanDe, 0, 'f', 1).arg(b.meanDe, 0, 'f', 1).arg(gradTxt));
}

}  // namespace jcolour::ui
