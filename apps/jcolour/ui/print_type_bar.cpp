#include "print_type_bar.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

namespace jcolour {

namespace {

// ---- Style A · Mirage 專業淺色 palette --------------------------------
constexpr const char* kAccent     = "#2e7dd7";
constexpr const char* kAccentSoft = "#eaf2fc";
constexpr const char* kCard       = "#ffffff";
constexpr const char* kCardHover  = "#f4f8fd";
constexpr const char* kBorder     = "#d2d5d9";
constexpr const char* kBarBg      = "#e3e5e8";
constexpr const char* kTitle      = "#23262b";
constexpr const char* kSub        = "#868c95";
constexpr const char* kHeader     = "#868c95";

// A single selectable print-type card.
class TypeCard : public QToolButton {
public:
    TypeCard(const QString& title, const QString& subtitle, QWidget* parent = nullptr)
        : QToolButton(parent), title_(title), subtitle_(subtitle) {
        setCheckable(true);
        setAutoRaise(true);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(40);
    }

protected:
    QSize sizeHint() const override { return {120, 40}; }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const bool active = isChecked();
        const bool hover  = underMouse();

        QRectF r = rect().adjusted(1.0, 1.0, -1.0, -1.0);
        QPainterPath card;
        card.addRoundedRect(r, 7, 7);

        const QColor fill = active ? QColor(kAccentSoft)
                          : hover  ? QColor(kCardHover)
                                   : QColor(kCard);
        p.fillPath(card, fill);

        QPen border(active ? QColor(kAccent) : QColor(kBorder));
        border.setWidthF(active ? 2.0 : 1.0);
        p.setPen(border);
        p.drawPath(card);

        // Title
        QFont tf = font();
        tf.setPointSizeF(12.0);
        tf.setBold(true);
        p.setFont(tf);
        p.setPen(active ? QColor(kAccent) : QColor(kTitle));
        p.drawText(QRectF(11, 5, width() - 22, 17),
                   Qt::AlignLeft | Qt::AlignVCenter, title_);

        // Subtitle
        QFont sf = font();
        sf.setPointSizeF(8.5);
        p.setFont(sf);
        p.setPen(QColor(kSub));
        p.drawText(QRectF(11, height() - 19, width() - 22, 15),
                   Qt::AlignLeft | Qt::AlignVCenter, subtitle_);
    }

private:
    QString title_;
    QString subtitle_;
};

}  // namespace

PrintTypeBar::PrintTypeBar(QWidget* parent) : QWidget(parent) {
    setObjectName("printTypeBar");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QString("#printTypeBar { background:%1; "
                          "border-bottom:1px solid %2; }")
                      .arg(kBarBg, kBorder));

    // The 5 print types (canonical workflow ids 1-4, 6 — see workflow_selector.cpp).
    types_ = {
        {1, QStringLiteral("單黑"),          QStringLiteral("照片黑 / 粗面黑 · 灰階")},
        {2, QStringLiteral("四色黑"),        QStringLiteral("黑+灰+淺灰+粗面黑 · 灰階")},
        {3, QStringLiteral("黑白＋彩色"),    QStringLiteral("中性＋彩色輔助 · 灰階")},
        {4, QStringLiteral("RGB"),           QStringLiteral("全色 · RGB")},
        {6, QStringLiteral("ProPhoto RGB"),  QStringLiteral("全色 · 寬色域")},
    };

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 8, 14, 8);
    root->setSpacing(0);

    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);

    group_ = new QButtonGroup(this);
    group_->setExclusive(true);

    for (int i = 0; i < types_.size(); ++i) {
        auto* card = new TypeCard(types_[i].name, types_[i].subtitle, this);
        group_->addButton(card, i);
        row->addWidget(card, 1);
    }
    root->addLayout(row);

    connect(group_, &QButtonGroup::idClicked, this, [this](int idx) {
        if (idx < 0 || idx >= types_.size()) return;
        emit typeSelected(types_[idx].id, types_[idx].name);
    });
}

QString PrintTypeBar::currentTypeName() const {
    int id = group_->checkedId();
    if (id < 0 || id >= types_.size()) return QString();
    return types_[id].name;
}

int PrintTypeBar::currentTypeId() const {
    int idx = group_->checkedId();
    if (idx < 0 || idx >= types_.size()) return -1;
    return types_[idx].id;
}

}  // namespace jcolour
