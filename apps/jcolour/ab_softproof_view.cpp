#include "ab_softproof_view.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace jcolour {
namespace ui {

// A simple scaled-to-fit image pane with a caption.
class ImagePane : public QWidget {
public:
    explicit ImagePane(const QString& caption, QWidget* parent = nullptr)
        : QWidget(parent), caption_(caption) {
        setMinimumSize(220, 200);
        setStyleSheet("background:#2b2d31;");
    }
    void setImage(const QImage& img) { img_ = img; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor("#2b2d31"));
        QRect area = rect().adjusted(6, 6, -6, -26);
        if (!img_.isNull()) {
            QImage scaled = img_.scaled(area.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPoint at(area.x() + (area.width() - scaled.width()) / 2,
                      area.y() + (area.height() - scaled.height()) / 2);
            p.drawImage(at, scaled);
        } else {
            p.setPen(QColor("#7a7d82"));
            p.drawText(area, Qt::AlignCenter, QStringLiteral("拖入或載入影像"));
        }
        p.setPen(QColor("#c9ccd1"));
        QFont f = p.font(); f.setPointSizeF(9.5); f.setBold(true); p.setFont(f);
        p.drawText(QRect(0, height() - 22, width(), 20), Qt::AlignCenter, caption_);
    }

private:
    QString caption_;
    QImage  img_;
};

ABSoftProofView::ABSoftProofView(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    // top controls
    auto* bar = new QHBoxLayout;
    auto* loadBtn = new QPushButton(QStringLiteral("載入影像…"));
    loadBtn->setCursor(Qt::PointingHandCursor);
    connect(loadBtn, &QPushButton::clicked, this, [this] {
        QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("載入影像"), QString(),
            QStringLiteral("影像 (*.png *.jpg *.jpeg *.tif *.tiff)"));
        if (!path.isEmpty()) loadImage(path);
    });
    paperWhiteChk_ = new QCheckBox(QStringLiteral("紙白模擬"));
    paperWhiteChk_->setChecked(true);
    heatmapChk_    = new QCheckBox(QStringLiteral("ΔE / 超色域熱圖"));
    connect(paperWhiteChk_, &QCheckBox::toggled, this, [this] { rebuildProof(); });
    connect(heatmapChk_,    &QCheckBox::toggled, this, [this] { rebuildProof(); });
    bar->addWidget(loadBtn);
    bar->addSpacing(12);
    bar->addWidget(paperWhiteChk_);
    bar->addWidget(heatmapChk_);
    bar->addStretch();
    root->addLayout(bar);

    // side-by-side panes
    auto* split = new QSplitter(Qt::Horizontal);
    leftPane_  = new ImagePane(QStringLiteral("原圖（顯示）"));
    rightPane_ = new ImagePane(QStringLiteral("軟打樣預覽"));
    split->addWidget(leftPane_);
    split->addWidget(rightPane_);
    split->setSizes({400, 400});
    root->addWidget(split, 1);

    stats_ = new QLabel(QStringLiteral("尚未載入影像"));
    stats_->setStyleSheet("color:#868c95;font-size:11px;");
    root->addWidget(stats_);
}

void ABSoftProofView::setPaperWhite(double L, double a, double b) {
    pwL_ = L; pwA_ = a; pwB_ = b;
    rebuildProof();
}

void ABSoftProofView::loadImage(const QString& path) {
    QImage img(path);
    if (img.isNull()) { stats_->setText(QStringLiteral("無法載入：%1").arg(path)); return; }
    setSource(img);
}

void ABSoftProofView::setSource(const QImage& img) {
    source_ = img.convertToFormat(QImage::Format_RGB32);
    leftPane_->setImage(source_);
    rebuildProof();
}

void ABSoftProofView::rebuildProof() {
    if (source_.isNull()) return;

    // Paper-white tone: a light grey/tinted white derived from L*a*b*.
    double pwScale = std::clamp(pwL_ / 100.0, 0.5, 1.0);
    int pwTintR = static_cast<int>(std::clamp(pwA_ * 0.6, -12.0, 12.0));   // a* → red/green
    int pwTintB = static_cast<int>(std::clamp(-pwB_ * 0.6, -12.0, 12.0));  // b* → blue/yellow

    QImage proof = source_.copy();
    bool simWhite = paperWhiteChk_->isChecked();
    bool heat     = heatmapChk_->isChecked();

    qint64 flagged = 0;
    const qint64 total = static_cast<qint64>(proof.width()) * proof.height();
    for (int y = 0; y < proof.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(proof.scanLine(y));
        const auto* srcLine = reinterpret_cast<const QRgb*>(source_.scanLine(y));
        for (int x = 0; x < proof.width(); ++x) {
            int r = qRed(srcLine[x]), g = qGreen(srcLine[x]), b = qBlue(srcLine[x]);

            // approximate out-of-gamut: high saturation pixels
            int mx = std::max({r, g, b}), mn = std::min({r, g, b});
            double sat = mx > 0 ? static_cast<double>(mx - mn) / mx : 0.0;
            bool oog = sat > chromaThreshold_;
            if (oog) ++flagged;

            if (simWhite) {
                r = static_cast<int>(r * pwScale) + pwTintR;
                g = static_cast<int>(g * pwScale);
                b = static_cast<int>(b * pwScale) + pwTintB;
            }
            if (heat && oog) { r = 255; g = 0; b = 200; }   // magenta gamut warning
            line[x] = qRgb(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
        }
    }
    rightPane_->setImage(proof);

    double oogPct = total > 0 ? 100.0 * static_cast<double>(flagged) / total : 0.0;
    stats_->setText(QStringLiteral("%1 × %2 px　超色域估計 %3%%4")
        .arg(source_.width()).arg(source_.height())
        .arg(oogPct, 0, 'f', 1)
        .arg(simWhite ? QStringLiteral("　· 紙白 L*%1").arg(pwL_, 0, 'f', 0) : QString()));
}

}  // namespace ui
}  // namespace jcolour
