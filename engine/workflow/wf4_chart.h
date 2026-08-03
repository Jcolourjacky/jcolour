#pragma once

#include <vector>
#include <string>
#include "wf1_param.h"          // BlackKind (PK / MK)
#include "../src/channel_layout.h"

namespace jcolour {
namespace workflow {

// Workflow 4 — full-color PRO12 (11 active channels per print job).
//
// PRO12 hardware loads 12 inks but PK and MK are mutually exclusive within a
// single print, so wf4 lives in the 11-channel active space defined by
// channel_layout.h. The physical 12-slot layout is materialised only at the
// PJL boundary via jcolour::expand_to_physical(active, kind).
//
// InkChannel is an alias for the canonical ActiveChannel enum — every wf4
// call site uses InkChannel::MAIN_K (whichever black is loaded), never PK or
// MK directly. See engine/workflow/README.md for the full channel table.
using InkChannel = ActiveChannel;
constexpr int NUM_INK_CHANNELS = NUM_ACTIVE_CHANNELS;  // = 11

// Single channel patch
struct InkPatch {
    std::string label;
    float       ink_density = 0.0f;  // 0.0..1.0
    InkChannel  channel = InkChannel::MAIN_K;
};

// TIL combination patch (multiple channels at once)
struct TILPatch {
    std::string label;
    std::vector<std::pair<InkChannel, float>> channels;  // channel + density
};

// Layout for 11-channel calibration chart
struct WF4ChartLayout {
    std::vector<InkPatch>     single_ink_patches;  // 11 channels × 21 steps = 231
    std::vector<TILPatch>     til_patches;
    BlackKind                 black_kind = BlackKind::PK;
    int                       columns = 12;
    int                       dpi = 1440;
    float                     patch_size_mm = 10.0f;
};

// Generate ink density ramp for all 11 active channels.
// 21 steps per channel: 0%, 5%, 10%, ... 100%
WF4ChartLayout generate_wf4_ink_ramp(BlackKind black_kind = BlackKind::PK);

// Generate TIL (Total Ink Limit) test patches.
// Tests various ink combinations to find the maximum acceptable ink coverage.
WF4ChartLayout generate_wf4_til_patches(BlackKind black_kind = BlackKind::PK);

// Generate combined chart (single ink + TIL)
WF4ChartLayout generate_wf4_calibration_chart(BlackKind black_kind = BlackKind::PK);

// Generate verification chart for post-calibration validation
WF4ChartLayout generate_wf4_verification_chart(BlackKind black_kind = BlackKind::PK);

// Generate ECI2002-style extended target for ICC profile training
// Returns ~1500-2300 patches covering the full color gamut
WF4ChartLayout generate_wf4_extended_target(int target_patches = 1500,
                                             BlackKind black_kind = BlackKind::PK);

// Export chart as SVG for printing
void export_wf4_chart_svg(const WF4ChartLayout& chart, const std::string& path);

// Export chart layout info as JSON for measurement software
void export_wf4_chart_json(const WF4ChartLayout& chart, const std::string& path);

}  // namespace workflow
}  // namespace jcolour
