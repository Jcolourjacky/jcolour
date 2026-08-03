#include "bw_mode_widget.h"
#include "split_tone_wheel.h"
#include "black_linearization_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QBoxLayout>
#include <QSignalBlocker>

namespace jcolour {
namespace ui {

void presetTints(TonePreset p, ZoneTint& hl, ZoneTint& mid, ZoneTint& sh) {
    switch (p) {
        case TonePreset::Neutral:
            hl = {0.f, 0.f};        mid = {0.f, 0.f};         sh = {0.f, 0.f};         break;
        case TonePreset::Selenium:  // cool purple shadows
            hl = {0.f, 0.f};        mid = {0.04f, -0.06f};    sh = {0.14f, -0.34f};    break;
        case TonePreset::Sepia:     // warm brown throughout
            hl = {0.18f, 0.34f};    mid = {0.22f, 0.40f};     sh = {0.15f, 0.28f};     break;
        case TonePreset::SplitCoolHighlight:  // cool highlights / warm shadows
            hl = {-0.18f, -0.28f};  mid = {0.f, 0.f};         sh = {0.20f, 0.30f};     break;
        case TonePreset::SplitWarmHighlight:  // warm highlights / cool shadows
            hl = {0.20f, 0.30f};    mid = {0.f, 0.f};         sh = {-0.18f, -0.28f};   break;
        case TonePreset::Custom:
            break;  // leave caller's values untouched
    }
}

}  // namespace ui
}  // namespace jcolour

BWModeWidget::BWModeWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    // Mode selector
    auto* modeGroup = new QGroupBox("B&W Mode");
    auto* modeLayout = new QVBoxLayout(modeGroup);

    auto* modeSelector = new QHBoxLayout;
    auto* singleRadio = new QRadioButton("Single Black");
    auto* multiRadio = new QRadioButton("Multi-channel Black");
    multiRadio->setChecked(true);
    modeSelector->addWidget(singleRadio);
    modeSelector->addWidget(multiRadio);
    modeSelector->addStretch();
    modeLayout->addLayout(modeSelector);

    stack_ = new QStackedWidget;

    // Single Black page
    auto* singlePage = new QWidget;
    auto* singleLayout = new QVBoxLayout(singlePage);

    inkTypeCombo_ = new QComboBox;
    inkTypeCombo_->addItems({"Photo Black (PK)", "Matte Black (MK)"});
    auto* inkLayout = new QHBoxLayout;
    inkLayout->addWidget(new QLabel("Black Ink:"));
    inkLayout->addWidget(inkTypeCombo_, 1);
    singleLayout->addLayout(inkLayout);

    toneSlider_ = new QSlider(Qt::Horizontal);
    toneSlider_->setRange(-50, 50);
    toneSlider_->setValue(0);
    auto* toneLayout = new QHBoxLayout;
    toneLayout->addWidget(new QLabel("◐ Cool"));
    toneLayout->addWidget(toneSlider_);
    toneLayout->addWidget(new QLabel("Warm ◑"));
    singleLayout->addLayout(toneLayout);

    halftoneCombo_ = new QComboBox;
    halftoneCombo_->addItems({
        QStringLiteral("Stochastic（建議 · 自然底片感）"),
        QStringLiteral("Error Diffusion（銳利細節）")
    });
    auto* halftoneLayout = new QHBoxLayout;
    halftoneLayout->addWidget(new QLabel("Halftone:"));
    halftoneLayout->addWidget(halftoneCombo_, 1);
    singleLayout->addLayout(halftoneLayout);

    singleLayout->addStretch();
    stack_->addWidget(singlePage);

    // Multi-channel page — 6 presets + 三段色環 (split-tone wheel)
    auto* multiPage = new QWidget;
    auto* multiLayout = new QVBoxLayout(multiPage);

    // Preset selector
    presetCombo_ = new QComboBox;
    presetCombo_->addItem(QStringLiteral("中性 Neutral"),
                          static_cast<int>(jcolour::ui::TonePreset::Neutral));
    presetCombo_->addItem(QStringLiteral("硒調 Selenium"),
                          static_cast<int>(jcolour::ui::TonePreset::Selenium));
    presetCombo_->addItem(QStringLiteral("棕褐 Sepia"),
                          static_cast<int>(jcolour::ui::TonePreset::Sepia));
    presetCombo_->addItem(QStringLiteral("分離調 · 冷高光"),
                          static_cast<int>(jcolour::ui::TonePreset::SplitCoolHighlight));
    presetCombo_->addItem(QStringLiteral("分離調 · 暖高光"),
                          static_cast<int>(jcolour::ui::TonePreset::SplitWarmHighlight));
    presetCombo_->addItem(QStringLiteral("自訂 Custom"),
                          static_cast<int>(jcolour::ui::TonePreset::Custom));
    auto* presetLayout = new QHBoxLayout;
    presetLayout->addWidget(new QLabel(QStringLiteral("色調預設:")));
    presetLayout->addWidget(presetCombo_, 1);
    multiLayout->addLayout(presetLayout);

    // Let combos shrink to the inspector width (300px) instead of forcing their
    // longest item's width and clipping the panel.
    for (auto* cb : {inkTypeCombo_, halftoneCombo_, presetCombo_}) {
        cb->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        cb->setMinimumContentsLength(6);
        cb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    // 三段色環: H/M/S handles on a CIELAB a*/b* disc
    auto* wheelBox = new QGroupBox(QStringLiteral("三段色環 (拖曳 H 高光 · M 中間調 · S 陰影)"));
    auto* wheelLayout = new QVBoxLayout(wheelBox);
    toneWheel_ = new jcolour::ui::SplitToneWheel;
    wheelLayout->addWidget(toneWheel_, 0, Qt::AlignHCenter);
    zoneReadout_ = new QLabel(QStringLiteral("H 0.00, 0.00   M 0.00, 0.00   S 0.00, 0.00"));
    zoneReadout_->setAlignment(Qt::AlignHCenter);
    zoneReadout_->setStyleSheet("color:#868c95;font-size:11px;");
    wheelLayout->addWidget(zoneReadout_);
    multiLayout->addWidget(wheelBox);

    // Per-ink 線性化 (4 黑通道 PK/MK/GY/LGY)
    auto* linBox = new QGroupBox(QStringLiteral("Per-ink 線性化 (4 黑通道 PK / MK / GY / LGY)"));
    auto* linLayout = new QVBoxLayout(linBox);
    blackLin_ = new jcolour::ui::BlackLinearizationPanel;
    linLayout->addWidget(blackLin_);
    multiLayout->addWidget(linBox);

    // Global neutrality
    auto* globalGroup = new QGroupBox("Global Neutrality");
    auto* globalLayout = new QHBoxLayout(globalGroup);
    globalLayout->addWidget(new QLabel("Strict Neutral"));
    globalNeutralitySlider_ = new QSlider(Qt::Horizontal);
    globalNeutralitySlider_->setRange(0, 100);
    globalNeutralitySlider_->setValue(0);
    globalLayout->addWidget(globalNeutralitySlider_);
    globalLayout->addWidget(new QLabel("Allow Shift"));
    multiLayout->addWidget(globalGroup);

    multiLayout->addStretch();
    stack_->addWidget(multiPage);

    modeLayout->addWidget(stack_);
    layout->addWidget(modeGroup);

    // Connect signals
    connect(singleRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            stack_->setCurrentIndex(0);
            settings_.mode = jcolour::ui::BWModeSettings::SingleBlack;
            emit settingsChanged();
        }
    });
    connect(multiRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            stack_->setCurrentIndex(1);
            settings_.mode = jcolour::ui::BWModeSettings::MultiChannel;
            emit settingsChanged();
        }
    });

    connect(inkTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BWModeWidget::onInkTypeChanged);
    connect(toneSlider_, &QSlider::valueChanged, this, &BWModeWidget::onToneChanged);
    connect(halftoneCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BWModeWidget::onHalftoneChanged);
    connect(presetCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        applyPreset(static_cast<jcolour::ui::TonePreset>(presetCombo_->currentData().toInt()));
    });
    connect(toneWheel_, &jcolour::ui::SplitToneWheel::zoneChanged,
            this, &BWModeWidget::onZoneChanged);
    connect(blackLin_, &jcolour::ui::BlackLinearizationPanel::curvesChanged,
            this, &BWModeWidget::settingsChanged);
    connect(globalNeutralitySlider_, &QSlider::valueChanged, this, &BWModeWidget::onGlobalNeutralityChanged);

    applyPreset(jcolour::ui::TonePreset::Neutral);  // initial sync
}

BWModeWidget::~BWModeWidget() = default;

void BWModeWidget::onModeChanged(int index) {
    settings_.mode = static_cast<jcolour::ui::BWModeSettings::Mode>(index);
    emit settingsChanged();
}

void BWModeWidget::onInkTypeChanged(int index) {
    settings_.inkType = static_cast<jcolour::ui::BWModeSettings::InkType>(index);
    emit settingsChanged();
}

void BWModeWidget::onToneChanged(int value) {
    settings_.toneAdjustment = value;
    emit settingsChanged();
}

void BWModeWidget::onHalftoneChanged(int index) {
    settings_.halftoneAlgorithm = index;
    emit settingsChanged();
}

static void updateReadout(QLabel* lbl, const jcolour::ui::BWModeSettings& s) {
    if (!lbl) return;
    lbl->setText(QString("H %1, %2   M %3, %4   S %5, %6")
        .arg(s.highlightTint.a, 0, 'f', 2).arg(s.highlightTint.b, 0, 'f', 2)
        .arg(s.midtoneTint.a,   0, 'f', 2).arg(s.midtoneTint.b,   0, 'f', 2)
        .arg(s.shadowTint.a,    0, 'f', 2).arg(s.shadowTint.b,    0, 'f', 2));
}

void BWModeWidget::applyPreset(jcolour::ui::TonePreset p) {
    settings_.tonePreset = p;
    if (p != jcolour::ui::TonePreset::Custom) {
        jcolour::ui::presetTints(p, settings_.highlightTint,
                                 settings_.midtoneTint, settings_.shadowTint);
        // push into the wheel without re-emitting back into us
        QSignalBlocker block(toneWheel_);
        toneWheel_->setZone(jcolour::ui::SplitToneWheel::Highlight,
                            settings_.highlightTint.a, settings_.highlightTint.b);
        toneWheel_->setZone(jcolour::ui::SplitToneWheel::Midtone,
                            settings_.midtoneTint.a, settings_.midtoneTint.b);
        toneWheel_->setZone(jcolour::ui::SplitToneWheel::Shadow,
                            settings_.shadowTint.a, settings_.shadowTint.b);
    }
    // keep combo in sync (e.g. when called programmatically)
    int idx = presetCombo_->findData(static_cast<int>(p));
    if (idx >= 0 && idx != presetCombo_->currentIndex()) {
        QSignalBlocker block(presetCombo_);
        presetCombo_->setCurrentIndex(idx);
    }
    updateReadout(zoneReadout_, settings_);
    emit settingsChanged();
}

void BWModeWidget::onZoneChanged(int zone, float a, float b) {
    jcolour::ui::ZoneTint t{a, b};
    switch (zone) {
        case jcolour::ui::SplitToneWheel::Highlight: settings_.highlightTint = t; break;
        case jcolour::ui::SplitToneWheel::Midtone:   settings_.midtoneTint   = t; break;
        case jcolour::ui::SplitToneWheel::Shadow:    settings_.shadowTint    = t; break;
        default: return;
    }
    // dragging a handle means we've left a built-in preset
    if (settings_.tonePreset != jcolour::ui::TonePreset::Custom) {
        settings_.tonePreset = jcolour::ui::TonePreset::Custom;
        int idx = presetCombo_->findData(static_cast<int>(jcolour::ui::TonePreset::Custom));
        if (idx >= 0) { QSignalBlocker block(presetCombo_); presetCombo_->setCurrentIndex(idx); }
    }
    updateReadout(zoneReadout_, settings_);
    emit settingsChanged();
}

void BWModeWidget::onGlobalNeutralityChanged(int value) {
    settings_.globalNeutrality = value;
    emit settingsChanged();
}
