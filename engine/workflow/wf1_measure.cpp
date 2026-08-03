#include "wf1_measure.h"
#include <random>
#include <algorithm>
#include <cmath>

namespace jcolour {
namespace workflow {

ScanResult scan_wf1_chart(const ChartLayout& chart) {
    ScanResult result;
    result.success = false;
    result.error_message = "i1Pro3 not connected - hardware stub";
    return result;
}

bool validate_monotonicity(const ScanResult& result, float tolerance) {
    if (result.patches.size() < 2) return true;

    bool has_decrease = false;  // Track if we've seen a decrease
    for (size_t i = 1; i < result.patches.size(); ++i) {
        float delta = result.patches[i].measured_lstar - result.patches[i-1].measured_lstar;

        // Check for non-monotonic increase (L* increases when it should decrease)
        if (delta > tolerance) {
            return false;  // L* increased too much - not monotonically decreasing
        }

        // Track if we've seen proper decrease
        if (delta < -tolerance) {
            has_decrease = true;
        }
    }
    return has_decrease;  // Must have at least one valid decrease
}

ScanResult simulate_wf1_measurement(const ChartLayout& chart, float noise_stddev) {
    ScanResult result;
    result.success = true;
    result.error_message = "simulated";

    std::mt19937 gen(42);  // Fixed seed for reproducibility
    std::normal_distribution<float> noise(0.0f, noise_stddev);

    for (const auto& patch : chart.patches) {
        PatchMeasurement m;
        m.label = patch.label;
        m.ink_density = patch.ink_density;  // Copy from original chart

        // Simple model: L* = 100 - 50 * ink_density + noise
        // At 0% ink: L* ≈ 100 (white)
        // At 100% ink: L* ≈ 50 (max black)
        m.measured_lstar = 100.0f - 50.0f * patch.ink_density + noise(gen);
        m.measured_lstar = std::clamp(m.measured_lstar, 0.0f, 100.0f);
        m.valid = true;

        result.patches.push_back(m);
    }

    return result;
}

}  // namespace workflow
}  // namespace jcolour