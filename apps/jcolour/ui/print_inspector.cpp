#include "print_inspector.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QDialog>

#include "print_gate_bar.h"
#include "../gamut_advisor_widget.h"
#include "../bw_mode_widget.h"
#include "../preview_widget.h"      // SoftProofingWidget
#include "../ab_softproof_view.h"
#include "oog_compare_dialog.h"
#include "render_intent_options.h"
#include "../test_print.h"
#include "../spot_color.h"
#include "../nesting.h"
#include "../theme.h"
#include "../translations.h"

namespace jcolour {
namespace ui {

namespace {

QLabel* subLabel(const QString& t, int w = 72) {
    auto* l = new QLabel(t);
    l->setFixedWidth(w);
    l->setStyleSheet("color:#868c95;font-size:11.5px;border:none;background:transparent;");
    return l;
}

// label + field, Mirage-style two-column row.
QWidget* frow(const QString& label, QWidget* field) {
    auto* w = new QWidget;
    w->setStyleSheet("background:transparent;");
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0); h->setSpacing(8);
    h->addWidget(subLabel(label));
    h->addWidget(field, 1);
    return w;
}

QComboBox* mkCombo(const QStringList& items, int cur = 0) {
    auto* c = new QComboBox; c->addItems(items); c->setCurrentIndex(cur);
    return c;
}

// numeric entry + unit selector.
QWidget* numUnit(const QString& val, const QStringList& units = {"cm", "in", "mm"}) {
    auto* w = new QWidget; w->setStyleSheet("background:transparent;");
    auto* h = new QHBoxLayout(w); h->setContentsMargins(0, 0, 0, 0); h->setSpacing(5);
    auto* e = new QLineEdit(val);
    auto* u = new QComboBox; u->addItems(units); u->setFixedWidth(62);
    h->addWidget(e, 1); h->addWidget(u);
    return w;
}

}  // namespace

QWidget* PrintInspector::makeSection(const QString& title, QWidget* content, bool open) {
    auto* card = new QWidget;
    card->setObjectName("sectCard");
    card->setStyleSheet("#sectCard{background:#fff;border:1px solid #d2d5d9;border-radius:8px;}");
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(0, 0, 0, 0); v->setSpacing(0);

    auto* header = new QPushButton;
    header->setCursor(Qt::PointingHandCursor);
    header->setStyleSheet(
        "QPushButton{border:none;background:transparent;text-align:left;"
        "font-size:12px;font-weight:700;color:#23262b;padding:10px 12px;}");
    v->addWidget(header);

    auto* body = new QWidget;
    body->setStyleSheet("background:transparent;border-top:1px solid #eee;");
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 10, 12, 12); bl->setSpacing(9);
    bl->addWidget(content);
    v->addWidget(body);

    const int idx = sections_.size();
    sections_.push_back({header, body, title});
    connect(header, &QPushButton::clicked, this, [this, idx] {
        const bool wasOpen = sections_[idx].body->isVisible();
        setOpen(wasOpen ? -1 : idx);
    });
    Q_UNUSED(open);
    return card;
}

void PrintInspector::setOpen(int idx) {
    for (int i = 0; i < sections_.size(); ++i) {
        const bool vis = (i == idx);
        sections_[i].body->setVisible(vis);
        sections_[i].header->setText(QString("%1   %2").arg(sections_[i].title, vis ? "▾" : "▸"));
    }
}

PrintInspector::PrintInspector(QWidget* parent) : QWidget(parent) {
    setFixedWidth(330);
    setObjectName("printInspector");
    setStyleSheet("#printInspector{background:#fafafb;border-left:1px solid #d2d5d9;}"
                  + jcolour::theme::pageStyle());
    setAttribute(Qt::WA_StyledBackground, true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* inner = new QWidget;
    inner->setStyleSheet("background:transparent;");
    auto* col = new QVBoxLayout(inner);
    col->setContentsMargins(10, 10, 10, 10);
    col->setSpacing(8);

    // ── 紙張與色彩 ───────────────────────────────────────────────
    {
        auto* w = new QWidget; w->setStyleSheet("background:transparent;");
        auto* l = new QVBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0); l->setSpacing(9);
        l->addWidget(frow(TR("ICC profile"), mkCombo({"PhotoRag_P9500_WF3", "Browse…"})));
        auto* intent = mkCombo(renderIntentLabels(), 0);   // 摄影默认：视觉保真
        intent->setToolTip(renderIntentTooltip());
        l->addWidget(frow(TR("Rendering intent"), intent));
        auto* oogAb = new QPushButton(QStringLiteral("色域外处理 A/B 对比…"));
        oogAb->setCursor(Qt::PointingHandCursor);
        connect(oogAb, &QPushButton::clicked, this, [this, intent] {
            auto* dlg = new OogCompareDialog(this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            connect(dlg, &OogCompareDialog::strategyChosen, intent, &QComboBox::setCurrentIndex);
            dlg->show();
        });
        l->addWidget(oogAb);
        gamut_ = new GamutAdvisorWidget;
        connect(gamut_, &GamutAdvisorWidget::applyRequested, this,
                [intent](int it, bool) { intent->setCurrentIndex(renderIndexForAdvice(it)); });
        l->addWidget(gamut_);
        col->addWidget(makeSection(QStringLiteral("紙張與色彩"), w, false));
    }

    // ── 黑白色調（預設展開 · 當前模式） ─────────────────────────
    int bwIndex = -1;
    {
        bw_ = new BWModeWidget;
        connect(bw_, &BWModeWidget::settingsChanged, this, &PrintInspector::jobModified);
        bwIndex = sections_.size();
        col->addWidget(makeSection(QStringLiteral("黑白色調"), bw_, true));
    }

    // ── 紙張（Paper · Mirage 版面） ─────────────────────────────
    {
        auto* w = new QWidget; w->setStyleSheet("background:transparent;");
        auto* l = new QVBoxLayout(w); l->setContentsMargins(0, 0, 0, 0); l->setSpacing(9);
        l->addWidget(frow(QStringLiteral("來源"), mkCombo({"紙捲（自動）· 裁切", "單張"})));
        l->addWidget(frow(QStringLiteral("寬度"), numUnit("60.96")));
        l->addWidget(frow(QStringLiteral("高度"), numUnit("191.05")));
        l->addWidget(frow(QStringLiteral("標準尺寸"), mkCombo({"24 in (61.0 cm)", "A2", "A1", "自訂"})));
        l->addWidget(frow(QStringLiteral("無邊"), mkCombo({"一般邊界", "無邊"})));
        col->addWidget(makeSection(QStringLiteral("紙張"), w, false));
    }

    // ── 影像尺寸（Image Size） ──────────────────────────────────
    {
        auto* w = new QWidget; w->setStyleSheet("background:transparent;");
        auto* l = new QVBoxLayout(w); l->setContentsMargins(0, 0, 0, 0); l->setSpacing(8);
        auto* grp = new QButtonGroup(w);
        for (const auto& t : {QStringLiteral("符合頁面"), QStringLiteral("符合寬度"),
                              QStringLiteral("填滿頁面"), QStringLiteral("自訂")}) {
            auto* rb = new QRadioButton(t);
            rb->setStyleSheet("color:#3b3f46;font-size:11.5px;background:transparent;");
            grp->addButton(rb); l->addWidget(rb);
            if (t == QStringLiteral("自訂")) rb->setChecked(true);
        }
        l->addWidget(frow(QStringLiteral("寬度"), numUnit("20.61", {"in", "cm", "mm"})));
        l->addWidget(frow(QStringLiteral("高度"), numUnit("13.71", {"in", "cm", "mm"})));
        col->addWidget(makeSection(QStringLiteral("影像尺寸"), w, false));
    }

    // ── 邊界（Border · 十字 margin） ────────────────────────────
    {
        auto* w = new QWidget; w->setStyleSheet("background:transparent;");
        auto* l = new QVBoxLayout(w); l->setContentsMargins(0, 0, 0, 0); l->setSpacing(9);
        auto* cross = new QWidget; cross->setStyleSheet("background:transparent;");
        auto* g = new QGridLayout(cross);
        g->setContentsMargins(0, 0, 0, 0); g->setSpacing(5);
        auto mk = [] { auto* e = new QLineEdit("4.00"); e->setFixedWidth(58);
                       e->setAlignment(Qt::AlignCenter); return e; };
        g->addWidget(mk(), 0, 1);
        g->addWidget(mk(), 1, 0);
        auto* link = new QLabel("⊞"); link->setAlignment(Qt::AlignCenter);
        link->setStyleSheet("color:#b0b5bd;border:none;background:transparent;");
        g->addWidget(link, 1, 1);
        g->addWidget(mk(), 1, 2);
        g->addWidget(mk(), 2, 1);
        l->addWidget(cross, 0, Qt::AlignHCenter);
        l->addWidget(frow(QStringLiteral("單位"), mkCombo({"cm", "in", "mm"})));
        auto* grp = new QButtonGroup(w);
        for (const auto& t : {QStringLiteral("留白"), QStringLiteral("填色"), QStringLiteral("鏡像延伸")}) {
            auto* rb = new QRadioButton(t);
            rb->setStyleSheet("color:#3b3f46;font-size:11.5px;background:transparent;");
            grp->addButton(rb); l->addWidget(rb);
            if (t == QStringLiteral("留白")) rb->setChecked(true);
        }
        l->addWidget(frow(QStringLiteral("繃框邊"), mkCombo({"無", "2 cm", "3 cm", "4 cm"})));
        col->addWidget(makeSection(QStringLiteral("邊界"), w, false));
    }

    // ── 旋轉 / 裁切（Rotation / Crop） ──────────────────────────
    {
        auto* w = new QWidget; w->setStyleSheet("background:transparent;");
        auto* l = new QVBoxLayout(w); l->setContentsMargins(0, 0, 0, 0); l->setSpacing(9);
        l->addWidget(frow(QStringLiteral("旋轉"), mkCombo({"不旋轉", "90° 順時針", "90° 逆時針", "180°", "自動"})));
        l->addWidget(frow(QStringLiteral("裁切寬"), numUnit("67.20")));
        l->addWidget(frow(QStringLiteral("裁切高"), numUnit("44.68")));
        l->addWidget(frow(QStringLiteral("錨點"), mkCombo({"置中", "左上", "右上", "左下", "右下"})));
        l->addWidget(frow(QStringLiteral("水平位移"), numUnit("0.00")));
        l->addWidget(frow(QStringLiteral("垂直位移"), numUnit("0.00")));
        auto* btnRow = new QWidget; btnRow->setStyleSheet("background:transparent;");
        auto* bh = new QHBoxLayout(btnRow); bh->setContentsMargins(0, 0, 0, 0); bh->setSpacing(7);
        bh->addWidget(new QPushButton(QStringLiteral("裁切…")));
        bh->addWidget(new QPushButton(QStringLiteral("重設")));
        bh->addStretch();
        l->addWidget(btnRow);
        col->addWidget(makeSection(QStringLiteral("旋轉 / 裁切"), w, false));
    }

    // ── 軟打樣 ───────────────────────────────────────────────────
    {
        auto* w = new QWidget; w->setStyleSheet("background:transparent;");
        auto* l = new QVBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0); l->setSpacing(8);
        softProof_ = new SoftProofingWidget;
        connect(softProof_, &SoftProofingWidget::softProofSettingsChanged,
                this, &PrintInspector::jobModified);
        l->addWidget(softProof_);
        auto* ab = new QPushButton(TR("A/B Side-by-Side…"));
        ab->setCursor(Qt::PointingHandCursor);
        connect(ab, &QPushButton::clicked, this, [this] {
            auto* dlg = new QDialog(this);
            dlg->setWindowTitle(TR("A/B Soft-Proof Compare"));
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->resize(900, 560);
            auto* dl = new QVBoxLayout(dlg); dl->setContentsMargins(0, 0, 0, 0);
            auto* view = new ABSoftProofView(dlg);
            view->setPaperWhite(softProof_->getPaperWhiteL(), softProof_->getPaperWhiteA(),
                                softProof_->getPaperWhiteB());
            dl->addWidget(view);
            dlg->show();
        });
        l->addWidget(ab);
        col->addWidget(makeSection(QStringLiteral("軟打樣"), w, false));
    }

    // ── 拼版 / 版次 / 工具 ───────────────────────────────────────
    {
        auto* w = new QWidget; w->setStyleSheet("background:transparent;");
        auto* l = new QVBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0); l->setSpacing(7);
        auto openDialog = [this](QWidget* dlgContent, const QString& title, int wpx, int hpx) {
            auto* dlg = new QDialog(this);
            dlg->setWindowTitle(title);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->resize(wpx, hpx);
            auto* dl = new QVBoxLayout(dlg); dl->setContentsMargins(0, 0, 0, 0);
            dlgContent->setParent(dlg);
            dl->addWidget(dlgContent);
            dlg->show();
        };
        auto* nestBtn = new QPushButton(QStringLiteral("拼版…"));
        connect(nestBtn, &QPushButton::clicked, this, [this, openDialog] {
            openDialog(new NestingWidget, QStringLiteral("拼版"), 760, 560); });
        auto* testBtn = new QPushButton(QStringLiteral("測試列印…"));
        connect(testBtn, &QPushButton::clicked, this, [this] {
            auto* d = new TestPrintDialog(this); d->setAttribute(Qt::WA_DeleteOnClose); d->show(); });
        auto* spotBtn = new QPushButton(QStringLiteral("專色校正…"));
        connect(spotBtn, &QPushButton::clicked, this, [this] {
            auto* d = new HoneycombSpotDialog(this); d->setAttribute(Qt::WA_DeleteOnClose); d->show(); });
        for (auto* b : {nestBtn, testBtn, spotBtn}) { b->setCursor(Qt::PointingHandCursor); l->addWidget(b); }
        col->addWidget(makeSection(QStringLiteral("拼版 / 版次 / 工具"), w, false));
    }

    col->addStretch(1);
    scroll->setWidget(inner);
    root->addWidget(scroll, 1);

    // Accordion: open only the current-mode section (黑白色調) at start.
    setOpen(bwIndex);
}

}  // namespace ui
}  // namespace jcolour
