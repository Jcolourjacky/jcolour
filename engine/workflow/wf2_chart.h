#pragma once

#include <vector>
#include <string>
#include "wf1_param.h"  // BlackKind (PK / MK)

namespace jcolour {
namespace workflow {

// Multi-tone B&W workflow channels.
//
// PRO12 hardware loads both Photo Black and Matte Black, but for any single
// print only one is active (paper-determined). wf2 therefore models a logical
// MAIN_K channel whose physical ink is chosen via Wf2ChartLayout::black_kind
// (BlackKind::PK for photo papers, BlackKind::MK for matte papers).
//
// See engine/workflow/README.md → "黑墨互斥规则" for the rationale.
enum class Wf2Channel : int {
    MAIN_K = 0,  // Main black (PK or MK; selected by session black_kind)
    GY = 1,       // Gray
    LGY = 2       // Light Gray
};
constexpr int WF2_NUM_CHANNELS = 3;

// Single channel patch for workflow 2
struct Wf2Patch {
    std::string label;
    float       ink_density = 0.0f;  // 0.0..1.0
    Wf2Channel  channel = Wf2Channel::MAIN_K;
};

// Layout for 3-channel calibration chart
struct Wf2ChartLayout {
    std::vector<Wf2Patch> patches;
    BlackKind             black_kind = BlackKind::PK;  // chosen at session start
    int                   columns = 8;
    int                   dpi = 1440;
    float                 patch_size_mm = 10.0f;
};

// Neugebauer primary patches for K-M model building
struct Wf2NeugebauerPatches {
    std::vector<std::vector<Wf2Channel>> primaries;  // e.g., {MAIN_K}, {GY}, {MAIN_K,GY}, ...
};

// Generate ink density ramp for all 3 channels.
// 21 steps per channel: 0%, 5%, 10%, ... 100%
Wf2ChartLayout generate_wf2_ink_ramp(BlackKind black_kind = BlackKind::PK);

// Generate Neugebauer primary patches: 3 single + 3 double + 1 triple = 7
Wf2NeugebauerPatches generate_wf2_neugebauer_primaries();

// Generate verification chart (neutral patches + mixed tones)
Wf2ChartLayout generate_wf2_verification(BlackKind black_kind = BlackKind::PK);

// Combined calibration chart (ink ramp + some Neugebauer)
Wf2ChartLayout generate_wf2_calibration_chart(BlackKind black_kind = BlackKind::PK);

// Export chart info as JSON
void export_wf2_chart_json(const Wf2ChartLayout& chart, const std::string& path);

}  // namespace workflow
}  // namespace jcolour
