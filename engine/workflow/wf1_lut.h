#pragma once

#include <vector>
#include <array>
#include "wf1_measure.h"
#include "wf1_ink_limit.h"

namespace jcolour {
namespace workflow {

// 1D LUT for workflow 1: maps L* (0-100) to ink percentage (0-1)
// Uses PCHIP (Piecewise Cubic Hermite Interpolating Polynomial) for monotonicity
class LStarToInkLUT {
public:
    // Build LUT from measurement data
    // x = measured_lstar, y = ink_density
    explicit LStarToInkLUT(const std::vector<PatchMeasurement>& measurements);

    // Look up ink density for a given L* target
    float lookup(float target_lstar) const;

    // Full 256-point LUT for embedding in ParamSet
    std::array<float, 256> to_array() const;

    // Verify monotonicity (ink should increase as L* decreases)
    bool is_monotonic() const;

private:
    std::vector<float> lstar_values_;
    std::vector<float> ink_values_;
};

// Build L*→ink% LUT from scan + ink_limit results
LStarToInkLUT build_lut(const ScanResult& measurements,
                        const InkLimitResult& ink_limit,
                        float max_ink = 1.0f);

// Validate LUT quality: check monotonicity and smoothness
struct LUTValidation {
    bool monotonic = false;
    bool smooth = false;
    float max_derivative = 0.0f;
    std::string error_message;
};
LUTValidation validate_lut(const LStarToInkLUT& lut);

}  // namespace workflow
}  // namespace jcolour