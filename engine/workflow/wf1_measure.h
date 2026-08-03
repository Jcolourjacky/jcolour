#pragma once

#include <vector>
#include "wf1_chart.h"

namespace jcolour {
namespace workflow {

// Measurement result for a single patch
struct PatchMeasurement {
    std::string label;
    float       ink_density = 0.0f;  // Original ink coverage (0.0..1.0)
    float       measured_lstar = 0.0f;
    bool        valid = true;
};

// Result of scanning a chart with i1Pro3
struct ScanResult {
    std::vector<PatchMeasurement> patches;
    std::string                   error_message;
    bool                          success = false;
};

// Scan ink ramp chart with i1Pro3.
// Returns ScanResult with L* values for each patch.
// This is a stub - actual implementation requires i1Pro3 SDK (B10).
[[maybe_unused]] ScanResult scan_wf1_chart([[maybe_unused]] const ChartLayout& chart);

// Validate that measured L* values are monotonically decreasing
// (more ink = darker = lower L*).
bool validate_monotonicity(const ScanResult& result, float tolerance = 0.5f);

// Simulate measurement for testing (no real hardware required).
// Uses a simple model: L* = 100 - 50 * ink_density + noise.
ScanResult simulate_wf1_measurement(const ChartLayout& chart, float noise_stddev = 0.3f);

}  // namespace workflow
}  // namespace jcolour