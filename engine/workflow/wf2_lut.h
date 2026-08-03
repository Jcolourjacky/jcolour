#pragma once

#include <vector>
#include <array>
#include <string>
#include <memory>
#include "wf2_measure.h"

namespace jcolour {
namespace workflow {

// LUT type: L* (0-100) -> ink coverage for 4 channels
// We use 256 points for smooth interpolation
constexpr int WF2_LUT_SIZE = 256;

using LStarToInkLUT = std::array<std::array<float, WF2_NUM_CHANNELS>, WF2_LUT_SIZE>;

// User preference for workflow 2 split
enum class SplitPreference {
    NEUTRAL,   // Maximum neutrality
    WARM,      // Warm tones
    COOL,      // Cool tones
    CREAMY     // Warm but with more texture
};

struct Wf2SplitConfig {
    float alpha_metamerism = 1.0f;   // Multi-source metamerism weight
    float beta_grain = 0.5f;        // Grain visibility weight
    float gamma_bronze = 0.3f;      // Bronzing suppression weight
    float delta_smooth = 0.2f;      // Smoothness weight
    float epsilon_ink = 0.1f;       // Total ink usage weight
    SplitPreference preference = SplitPreference::NEUTRAL;
};

// Workflow 2 LUT result
struct Wf2LUTResult {
    LStarToInkLUT lut{};
    float avg_delta_e = 0.0f;
    float max_delta_e = 0.0f;
    bool  valid = false;
};

// Fit Yule-Nielsen n factor to minimize prediction error
float fit_yule_nielsen_n_optimal(
    const Wf2KMModel& km_model,
    const std::vector<Wf2Measurement>& measurements,
    float initial_n = 2.0f);

// Generate split LUT using SQP optimization
Wf2LUTResult generate_wf2_split_lut(
    const Wf2KMModel& km_model,
    const Wf2AllInkLimits& ink_limits,
    const Wf2SplitConfig& config = {});

// Apply LUT to gray input
std::array<float, WF2_NUM_CHANNELS> apply_wf2_lut(float lstar, const LStarToInkLUT& lut);

// Validate LUT monotonicity
bool validate_wf2_lut_monotonic(const LStarToInkLUT& lut);

// Export LUT as JSON
void export_wf2_lut_json(const LStarToInkLUT& lut, const std::string& path);

}  // namespace workflow
}  // namespace jcolour