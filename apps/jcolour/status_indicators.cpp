#include "status_indicators.h"
#include <QHBoxLayout>
#include <QPainter>

namespace jcolour {

ConnectionBadge::ConnectionBadge(const QString& label, QWidget* parent)
    : QWidget(parent), labelText_(label) {
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 0, 8, 0);
    lay->setSpacing(6);

    dot_ = new QLabel("●");
    dot_->setStyleSheet("color:#cc0000;");

    labelLeft_ = new QLabel(label);
    labelLeft_->setStyleSheet("color:#ffffff;");

    subtitle_ = new QLabel;
    subtitle_->setStyleSheet("color:#555555;");

    lay->addWidget(dot_);
    lay->addWidget(labelLeft_);
    lay->addWidget(subtitle_);
}

void ConnectionBadge::setState(State s) {
    state_ = s;
    refresh();
}

void ConnectionBadge::setSubtitle(const QString& s) {
    subtitle_->setText(s);
}

void ConnectionBadge::refresh() {
    QString c;
    switch (state_) {
        case Connected:    c = "#2f8c2f"; break;
        case Disconnected: c = "#cc0000"; break;
        case Warning:      c = "#c68900"; break;
    }
    dot_->setStyleSheet("color:" + c + ";");
}

// --- InkLevelStrip --------------------------------------------------------

static const QStringList kChanNames = {
    "PK","MK","LC","C","LM","M","Y","Or","LK","LLK","V","Gr"
};

static QColor inkColor(int i) {
    static const QColor colors[12] = {
        QColor("#1c1c1e"), // PK
        QColor("#3a3a3c"), // MK
        QColor("#7dd3fc"), // LC
        QColor("#0bb5dd"), // C
        QColor("#f9a8d4"), // LM
        QColor("#e50079"), // M
        QColor("#ffe800"), // Y
        QColor("#ff7b00"), // Or
        QColor("#48484a"), // LK
        QColor("#8e8e93"), // LLK
        QColor("#5e5ce6"), // V
        QColor("#aeaeb2"), // Gr
    };
    return colors[qBound(0, i, 11)];
}

InkLevelStrip::InkLevelStrip(QWidget* parent) : QWidget(parent) {
    setFixedSize(180, 22);
    setToolTip("Ink levels");
    levels_ = QVector<double>(12, 0.6);
}

void InkLevelStrip::setLevels(const QVector<double>& levels) {
    levels_ = levels;
    if (levels_.size() < 12) levels_.resize(12);
    update();
}

void InkLevelStrip::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int n = 12;
    int gap = 1;
    int barW = (width() - gap * (n - 1)) / n;
    int barH = height() - 4;

    for (int i = 0; i < n; ++i) {
        int x = i * (barW + gap);
        QRect track(x, 2, barW, barH);
        p.fillRect(track, QColor("#2c2c2e"));

        double lvl = qBound(0.0, levels_.value(i, 0.0), 1.0);
        QRect fill(x, 2 + int((1 - lvl) * barH), barW, int(lvl * barH));
        QColor c = inkColor(i);
        if (lvl < 0.15) c = QColor("#ff453a");
        else if (lvl < 0.3) c = QColor("#ff9f0a");
        p.fillRect(fill, c);
    }
}

} // namespace jcolour
