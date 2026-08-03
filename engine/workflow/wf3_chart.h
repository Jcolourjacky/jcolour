#pragma once

#include <vector>
#include <string>
#include "wf1_param.h"  // BlackKind (PK / MK)

namespace jcolour {
namespace workflow {

// Workflow 3: B&W + microcolor split-tone (6 active channels).
//
// PRO12 hardware loads 12 inks but PK and MK are mutually exclusive within a
// single print job. wf3 therefore models MAIN_K as a logical channel whose
// physical ink (PK or MK) is selected by Wf3ChartLayout::black_kind at
// session start. CMY pulled from PRO12 are Cyan + Vivid Magenta + Yellow
// (the Vivid Light Magenta and LC channels are not used by wf3's tint
// budget; that level of chromatic detail is wf4/wf6 territory).
//
// See engine/workflow/README.md → "黑墨互斥规则" for the rationale.
enum class Wf3Channel : int {
    MAIN_K = 0,  // Main black (PK or MK; selected by session black_kind)
    GY     = 1,  // Gray
    LGY    = 2,  // Light Gray
    C      = 3,  // Cyan
    VM     = 4,  // Vivid Magenta
    Y      = 5   // Yellow
};
constexpr int WF3_NUM_CHANNELS = 6;

// Single channel patch
struct Wf3Patch {
    std::string label;
    float ink_density = 0.0f;
    Wf3Channel channel = Wf3Channel::MAIN_K;
};

// Multi-channel patch (for Neugebauer / CYNSN sampling). Carries an
// arbitrary list of (channel, density) pairs that should be printed
// simultaneously on the same patch. F22 audit fix: cellular patches were
// previously degraded to a sequence of single-channel Wf3Patch entries
// with the same label, losing the multi-ink intent.
struct Wf3MultiPatch {
    std::string label;
    std::vector<std::pair<Wf3Channel, float>> channels;
};

// Layout for 6-channel calibration chart
struct Wf3ChartLayout {
    std::vector<Wf3Patch>      patches;        // single-ink patches
    std::vector<Wf3MultiPatch> multi_patches;  // Neugebauer / cellular combos
    BlackKind                  black_kind = BlackKind::PK;
    int                        columns = 8;
    int                        dpi = 1440;
    float                      patch_size_mm = 10.0f;
};

// Tone preset for split-tone effects
enum class TonePreset {
    NEUTRAL,   // Pure neutral
    WARM,      // Warm sepia-like
    COOL,      // Cool blue tone
    SPLIT      // Split tone
};

// Tint direction
struct TintDirection {
    float a_offset = 0.0f;  // Red-green shift
    float b_offset = 0.0f;  // Yellow-blue shift
    float strength = 0.0f;    // 0.0 = none, 1.0 = full
};

// Generate ink density ramp for 6 channels
Wf3ChartLayout generate_wf3_ink_ramp(BlackKind black_kind = BlackKind::PK);

// Generate cellular CYNSN test patches
Wf3ChartLayout generate_wf3_cellular_patches(BlackKind black_kind = BlackKind::PK);

// Generate verification chart
Wf3ChartLayout generate_wf3_verification(BlackKind black_kind = BlackKind::PK);

// Combined calibration chart
Wf3ChartLayout generate_wf3_calibration_chart(BlackKind black_kind = BlackKind::PK);

// generate_wf3_self_test_chart is declared in wf3_self_test.h (single source).

}  // namespace workflow
}  // namespace jcolour
