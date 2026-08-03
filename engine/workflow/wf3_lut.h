#pragma once

#include <vector>
#include <string>
#include <array>
#include <map>
#include "wf3_chart.h"
#include "wf3_measure.h"

namespace jcolour {
namespace workflow {

// Workflow 3 LUT: maps L* + tint direction to 7-channel ink values
// Each tone preset has its own LUT

constexpr int WF3_LUT_SIZE = 256;  // L* levels

// LUT entry: 7 ink densities for a given L* and tint
struct Wf3LUTEntry {
    std::array<float, WF3_NUM_CHANNELS> inks;  // PK, MK, GY, LGY, C, M, Y
    float metamerism_score = 0.0f;  // Lower is better
};

// Full LUT for one preset
struct Wf3LUT {
    TonePreset preset = TonePreset::NEUTRAL;
    std::vector<Wf3LUTEntry> entries;  // 256 entries for L* = 0..100
};

// LUT map: preset name -> LUT
using Wf3PresetLUTMap = std::map<TonePreset, Wf3LUT>;

// Configuration for split-tone optimization
struct Wf3SplitConfig {
    // Weight for each optimization criterion
    float metamerism_weight = 0.4f;    // Anti-metamerism (multi-light source)
    float grain_weight = 0.2f;         // Grain visibility
    float ink_limit_weight = 0.2f;      // Stay within ink limits
    float smoothness_weight = 0.2f;     // Smooth tone transitions

    // Tint direction targets
    float target_a = 0.0f;  // Red-green (warm=positive, cool=negative)
    float target_b = 0.0f;   // Yellow-blue (warm=positive, cool=negative)

    // Ink limits per channel
    std::array<float, WF3_NUM_CHANNELS> ink_limits;
};

// Generate LUT for a given preset using split-tone optimization
Wf3LUT generate_wf3_lut(
    const WF3KMModel& km_model,
    const WF3InkLimits& ink_limits,
    const Wf3SplitConfig& config,
    TonePreset preset);

// Generate LUTs for all presets
Wf3PresetLUTMap generate_all_wf3_presets(
    const WF3KMModel& km_model,
    const WF3InkLimits& ink_limits);

// Lookup ink values from LUT
std::array<float, WF3_NUM_CHANNELS> wf3_lut_lookup(
    const Wf3LUT& lut,
    float l_star,  // 0..100
    float tint_strength = 1.0f);  // 0..1

// Enforce monotonicity on LUT entries
void enforce_wf3_monotonicity(Wf3LUT& lut);

// Apply smoothness constraint to adjacent LUT entries
void smooth_wf3_lut(Wf3LUT& lut, float strength);

// Validate LUT quality
struct Wf3LUTValidation {
    bool is_valid = false;
    float max_delta_e = 0.0f;
    float avg_delta_e = 0.0f;
    int out_of_range_count = 0;
};

Wf3LUTValidation validate_wf3_lut(
    const Wf3LUT& lut,
    const WF3KMModel& km_model);

}  // namespace workflow
}  // namespace jcolour
