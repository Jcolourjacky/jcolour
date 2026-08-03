#pragma once

#include <vector>
#include "wf1_measure.h"

namespace jcolour {
namespace workflow {

// Result of ink limit finding
struct InkLimitResult {
    float ink_limit = 0.85f;   // Optimal ink limit (0.0..1.0)
    float max_density = 0.0f;  // L* at ink_limit
    float saturation_point = 0.95f;  // Where L* stops decreasing
    bool success = false;
    std::string error_message;
};

// Find the ink limit where L* stops decreasing (saturation point).
// Uses derivative analysis to detect when dL*/d(ink) ≈ 0.
// Returns InkLimitResult with optimal ink_limit.
//
// Algorithm:
// 1. Compute first derivative of L* vs ink
// 2. Find where derivative becomes ~0 (saturation)
// 3. Apply safety margin to stay below saturation
InkLimitResult find_ink_limit(const ScanResult& measurements, float safety_margin = 0.05f);

// Validate that measurements are suitable for ink limit finding.
// Returns false if data is too noisy or non-monotonic beyond tolerance.
bool validate_measurements_for_ink_limit(const ScanResult& measurements);

// Export chart with logo overlay as PNG
void export_chart_with_logo(const ChartLayout& chart,
                            const std::string& logo_path,
                            const std::string& output_path);

}  // namespace workflow
}  // namespace jcolour