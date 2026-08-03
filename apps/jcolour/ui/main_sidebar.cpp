#include "main_sidebar.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

#include "../translations.h"
#include "icons.h"

// MainSidebar — 194px labelled sidebar matching design/ui-mockup.html:
// logo · group「主要」(列印 / 建檔校準) · group「其他」(記錄 / 設定) · device foot.

namespace jcolour {

namespace {
constexpr int kSidebarWidth = 194;

QLabel* groupLabel(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet("font-size:10px;letter-spacing:2px;color:#a3a8b0;"
                     "padding:12px 11px 5px;");
    return l;
}
}  // namespace

MainSidebar::MainSidebar(QWidget* parent) : QWidget(parent) {
    setObjectName("mainSidebar");
    setFixedWidth(kSidebarWidth);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "#mainSidebar { background:#f5f5f7; border-right:1px solid #d2d5d9; }"
        "#mainSidebar QPushButton {"
        "  text-align:left; padding:8px 11px; border:none; border-radius:7px;"
        "  font-size:13px; color:#3b3f46; background:transparent; }"
        "#mainSidebar QPushButton:hover { background:#ececef; }"
        "#mainSidebar QPushButton:checked {"
        "  background:#2e7dd7; color:#ffffff; font-weight:600; }"
        "QLabel#sideFoot { color:#a3a8b0; font-size:11px; padding:8px 11px; }"
    );

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 14, 10, 10);
    lay->setSpacing(3);

    // Logo: j + colour (accent)
    auto* logo = new QLabel(QStringLiteral(
        "<span style='color:#23262b'>j</span><span style='color:#2e7dd7'>colour</span>"));
    logo->setTextFormat(Qt::RichText);
    logo->setStyleSheet("font-size:17px;font-weight:700;letter-spacing:1px;padding:6px 10px 14px;");
    lay->addWidget(logo);

    group_ = new QButtonGroup(this);
    group_->setExclusive(true);

    const QColor navOff(0x5a, 0x5f, 0x66);
    const QColor navOn(Qt::white);
    auto add = [&](Page page, ic::Icon glyph, const QString& label) {
        auto* btn = new QPushButton(QStringLiteral("  ") + label);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setIcon(ic::toggleIcon(glyph, navOff, navOn, 18));
        btn->setIconSize(QSize(18, 18));
        buttons_.push_back(btn);
        group_->addButton(btn, static_cast<int>(page));
        lay->addWidget(btn);
    };

    lay->addWidget(groupLabel(QStringLiteral("主要")));
    add(PageJobs,            ic::Icon::Printer, QStringLiteral("列印"));
    add(PageColorManagement, ic::Icon::Sliders, QStringLiteral("建檔校準"));

    lay->addWidget(groupLabel(QStringLiteral("其他")));
    add(PageHistory,  ic::Icon::Clock,    QStringLiteral("記錄"));
    add(PageSettings, ic::Icon::Settings, QStringLiteral("設定"));

    lay->addStretch(1);

    version_ = new QLabel(QStringLiteral("P9500 · 已連線\n墨批 #A7 · 11 色就緒"));
    version_->setObjectName("sideFoot");
    lay->addWidget(version_);

    connect(group_, &QButtonGroup::idClicked, this, [this](int id){
        emit pageRequested(id);
    });

    if (!buttons_.isEmpty()) buttons_.first()->setChecked(true);
}

QWidget* MainSidebar::makeLogoHeader() { return nullptr; }

QToolButton* MainSidebar::makeNavButton(const QString&, const QString&, Page) {
    return nullptr;
}

void MainSidebar::setActive(Page page) {
    if (auto* btn = group_->button(static_cast<int>(page)))
        btn->setChecked(true);
}

void MainSidebar::setVersionText(const QString& text) {
    if (version_) version_->setText(text);
}

}  // namespace jcolour
