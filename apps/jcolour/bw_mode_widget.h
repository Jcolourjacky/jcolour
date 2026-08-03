#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QRadioButton>
#include <QComboBox>
#include <QSlider>
#include <QLabel>

namespace jcolour {
namespace ui {

// 多色黑 split-tone presets (maps to per-zone CIELAB a*/b* tint below).
enum class TonePreset {
    Neutral = 0,        // pure neutral
    Selenium,           // cool purple shadows
    Sepia,              // warm brown throughout
    SplitCoolHighlight, // cool highlights / warm shadows
    SplitWarmHighlight, // warm highlights / cool shadows
    Custom              // user-dragged
};

struct ZoneTint { float a = 0.f; float b = 0.f; };  // -1..1, unit disc

// Fill h/m/s with the tint for a built-in preset. No-op for Custom.
void presetTints(TonePreset p, ZoneTint& highlight, ZoneTint& midtone, ZoneTint& shadow);

struct BWModeSettings {
    enum Mode {
        SingleBlack = 0,
        MultiChannel = 1
    } mode;

    // Single Black
    enum InkType { PhotoBlack, MatteBlack } inkType;
    int toneAdjustment; // -50 (cool) to +50 (warm)
    int halftoneAlgorithm; // 0=Stochastic, 1=Error Diffusion

    // Multi-channel (legacy flat warmth; kept for serialization compat)
    int highlightTone; // -100 (cool) to +100 (warm)
    int midtoneTone;
    int shadowTone;
    int globalNeutrality; // 0 (strict neutral) to 100 (allow shift)

    // Multi-channel split-tone (三段色環): per-zone CIELAB a*/b* tint, -1..1
    TonePreset tonePreset = TonePreset::Neutral;
    ZoneTint   highlightTint{};
    ZoneTint   midtoneTint{};
    ZoneTint   shadowTint{};

    bool isSingleBlack() const { return mode == SingleBlack; }
};

}
}

namespace jcolour { namespace ui { class SplitToneWheel; class BlackLinearizationPanel; } }

class BWModeWidget : public QWidget {
    Q_OBJECT
public:
    explicit BWModeWidget(QWidget* parent = nullptr);
    ~BWModeWidget() override;

    const jcolour::ui::BWModeSettings& settings() const { return settings_; }

    // Per-ink 線性化 (4 黑通道 PK/MK/GY/LGY) — exposed for engine LUT wiring.
    jcolour::ui::BlackLinearizationPanel* blackLinearization() const { return blackLin_; }

signals:
    void settingsChanged();

private slots:
    void onModeChanged(int index);
    void onInkTypeChanged(int index);
    void onToneChanged(int value);
    void onHalftoneChanged(int index);
    void onGlobalNeutralityChanged(int value);

private:
    void setupSingleBlackUI();
    void setupMultiChannelUI();
    void applyPreset(jcolour::ui::TonePreset p);   // push preset tints into the wheel + settings
    void onZoneChanged(int zone, float a, float b); // wheel drag → settings (preset → Custom)

    jcolour::ui::BWModeSettings settings_;
    QStackedWidget* stack_;

    // Single Black controls
    QComboBox* inkTypeCombo_;
    QSlider* toneSlider_;
    QComboBox* halftoneCombo_;

    // Multi-channel controls (三段色環)
    QComboBox*                presetCombo_ = nullptr;
    jcolour::ui::SplitToneWheel* toneWheel_ = nullptr;
    QLabel*                   zoneReadout_ = nullptr;
    jcolour::ui::BlackLinearizationPanel* blackLin_ = nullptr;
    QSlider*                  globalNeutralitySlider_ = nullptr;
};
