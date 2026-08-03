#include "black_linearization_panel.h"
#include "curve_editor.h"
#include "black_split.h"   // engine: QTR-style 3-black partition (sub-problem A)

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QTabBar>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>

namespace jcolour {
namespace ui {

namespace {
struct ChanDef { const char* label; const char* color; };

// Read-only preview of the engine's black_split partition: how an input neutral
// tone (white→black) is shared across LGY / GY / K — the layer BELOW per-ink
// linearization. Source of truth = calibration::neutral_ramp().
class BlackSplitPreview : public QWidget {
public:
    explicit BlackSplitPreview(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(360, 240);
        ramp_ = calibration::neutral_ramp(128);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = rect().adjusted(38, 12, -12, -26);
        p.fillRect(rect(), QColor("#fafbfc"));
        p.setPen(QColor("#e0e3e7"));
        for (int i = 0; i <= 4; ++i) {
            double y = r.top() + r.height() * i / 4.0;
            p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
            double x = r.left() + r.width() * i / 4.0;
            p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
        }
        p.setPen(QColor("#9aa0a8"));
        p.drawText(rect().adjusted(2, 0, 0, 0), Qt::AlignLeft | Qt::AlignTop, "墨量%");
        p.drawText(r.adjusted(0, 0, 0, 24), Qt::AlignLeft  | Qt::AlignBottom, "白");
        p.drawText(r.adjusted(0, 0, 0, 24), Qt::AlignRight | Qt::AlignBottom, "黑（输入色调）");

        struct Curve { const char* name; QColor col; double calibration::InkSplit::* f; };
        const Curve curves[] = {
            {"LGY 淺灰", QColor("#b4b4b4"), &calibration::InkSplit::lgy},
            {"GY 灰",    QColor("#7c7c7c"), &calibration::InkSplit::gy},
            {"K 主黑",   QColor("#1b1b1b"), &calibration::InkSplit::k},
        };
        const int N = static_cast<int>(ramp_.size());
        int li = 0;
        for (const auto& c : curves) {
            QPainterPath path;
            for (int i = 0; i < N; ++i) {
                double x = r.left() + r.width() * i / (N - 1);
                double y = r.bottom() - r.height() * (ramp_[i].*c.f);
                if (i == 0) path.moveTo(x, y); else path.lineTo(x, y);
            }
            p.setPen(QPen(c.col, 2.0));
            p.drawPath(path);
            const QRectF lg(r.left() + 6, r.top() + 4 + li * 15, 150, 14);
            p.fillRect(QRectF(lg.left(), lg.center().y() - 1.5, 14, 3), c.col);
            p.setPen(QColor("#555"));
            p.drawText(lg.adjusted(20, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, c.name);
            ++li;
        }
    }
private:
    std::vector<calibration::InkSplit> ramp_;
};
// PK/MK/GY/LGY — colours echo the calibration library + ink-channel widgets.
const ChanDef kChans[BlackLinearizationPanel::kChannels] = {
    {"PK · 照片黑",  "#1b1b1b"},
    {"MK · 啞光黑",  "#3a3a3a"},
    {"GY · 灰",      "#7c7c7c"},
    {"LGY · 淺灰",   "#b4b4b4"},
};
}  // namespace

BlackLinearizationPanel::BlackLinearizationPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    auto* hint = new QLabel(QStringLiteral(
        "每條黑墨道獨立線性化：輸入 L* → 墨量輸出。測量後在此拉平各通道響應。"));
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#868c95;font-size:11px;");
    root->addWidget(hint);

    tabs_ = new QTabWidget;
    tabs_->setDocumentMode(true);
    // Tab 0: engine分墨预览 (QTR-style 3-black partition; read-only)
    tabs_->addTab(new BlackSplitPreview, QStringLiteral("分墨（K/GY/LGY）"));
    for (int i = 0; i < kChannels; ++i) {
        auto* ed = new InkCurveEditor;
        ed->setMinimumSize(360, 240);           // tighter than the generic 12-ch panel
        editors_.append(ed);
        const int tabIdx = tabs_->addTab(ed, QString::fromUtf8(kChans[i].label));
        tabs_->tabBar()->setTabTextColor(tabIdx, QColor(kChans[i].color));
        connect(ed, &InkCurveEditor::curveChanged, this,
                &BlackLinearizationPanel::curvesChanged);
    }
    root->addWidget(tabs_, 1);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto* resetBtn = new QPushButton(QStringLiteral("全部重設為線性"));
    connect(resetBtn, &QPushButton::clicked, this, [this] { resetAll(); });
    btnRow->addWidget(resetBtn);
    root->addLayout(btnRow);
}

QVector<double> BlackLinearizationPanel::curve(int channel, int resolution) const {
    if (channel < 0 || channel >= editors_.size())
        return QVector<double>(resolution, 0.0);
    return editors_[channel]->getCurveValues(resolution);
}

void BlackLinearizationPanel::resetAll() {
    for (auto* ed : editors_) ed->setNeutralCurve();
    emit curvesChanged();
}

}  // namespace ui
}  // namespace jcolour
