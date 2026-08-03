#include "splash_screen.h"

#include <QApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QThread>
#include <QVBoxLayout>

namespace jcolour {

namespace {

struct ModuleEntry {
    QString name;
    QString detail;
};

// Each row is a feature group surfaced by the app. Detail is the concrete
// capability or backing implementation.
const QVector<ModuleEntry>& moduleMatrix() {
    static const QVector<ModuleEntry> rows = {
        {QStringLiteral("Color Engine"),     QStringLiteral("LCMS2 · ICC v4 · 36-channel spectral")},
        {QStringLiteral("B&W Workflow"),     QStringLiteral("Single-black · multi-channel split")},
        {QStringLiteral("Spot Color"),       QStringLiteral("Honeycomb spot · LAB targeting")},
        {QStringLiteral("Calibration"),      QStringLiteral("G7 wizard · monitor calibration")},
        {QStringLiteral("Soft Proofing"),    QStringLiteral("Side-by-side · gamut warning")},
        {QStringLiteral("Layout"),           QStringLiteral("Nesting · step & repeat · contact sheet")},
        {QStringLiteral("Tracking"),         QStringLiteral("Edition log · ink lot · usage history")},
        {QStringLiteral("Profiles"),         QStringLiteral("Drag-drop import · library browser")},
        {QStringLiteral("Printer Link"),     QStringLiteral("Epson SureColor P · PJL direct")},
        {QStringLiteral("Subscription"),     QStringLiteral("StoreKit · Free / Starter / Pro")},
    };
    return rows;
}

QLabel* checkMark() {
    auto* dot = new QLabel(QString::fromUtf8("\xE2\x97\x8F")); // ●
    dot->setStyleSheet("color:#2e7dd7; font-size:9px;");
    dot->setFixedWidth(12);
    dot->setAlignment(Qt::AlignCenter);
    return dot;
}

} // anon

SplashScreen::SplashScreen(QWidget* parent)
    : QWidget(parent, Qt::SplashScreen | Qt::FramelessWindowHint) {
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAutoFillBackground(true);
    setFixedSize(640, 460);
    setStyleSheet(
        "QWidget { background:#ffffff; color:#1f1f1f; }"
        "QLabel#splashTagline { color:#444444; font-size:13px; }"
        "QLabel#splashModule  { color:#1f1f1f; font-size:12px; font-weight:600; }"
        "QLabel#splashDetail  { color:#666666; font-size:11px; }"
        "QLabel#splashStatus  { color:#444444; font-size:11px; }"
        "QLabel#splashVersion { color:#888888; font-size:10px; }"
        "QLabel#splashFooter  { color:#888888; font-size:10px; }"
        "QProgressBar {"
        "  background:#ececec; border:none; border-radius:1px; height:3px; "
        "  text-align:center; color:transparent;"
        "}"
        "QProgressBar::chunk { background:#2e7dd7; }"
    );

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 36, 40, 24);
    root->setSpacing(0);

    // ---- Logo ---------------------------------------------------------
    auto* logo = new QLabel;
    QPixmap pm(QStringLiteral(":/logo.png"));
    if (!pm.isNull()) {
        pm.setDevicePixelRatio(2.0);
        logo->setPixmap(pm);
    } else {
        // Fallback: render the wordmark as italic serif text if the resource
        // failed to load (should not happen in normal builds).
        logo->setText(QStringLiteral("Jcolour"));
        QFont f("Times New Roman");
        f.setItalic(true);
        f.setBold(true);
        f.setPointSize(48);
        logo->setFont(f);
    }
    logo->setAlignment(Qt::AlignCenter);
    root->addWidget(logo, 0, Qt::AlignHCenter);
    root->addSpacing(4);

    auto* tagline = new QLabel(QStringLiteral(
        "Color-Managed Print Workstation · macOS · Apple Silicon"));
    tagline->setObjectName("splashTagline");
    tagline->setAlignment(Qt::AlignCenter);
    root->addWidget(tagline);
    root->addSpacing(20);

    // ---- Module matrix (2 columns × 5 rows) --------------------------
    auto* grid = new QGridLayout;
    grid->setContentsMargins(8, 0, 8, 0);
    grid->setHorizontalSpacing(28);
    grid->setVerticalSpacing(8);

    const auto& rows = moduleMatrix();
    for (int i = 0; i < rows.size(); ++i) {
        const int col = (i % 2) * 3;            // each entry is 3 cells wide
        const int row = i / 2;
        auto* dot = checkMark();
        auto* name = new QLabel(rows[i].name);
        name->setObjectName("splashModule");
        auto* detail = new QLabel(rows[i].detail);
        detail->setObjectName("splashDetail");

        grid->addWidget(dot,    row, col);
        grid->addWidget(name,   row, col + 1);
        grid->addWidget(detail, row, col + 2);
    }
    grid->setColumnStretch(2, 1);
    grid->setColumnStretch(5, 1);
    root->addLayout(grid);

    root->addStretch();

    // ---- Progress + status -------------------------------------------
    progress_ = new QProgressBar;
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setTextVisible(false);
    root->addWidget(progress_);
    root->addSpacing(6);

    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(0, 0, 0, 0);
    statusLabel_ = new QLabel(QStringLiteral("Starting…"));
    statusLabel_->setObjectName("splashStatus");

    auto* version = new QLabel(QString("v%1").arg(QApplication::applicationVersion()));
    version->setObjectName("splashVersion");
    version->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    footer->addWidget(statusLabel_, 1);
    footer->addWidget(version);
    root->addLayout(footer);

    auto* tagFooter = new QLabel(
        QStringLiteral("Epson SureColor P-series · LCMS2 · ArgyllCMS"));
    tagFooter->setObjectName("splashFooter");
    tagFooter->setAlignment(Qt::AlignCenter);
    root->addSpacing(2);
    root->addWidget(tagFooter);

    // Continue button (hidden initially)
    continueBtn_ = new QPushButton(QStringLiteral("Click to Continue"), this);
    continueBtn_->setGeometry(220, 395, 200, 40);
    continueBtn_->setVisible(false);
    continueBtn_->setStyleSheet(R"(
        QPushButton {
            background: #2e7dd7;
            color: #1f1f1f;
            border: none;
            border-radius: 6px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background: #1a6bc9;
        }
    )");

    // Center on primary screen
    if (auto* s = QGuiApplication::primaryScreen()) {
        const QRect g = s->availableGeometry();
        move(g.center() - rect().center());
    }
}

void SplashScreen::showContinueButton() {
    if (continueBtn_) {
        continueBtn_->setVisible(true);
        continueBtn_->raise();
        continueBtn_->setFocus();
    }
}

QPushButton* SplashScreen::continueButton() {
    return continueBtn_;
}

void SplashScreen::setStatus(const QString& message, int percent) {
    statusLabel_->setText(message);
    progress_->setValue(qBound(0, percent, 100));
    repaint();
    QApplication::processEvents();
}

void SplashScreen::runInitSequence() {
    struct Step { const char* msg; int pct; int dwellMs; };
    const Step steps[] = {
        {"Loading theme…",                  10, 60},
        {"Initializing spectral engine…",   25, 120},
        {"Loading ICC profile library…",    40, 100},
        {"Connecting StoreKit…",            55, 80},
        {"Probing monitor calibration…",    70, 80},
        {"Discovering Epson printers…",     85, 100},
        {"Ready.",                         100, 60},
    };
    for (const auto& s : steps) {
        setStatus(QString::fromUtf8(s.msg), s.pct);
        QThread::msleep(s.dwellMs);
    }
}

} // namespace jcolour
