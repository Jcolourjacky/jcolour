#include "wf1_ink_limit.h"
#include <algorithm>
#include <stdexcept>

namespace jcolour {
namespace workflow {

InkLimitResult find_ink_limit(const ScanResult& measurements, float safety_margin) {
    InkLimitResult result;

    if (!validate_measurements_for_ink_limit(measurements)) {
        result.success = false;
        result.error_message = "Measurements not valid for ink limit analysis";
        return result;
    }

    const auto& patches = measurements.patches;
    if (patches.size() < 3) {
        result.success = false;
        result.error_message = "Need at least 3 measurement points";
        return result;
    }

    // Compute derivatives: dL*/d(ink)
    // We expect L* to decrease as ink increases (more ink = darker)
    // At saturation point, L* stops decreasing significantly
    std::vector<float> derivatives;
    std::vector<size_t> indices;  // Keep track of patch indices

    for (size_t i = 1; i < patches.size(); ++i) {
        float ink_delta = patches[i].ink_density - patches[i-1].ink_density;
        if (ink_delta > 0) {
            float dL = patches[i].measured_lstar - patches[i-1].measured_lstar;
            float derivative = dL / ink_delta;  // Should be negative
            derivatives.push_back(derivative);
            indices.push_back(i);
        }
    }

    if (derivatives.empty()) {
        result.success = false;
        result.error_message = "Could not compute derivatives";
        return result;
    }

    // Find saturation: where derivative becomes close to zero (|dL/dInk| < threshold)
    // Saturation means L* stops decreasing significantly
    const float saturation_threshold = -2.0f;  // |derivative| < 2 means near saturation

    size_t saturation_idx = derivatives.size();
    for (size_t i = 0; i < derivatives.size(); ++i) {
        if (derivatives[i] > saturation_threshold) {  // derivative is close to 0 or positive
            saturation_idx = i + 1;  // +1 because derivatives[i] is between patch[i] and patch[i+1]
            break;
        }
    }

    // Clamp saturation point to valid range
    saturation_idx = std::min(saturation_idx, patches.size() - 1);

    // ink_limit = saturation_point - safety_margin
    float saturation_density = patches[saturation_idx].ink_density;
    result.ink_limit = std::max(0.5f, saturation_density - safety_margin);
    result.saturation_point = saturation_density;

    // Record max density (L* at saturation point)
    result.max_density = patches[saturation_idx].measured_lstar;
    result.success = true;

    return result;
}

bool validate_measurements_for_ink_limit(const ScanResult& measurements) {
    if (!measurements.success) return false;
    if (measurements.patches.size() < 5) return false;

    // Check basic monotonicity (L* should decrease with more ink)
    int violations = 0;
    for (size_t i = 1; i < measurements.patches.size() && violations < 3; ++i) {
        if (measurements.patches[i].measured_lstar > measurements.patches[i-1].measured_lstar + 2.0f) {
            ++violations;
        }
    }
    return violations < 3;  // Allow up to 2 violations (noise)
}

void export_chart_with_logo(const ChartLayout& chart,
                            const std::string& logo_path,
                            const std::string& output_path) {
    // This would require Qt or image library to render PNG
    // For now, this is a placeholder that validates the inputs
    if (chart.patches.empty()) {
        throw std::invalid_argument("Cannot export empty chart");
    }
    if (logo_path.empty()) {
        throw std::invalid_argument("Logo path is required");
    }
    // Actual PNG generation would go here with Qt or libtiff
    (void)output_path;  // Placeholder
}

}  // namespace workflow
}  // namespace jcolour