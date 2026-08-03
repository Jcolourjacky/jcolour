#include "job_inspector.h"
#include "bw_mode_widget.h"
#include "gamut_advisor_widget.h"
#include "ab_softproof_view.h"
#include "curve_editor.h"
#include "preview_widget.h"  // SoftProofingWidget
#include "translations.h"

#include "calibration.h"           // G7CalibrationWizard, InkCartridgeTracker
#include "contact_sheet.h"         // jcolour::ContactSheetDialog
#include "ink_lot_tracker.h"       // jcolour::InkLotTracker
#include "monitor_calibration.h"   // jcolour::monitor::MonitorCalibrationMonitor
#include "nesting.h"               // ::NestingWidget
#include "profile_drag_drop.h"     // jcolour::ProfileLibraryBrowser, ProfileImportDialog
#include "queue.h"                 // ::PrintQueueWidget, ::EditionTrackingDialog
#include "spot_color.h"            // ::HoneycombSpotDialog
#include "step_repeat.h"           // jcolour::StepRepeatDialog
#include "storekit_manager.h"      // jcolour::store::StoreKitManager
#include "test_print.h"            // jcolour::TestPrintDialog

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QScrollArea>
#include <QFileInfo>
#include <QDialog>
#include <QMessageBox>
#include <QPointer>

namespace jcolour {

namespace {

// Wrap a non-QDialog widget in a modeless QDialog so it can be shown stand-alone.
QDialog* wrapInDialog(QWidget* content, const QString& title, QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(title);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    auto* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(0, 0, 0, 0);
    content->setParent(dlg);
    lay->addWidget(content);
    dlg->resize(content->sizeHint().expandedTo(QSize(640, 480)));
    return dlg;
}

QString tierName(jcolour::store::SubscriptionTier t) {
    using T = jcolour::store::SubscriptionTier;
    switch (t) {
        case T::Free:    return QStringLiteral("Free");
        case T::Starter: return QStringLiteral("Starter");
        case T::Pro:     return QStringLiteral("Pro");
    }
    return QStringLiteral("Unknown");
}

QString statusName(jcolour::store::SubscriptionStatus s) {
    using S = jcolour::store::SubscriptionStatus;
    switch (s) {
        case S::Active:      return QStringLiteral("Active");
        case S::Expired:     return QStringLiteral("Expired");
        case S::Cancelled:   return QStringLiteral("Cancelled");
        case S::Pending:     return QStringLiteral("Pending");
        case S::GracePeriod: return QStringLiteral("Grace period");
        case S::Unknown:     return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

QString monitorStatusName(jcolour::monitor::CalibrationStatus s) {
    using St = jcolour::monitor::CalibrationStatus;
    switch (s) {
        case St::NotCalibrated:        return QStringLiteral("Not calibrated");
        case St::RecentlyCalibrated:   return QStringLiteral("Recently calibrated");
        case St::CalibrationExpired:   return QStringLiteral("Calibration expired");
        case St::OutOfGamut:           return QStringLiteral("Out of gamut");
        case St::Unknown:              return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

QLabel* sectionLabel(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("inspectorSectionTitle");
    return l;
}

QWidget* swatch(const QColor& c) {
    auto* w = new QLabel;
    w->setObjectName("swatch");
    w->setStyleSheet(QString("background:%1; border:1px solid #767676; "
                             "border-radius:2px; min-width:12px; max-width:12px;"
                             "min-height:12px; max-height:12px;").arg(c.name()));
    return w;
}

QHBoxLayout* makeChannelRow(const QColor& sw, QSlider*& sliderOut, QLabel*& valueOut) {
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);
    row->addWidget(swatch(sw));
    sliderOut = new QSlider(Qt::Horizontal);
    sliderOut->setRange(-100, 100);
    sliderOut->setValue(0);
    valueOut = new QLabel("0 %");
    valueOut->setStyleSheet("color:#555555; min-width:36px;");
    valueOut->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(sliderOut, 1);
    row->addWidget(valueOut);
    return row;
}

} // anon

JobInspector::JobInspector(QWidget* parent) : QWidget(parent) {
    setObjectName("jobInspector");
    setMinimumWidth(420);
    setStyleSheet("QWidget#jobInspector { background:#e6e6e6; "
                  "border-left:1px solid #b8b8b8; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    topTabs_ = new QTabWidget;
    topTabs_->setDocumentMode(false);
    topTabs_->setTabPosition(QTabWidget::North);
    topTabs_->addTab(buildInputTab(),     TR("Input"));
    topTabs_->addTab(buildColorTab(),     TR("Color"));
    topTabs_->addTab(buildPrintTab(),     TR("Print"));
    topTabs_->addTab(buildFinishingTab(), TR("Finishing"));
    topTabs_->addTab(buildSystemTab(),    TR("System"));
    topTabs_->setCurrentIndex(1); // Color by default
    layout->addWidget(topTabs_);
}

// ---- Input ---------------------------------------------------------------
QWidget* JobInspector::buildInputTab() {
    auto* page = new QWidget;
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(6);

    lay->addWidget(sectionLabel("Image"));
    imagePathLabel_ = new QLabel("(no image loaded)");
    imagePathLabel_->setWordWrap(true);
    imagePathLabel_->setStyleSheet("color:#555555;");
    lay->addWidget(imagePathLabel_);

    imageInfoLabel_ = new QLabel("—");
    imageInfoLabel_->setStyleSheet("color:#888888; font-size:11px;");
    lay->addWidget(imageInfoLabel_);

    lay->addWidget(sectionLabel("Source profile"));
    sourceProfileCombo_ = new QComboBox;
    sourceProfileCombo_->addItems({
        "Embedded (auto)",
        "sRGB IEC61966-2.1",
        "Adobe RGB (1998)",
        "ProPhoto RGB",
        "Display P3"
    });
    lay->addWidget(sourceProfileCombo_);

    lay->addStretch();
    return page;
}

// ---- Color (sub-tabs) ----------------------------------------------------
QWidget* JobInspector::buildColorTab() {
    auto* page = new QWidget;
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    colorSubTabs_ = new QTabWidget;
    colorSubTabs_->setDocumentMode(true);

    // -- Adjust (Farbanpassung mirror) --
    auto* adjPage = new QWidget;
    auto* adjScroll = new QScrollArea;
    adjScroll->setWidgetResizable(true);
    adjScroll->setFrameShape(QFrame::NoFrame);
    adjScroll->setWidget(adjPage);
    auto* adj = new QVBoxLayout(adjPage);
    adj->setContentsMargins(12, 10, 12, 10);
    adj->setSpacing(4);

    adj->addWidget(sectionLabel("Channel corrections"));
    adj->addLayout(makeChannelRow(QColor("#0BB5DD"), slC_, lblC_));
    adj->addLayout(makeChannelRow(QColor("#E50079"), slM_, lblM_));
    adj->addLayout(makeChannelRow(QColor("#FFE800"), slY_, lblY_));
    adj->addLayout(makeChannelRow(QColor("#ffffff"), slK_, lblK_));
    adj->addSpacing(4);
    adj->addLayout(makeChannelRow(QColor("#cc0000"), slR_, lblR_));
    adj->addLayout(makeChannelRow(QColor("#2f8c2f"), slG_, lblG_));
    adj->addLayout(makeChannelRow(QColor("#316ac5"), slB_, lblB_));

    adj->addWidget(sectionLabel("Brightness / contrast / saturation"));
    adj->addLayout(makeChannelRow(QColor("#f5f5f5"), slBrightness_, lblBrightness_));
    adj->addLayout(makeChannelRow(QColor("#888888"), slContrast_,   lblContrast_));
    adj->addLayout(makeChannelRow(QColor("#c68900"), slSaturation_, lblSaturation_));

    adj->addWidget(sectionLabel("Sharpen / soften"));
    adj->addLayout(makeChannelRow(QColor("#5a5acb"), slSharpen_,    lblSharpen_));

    auto* resetRow = new QHBoxLayout;
    resetRow->addStretch();
    auto* resetBtn = new QPushButton("Reset");
    resetRow->addWidget(resetBtn);
    adj->addSpacing(6);
    adj->addLayout(resetRow);
    adj->addStretch();

    auto wireSlider = [this](QSlider* s, QLabel* l) {
        connect(s, &QSlider::valueChanged, l, [l](int v) {
            l->setText(QString("%1 %").arg(v));
        });
        connect(s, &QSlider::valueChanged, this, [this] {
            if (!blockSignals_) emit jobModified();
        });
    };
    wireSlider(slC_, lblC_); wireSlider(slM_, lblM_);
    wireSlider(slY_, lblY_); wireSlider(slK_, lblK_);
    wireSlider(slR_, lblR_); wireSlider(slG_, lblG_);
    wireSlider(slB_, lblB_);
    wireSlider(slBrightness_, lblBrightness_);
    wireSlider(slContrast_,   lblContrast_);
    wireSlider(slSaturation_, lblSaturation_);
    wireSlider(slSharpen_,    lblSharpen_);

    connect(resetBtn, &QPushButton::clicked, this, [this] {
        for (auto* s : {slC_, slM_, slY_, slK_, slR_, slG_, slB_,
                        slBrightness_, slContrast_, slSaturation_, slSharpen_}) {
            s->setValue(0);
        }
    });

    auto* adjWrap = new QWidget;
    auto* adjWrapLay = new QVBoxLayout(adjWrap);
    adjWrapLay->setContentsMargins(0, 0, 0, 0);
    adjWrapLay->addWidget(adjScroll);
    colorSubTabs_->addTab(adjWrap, TR("Adjust"));

    // -- Color Management (incl. soft proofing) --
    auto* mgmtPage = new QWidget;
    auto* mgmtScroll = new QScrollArea;
    mgmtScroll->setWidgetResizable(true);
    mgmtScroll->setFrameShape(QFrame::NoFrame);
    mgmtScroll->setWidget(mgmtPage);
    auto* mgmt = new QVBoxLayout(mgmtPage);
    mgmt->setContentsMargins(12, 10, 12, 10);
    mgmt->setSpacing(4);

    mgmt->addWidget(sectionLabel("Output ICC profile"));
    iccProfileCombo_ = new QComboBox;
    iccProfileCombo_->setEditable(false);
    iccProfileCombo_->addItem("Browse...");
    mgmt->addWidget(iccProfileCombo_);

    mgmt->addWidget(sectionLabel("Rendering intent"));
    intentCombo_ = new QComboBox;
    intentCombo_->addItems({"Perceptual", "Relative Colorimetric",
                            "Saturation", "Absolute Colorimetric"});
    intentCombo_->setCurrentIndex(1);
    mgmt->addWidget(intentCombo_);

    bpcCheck_ = new QCheckBox("Black Point Compensation");
    bpcCheck_->setChecked(true);
    mgmt->addWidget(bpcCheck_);

    mgmt->addWidget(sectionLabel("Gamut advisor"));
    auto* gamutAdvisor = new jcolour::ui::GamutAdvisorWidget;
    connect(gamutAdvisor, &jcolour::ui::GamutAdvisorWidget::applyRequested, this,
            [this](int intent, bool bpc) {
        intentCombo_->setCurrentIndex(intent);
        bpcCheck_->setChecked(bpc);
        if (!blockSignals_) emit jobModified();
    });
    mgmt->addWidget(gamutAdvisor);

    mgmt->addWidget(sectionLabel("Soft proofing"));
    softProof_ = new SoftProofingWidget;
    mgmt->addWidget(softProof_);

    auto* abBtn = new QPushButton(TR("A/B Side-by-Side…"));
    abBtn->setCursor(Qt::PointingHandCursor);
    connect(abBtn, &QPushButton::clicked, this, [this] {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle(TR("A/B Soft-Proof Compare"));
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->resize(900, 560);
        auto* dl = new QVBoxLayout(dlg);
        dl->setContentsMargins(0, 0, 0, 0);
        auto* ab = new jcolour::ui::ABSoftProofView(dlg);
        ab->setPaperWhite(softProof_->getPaperWhiteL(),
                          softProof_->getPaperWhiteA(),
                          softProof_->getPaperWhiteB());
        dl->addWidget(ab);
        dlg->show();
    });
    mgmt->addWidget(abBtn);
    mgmt->addStretch();

    auto* mgmtWrap = new QWidget;
    auto* mgmtWrapLay = new QVBoxLayout(mgmtWrap);
    mgmtWrapLay->setContentsMargins(0, 0, 0, 0);
    mgmtWrapLay->addWidget(mgmtScroll);
    colorSubTabs_->addTab(mgmtWrap, TR("Mgmt"));

    // -- B&W --
    auto* bwScroll = new QScrollArea;
    bwScroll->setWidgetResizable(true);
    bwScroll->setFrameShape(QFrame::NoFrame);
    bwWidget_ = new BWModeWidget;
    bwScroll->setWidget(bwWidget_);
    connect(bwWidget_, &BWModeWidget::settingsChanged, this, [this] {
        if (!blockSignals_) emit jobModified();
    });
    colorSubTabs_->addTab(bwScroll, TR("B&W"));

    // -- Curve --
    auto* curveScroll = new QScrollArea;
    curveScroll->setWidgetResizable(true);
    curveScroll->setFrameShape(QFrame::NoFrame);
    curveEditor_ = new InkCurveEditor;
    curveEditor_->setMinimumHeight(260);
    curveScroll->setWidget(curveEditor_);
    colorSubTabs_->addTab(curveScroll, TR("Curve"));

    lay->addWidget(colorSubTabs_);
    return page;
}

// ---- Print (merged Layout + Output) --------------------------------------
QWidget* JobInspector::buildPrintTab() {
    auto* page = new QWidget;
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(page);

    auto* outerWrap = new QWidget;
    auto* outerLay = new QVBoxLayout(outerWrap);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->addWidget(scroll);

    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(4);

    lay->addWidget(sectionLabel("Paper"));
    paperCombo_ = new QComboBox;
    paperCombo_->addItems({"A4", "A3", "A3+", "A2", "17×22", "24×36", "Custom"});
    paperCombo_->setCurrentText("A3");
    lay->addWidget(paperCombo_);

    auto* size = new QHBoxLayout;
    paperW_ = new QDoubleSpinBox; paperW_->setRange(50, 1500); paperW_->setSuffix(" mm");
    paperW_->setValue(297);
    paperH_ = new QDoubleSpinBox; paperH_->setRange(50, 3000); paperH_->setSuffix(" mm");
    paperH_->setValue(420);
    auto wlbl = new QLabel("W"); wlbl->setStyleSheet("color:#555555;");
    auto hlbl = new QLabel("H"); hlbl->setStyleSheet("color:#555555;");
    size->addWidget(wlbl); size->addWidget(paperW_, 1);
    size->addWidget(hlbl); size->addWidget(paperH_, 1);
    lay->addLayout(size);

    lay->addWidget(sectionLabel("Margins"));
    auto* m = new QGridLayout;
    m->setHorizontalSpacing(6); m->setVerticalSpacing(2);
    marginT_ = new QSpinBox; marginT_->setRange(0, 100); marginT_->setSuffix(" mm"); marginT_->setValue(10);
    marginR_ = new QSpinBox; marginR_->setRange(0, 100); marginR_->setSuffix(" mm"); marginR_->setValue(10);
    marginB_ = new QSpinBox; marginB_->setRange(0, 100); marginB_->setSuffix(" mm"); marginB_->setValue(10);
    marginL_ = new QSpinBox; marginL_->setRange(0, 100); marginL_->setSuffix(" mm"); marginL_->setValue(10);
    auto mLbl = [](const QString& t){ auto* l = new QLabel(t); l->setStyleSheet("color:#555555;"); return l; };
    m->addWidget(mLbl("Top"),    0, 0); m->addWidget(marginT_, 0, 1);
    m->addWidget(mLbl("Right"),  0, 2); m->addWidget(marginR_, 0, 3);
    m->addWidget(mLbl("Bottom"), 1, 0); m->addWidget(marginB_, 1, 1);
    m->addWidget(mLbl("Left"),   1, 2); m->addWidget(marginL_, 1, 3);
    lay->addLayout(m);

    lay->addWidget(sectionLabel("Position"));
    auto* pos = new QGridLayout;
    pos->setHorizontalSpacing(6); pos->setVerticalSpacing(2);
    objX_ = new QSpinBox; objX_->setRange(-9999, 9999); objX_->setSuffix(" mm");
    objY_ = new QSpinBox; objY_->setRange(-9999, 9999); objY_->setSuffix(" mm");
    objR_ = new QSpinBox; objR_->setRange(-360, 360);   objR_->setSuffix(" °");
    pos->addWidget(mLbl("X"),   0, 0); pos->addWidget(objX_, 0, 1);
    pos->addWidget(mLbl("Y"),   0, 2); pos->addWidget(objY_, 0, 3);
    pos->addWidget(mLbl("Rot"), 1, 0); pos->addWidget(objR_, 1, 1);
    lay->addLayout(pos);

    lay->addWidget(sectionLabel("Printer"));
    printerCombo_ = new QComboBox;
    printerCombo_->addItems({
        "Epson SureColor P9500",
        "Epson SureColor P7500",
        "Epson SureColor P900",
        "Epson SureColor P700"
    });
    lay->addWidget(printerCombo_);

    auto* copiesRow = new QHBoxLayout;
    copiesSpin_ = new QSpinBox;
    copiesSpin_->setRange(1, 999);
    copiesSpin_->setValue(1);
    copiesRow->addWidget(mLbl("Copies"));
    copiesRow->addWidget(copiesSpin_);
    copiesRow->addStretch();
    lay->addLayout(copiesRow);

    auto* btnRow = new QHBoxLayout;
    auto* nestBtn   = new QPushButton("Nesting…");
    auto* testBtn   = new QPushButton("Test print…");
    auto* stepBtn   = new QPushButton("Step && Repeat…");
    auto* queueBtn  = new QPushButton("Print Queue…");
    btnRow->addWidget(nestBtn);
    btnRow->addWidget(testBtn);
    auto* btnRow2 = new QHBoxLayout;
    btnRow2->addWidget(stepBtn);
    btnRow2->addWidget(queueBtn);
    lay->addSpacing(6);
    lay->addLayout(btnRow);
    lay->addLayout(btnRow2);

    connect(nestBtn, &QPushButton::clicked, this, [this] {
        auto* w = new NestingWidget;
        w->setPaperSize(paperW_->value(), paperH_->value());
        w->setMargins(marginT_->value(), marginR_->value(),
                      marginB_->value(), marginL_->value());
        wrapInDialog(w, TR("Nesting"), this)->show();
    });
    connect(testBtn, &QPushButton::clicked, this, [this] {
        auto* d = new TestPrintDialog(this);
        d->setAttribute(Qt::WA_DeleteOnClose);
        d->show();
    });
    connect(stepBtn, &QPushButton::clicked, this, [this] {
        auto* d = new StepRepeatDialog(this);
        d->setAttribute(Qt::WA_DeleteOnClose);
        d->show();
    });
    connect(queueBtn, &QPushButton::clicked, this, [this] {
        auto* w = new PrintQueueWidget;
        wrapInDialog(w, TR("Print Queue"), this)->show();
    });

    lay->addStretch();
    return outerWrap;
}

// ---- Finishing -----------------------------------------------------------
QWidget* JobInspector::buildFinishingTab() {
    auto* page = new QWidget;
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(6);

    lay->addWidget(sectionLabel("Edition"));
    auto* editionBtn = new QPushButton("Open edition manager…");
    lay->addWidget(editionBtn);

    lay->addWidget(sectionLabel("Contact sheet"));
    auto* contactBtn = new QPushButton("Generate contact sheet…");
    lay->addWidget(contactBtn);

    lay->addWidget(sectionLabel("Spot color"));
    auto* spotBtn = new QPushButton("Honeycomb spot color…");
    lay->addWidget(spotBtn);

    connect(editionBtn, &QPushButton::clicked, this, [this] {
        auto* w = new EditionTrackingDialog;
        wrapInDialog(w, TR("Edition Manager"), this)->show();
    });
    connect(contactBtn, &QPushButton::clicked, this, [this] {
        auto* w = new ContactSheetDialog;
        wrapInDialog(w, TR("Contact Sheet"), this)->show();
    });
    connect(spotBtn, &QPushButton::clicked, this, [this] {
        auto* d = new HoneycombSpotDialog(this);
        d->setAttribute(Qt::WA_DeleteOnClose);
        d->show();
    });

    lay->addStretch();
    return page;
}

// ---- System --------------------------------------------------------------
QWidget* JobInspector::buildSystemTab() {
    auto* page = new QWidget;
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(6);

    lay->addWidget(sectionLabel("Calibration"));
    auto* calBtn = new QPushButton("G7 calibration wizard…");
    auto* monBtn = new QPushButton("Monitor calibration check…");
    lay->addWidget(calBtn);
    lay->addWidget(monBtn);

    lay->addWidget(sectionLabel("Inks"));
    auto* inkBtn     = new QPushButton("Manage ink cartridges…");
    auto* inkLotBtn  = new QPushButton("Ink lot tracker…");
    lay->addWidget(inkBtn);
    lay->addWidget(inkLotBtn);

    lay->addWidget(sectionLabel("Profiles"));
    auto* profBtn   = new QPushButton("Open profile library…");
    auto* importBtn = new QPushButton("Import ICC profile…");
    lay->addWidget(profBtn);
    lay->addWidget(importBtn);

    lay->addWidget(sectionLabel("Subscription"));
    auto* statusLabel = new QLabel;
    {
        auto& sk = jcolour::store::StoreKitManager::instance();
        auto info = sk.getSubscriptionStatus();
        statusLabel->setText(QString("%1 · %2").arg(tierName(info.tier), statusName(info.status)));
        statusLabel->setObjectName(
            info.status == jcolour::store::SubscriptionStatus::Active ? "badgeOk" :
            info.status == jcolour::store::SubscriptionStatus::Expired ? "badgeError" : "badgeWarn");
    }
    lay->addWidget(statusLabel);
    auto* subBtn = new QPushButton("Manage subscription…");
    lay->addWidget(subBtn);

    connect(calBtn, &QPushButton::clicked, this, [this] {
        auto* d = new G7CalibrationWizard(this);
        d->setAttribute(Qt::WA_DeleteOnClose);
        d->show();
    });
    connect(monBtn, &QPushButton::clicked, this, [this] {
        auto& mon = jcolour::monitor::MonitorCalibrationMonitor::instance();
        auto info = mon.checkCalibration();
        QString text;
        text += QString("Status: %1\n").arg(monitorStatusName(info.status));
        text += QString("Profile: %1\n").arg(info.profileName.isEmpty() ? "—" : info.profileName);
        text += QString("Device: %1\n").arg(info.deviceInfo.isEmpty() ? "—" : info.deviceInfo);
        if (info.lastCalibration.isValid()) {
            text += QString("Last calibrated: %1 (%2 days ago)\n")
                .arg(info.lastCalibration.toString(Qt::ISODate))
                .arg(info.daysSinceCalibration);
        }
        if (info.maxLuminance > 0) {
            text += QString("Max luminance: %1 cd/m²\n").arg(info.maxLuminance, 0, 'f', 1);
        }
        if (info.whitePointTemp > 0) {
            text += QString("White point: %1 K\n").arg(int(info.whitePointTemp));
        }
        QMessageBox::information(this, TR("Monitor Calibration"), text);
    });
    connect(inkBtn, &QPushButton::clicked, this, [this] {
        auto* w = new InkCartridgeTracker;
        wrapInDialog(w, TR("Ink Cartridges"), this)->show();
    });
    connect(inkLotBtn, &QPushButton::clicked, this, [this] {
        auto& tracker = jcolour::InkLotTracker::instance();
        auto lots = tracker.getAllLots();
        auto expiring = tracker.getExpiringLots(30);
        auto alerts = tracker.getReorderAlerts();
        QString text;
        text += QString("Total lots: %1\n").arg(lots.size());
        text += QString("Expiring within 30 days: %1\n").arg(expiring.size());
        text += QString("Reorder alerts: %1\n").arg(alerts.size());
        if (!alerts.isEmpty()) {
            text += "\n" + alerts.join("\n");
        }
        QMessageBox::information(this, TR("Ink Lot Tracker"), text);
    });
    connect(profBtn, &QPushButton::clicked, this, [this] {
        auto* w = new ProfileLibraryBrowser;
        wrapInDialog(w, TR("Profile Library"), this)->show();
    });
    connect(importBtn, &QPushButton::clicked, this, [this] {
        auto* d = new ProfileImportDialog(this);
        d->setAttribute(Qt::WA_DeleteOnClose);
        d->show();
    });
    connect(subBtn, &QPushButton::clicked, this, [this, statusLabel] {
        auto& sk = jcolour::store::StoreKitManager::instance();
        auto info = sk.getSubscriptionStatus();
        auto ent  = sk.getEntitlements();
        QString text;
        text += QString("Tier: %1\n").arg(tierName(info.tier));
        text += QString("Status: %1\n").arg(statusName(info.status));
        if (info.expirationDate.isValid()) {
            text += QString("Expires: %1\n").arg(info.expirationDate.toString(Qt::ISODate));
        }
        text += QString("Auto-renewing: %1\n").arg(info.isAutoRenewing ? "yes" : "no");
        text += "\nEntitlements:\n";
        text += QString("  Print: %1\n").arg(ent.canPrint ? "✓" : "—");
        text += QString("  B&W single black: %1\n").arg(ent.canUseBWSingleBlack ? "✓" : "—");
        text += QString("  B&W multi-channel: %1\n").arg(ent.canUseBWMultiChannel ? "✓" : "—");
        text += QString("  Spot color: %1\n").arg(ent.canUseSpotColor ? "✓" : "—");
        text += QString("  Nesting: %1\n").arg(ent.canUseNesting ? "✓" : "—");
        text += QString("  Contact sheet: %1\n").arg(ent.canUseContactSheet ? "✓" : "—");
        text += QString("  G7 validation: %1\n").arg(ent.canUseG7Validation ? "✓" : "—");
        text += QString("  Max queue: %1\n").arg(ent.maxImagesInQueue);
        QMessageBox::information(this, TR("Subscription"), text);
        // Refresh badge text from current data
        statusLabel->setText(QString("%1 · %2").arg(tierName(info.tier), statusName(info.status)));
    });

    lay->addStretch();
    return page;
}

// ---- setJob / buildJob ---------------------------------------------------
void JobInspector::clear() {
    Job empty;
    setJob(empty);
    imagePathLabel_->setText("(no image loaded)");
    imageInfoLabel_->setText("—");
}

void JobInspector::setJob(const Job& j) {
    blockSignals_ = true;
    if (!j.imagePath.isEmpty()) {
        QFileInfo fi(j.imagePath);
        imagePathLabel_->setText(fi.fileName());
        imageInfoLabel_->setText(QString("%1 · %2 KB")
            .arg(fi.suffix().toUpper())
            .arg(fi.size() / 1024));
    }
    paperW_->setValue(j.paperWidthMm);
    paperH_->setValue(j.paperHeightMm);
    paperCombo_->setCurrentText(j.paperName);
    marginT_->setValue(int(j.marginsMm[0]));
    marginR_->setValue(int(j.marginsMm[1]));
    marginB_->setValue(int(j.marginsMm[2]));
    marginL_->setValue(int(j.marginsMm[3]));
    intentCombo_->setCurrentIndex(j.renderingIntent);
    bpcCheck_->setChecked(j.blackPointCompensation);
    slC_->setValue(j.corrC); slM_->setValue(j.corrM);
    slY_->setValue(j.corrY); slK_->setValue(j.corrK);
    slR_->setValue(j.corrR); slG_->setValue(j.corrG);
    slB_->setValue(j.corrB);
    slBrightness_->setValue(j.brightness);
    slContrast_->setValue(j.contrast);
    slSaturation_->setValue(j.saturation);
    slSharpen_->setValue(j.sharpen);
    printerCombo_->setCurrentText(j.printerName);
    copiesSpin_->setValue(j.copies);
    blockSignals_ = false;
}

Job JobInspector::buildJob() const {
    Job j;
    j.paperName     = paperCombo_->currentText();
    j.paperWidthMm  = paperW_->value();
    j.paperHeightMm = paperH_->value();
    j.marginsMm[0] = marginT_->value();
    j.marginsMm[1] = marginR_->value();
    j.marginsMm[2] = marginB_->value();
    j.marginsMm[3] = marginL_->value();
    j.renderingIntent = intentCombo_->currentIndex();
    j.blackPointCompensation = bpcCheck_->isChecked();
    j.corrC = slC_->value(); j.corrM = slM_->value();
    j.corrY = slY_->value(); j.corrK = slK_->value();
    j.corrR = slR_->value(); j.corrG = slG_->value();
    j.corrB = slB_->value();
    j.brightness = slBrightness_->value();
    j.contrast   = slContrast_->value();
    j.saturation = slSaturation_->value();
    j.sharpen    = slSharpen_->value();
    j.printerName = printerCombo_->currentText();
    j.copies      = copiesSpin_->value();
    return j;
}

} // namespace jcolour
