#include "wf1_lut.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace jcolour {
namespace workflow {

// PCHIP: Piecewise Cubic Hermite Interpolating Polynomial
// Ensures monotonicity by using slopes that don't change sign

namespace {

// Compute PCHIP slopes (monotonicity-preserving)
std::vector<float> pchip_slopes(const std::vector<float>& x,
                                 const std::vector<float>& y) {
    size_t n = x.size();
    std::vector<float> h(n - 1);
    std::vector<float> delta(n - 1);

    for (size_t i = 0; i < n - 1; ++i) {
        h[i] = x[i + 1] - x[i];
        delta[i] = (y[i + 1] - y[i]) / h[i];
    }

    std::vector<float> d(n);
    d[0] = delta[0];
    d[n - 1] = delta[n - 2];

    for (size_t i = 1; i < n - 1; ++i) {
        if (delta[i - 1] * delta[i] > 0) {
            // Same sign: use weighted harmonic mean
            d[i] = 3.0f * h[i - 1] * h[i] * (delta[i - 1] + delta[i]) /
                   (2.0f * h[i - 1] * h[i] + h[i] * delta[i - 1] + h[i - 1] * delta[i]);
        } else {
            // Different sign or zero: set to 0
            d[i] = 0.0f;
        }
    }

    return d;
}

// Evaluate PCHIP at a given x value
float pchip_eval(const std::vector<float>& x,
                  const std::vector<float>& y,
                  const std::vector<float>& d,
                  float x_val) {
    size_t n = x.size();

    // Find interval
    size_t i = 0;
    if (x_val <= x[0]) {
        i = 0;
    } else if (x_val >= x[n - 1]) {
        i = n - 2;
    } else {
        while (i < n - 1 && x[i + 1] < x_val) ++i;
    }

    float h = x[i + 1] - x[i];
    float t = (x_val - x[i]) / h;
    float t2 = t * t;
    float t3 = t2 * t;

    // Hermite basis functions
    float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    float h10 = t3 - 2.0f * t2 + t;
    float h01 = -2.0f * t3 + 3.0f * t2;
    float h11 = t3 - t2;

    return h00 * y[i] + h10 * h * d[i] + h01 * y[i + 1] + h11 * h * d[i + 1];
}

}  // anonymous namespace

LStarToInkLUT::LStarToInkLUT(const std::vector<PatchMeasurement>& measurements) {
    if (measurements.size() < 2) {
        throw std::invalid_argument("Need at least 2 measurements for LUT");
    }

    // Extract x (L*) and y (ink) - sort by L* ascending (ascending for PCHIP)
    std::vector<std::pair<float, float>> sorted;
    for (const auto& m : measurements) {
        sorted.emplace_back(m.measured_lstar, m.ink_density);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });  // Ascending

    for (const auto& p : sorted) {
        lstar_values_.push_back(p.first);
        ink_values_.push_back(p.second);
    }
}

float LStarToInkLUT::lookup(float target_lstar) const {
    if (lstar_values_.empty()) return 0.0f;

    if (target_lstar <= lstar_values_.front()) {
        return ink_values_.front();
    }
    if (target_lstar >= lstar_values_.back()) {
        return ink_values_.back();
    }

    // Build slopes and interpolate
    auto slopes = pchip_slopes(lstar_values_, ink_values_);
    return pchip_eval(lstar_values_, ink_values_, slopes, target_lstar);
}

std::array<float, 256> LStarToInkLUT::to_array() const {
    std::array<float, 256> arr;
    for (int i = 0; i < 256; ++i) {
        float lstar = static_cast<float>(i) * 100.0f / 255.0f;  // L* from 0 to ~100
        arr[i] = lookup(lstar);
    }
    return arr;
}

bool LStarToInkLUT::is_monotonic() const {
    if (ink_values_.size() < 2) return true;

    // Check that ink values change in one direction consistently
    bool increasing = true;
    bool decreasing = true;

    for (size_t i = 1; i < ink_values_.size(); ++i) {
        if (ink_values_[i] <= ink_values_[i - 1] + 0.001f) {
            increasing = false;
        }
        if (ink_values_[i] >= ink_values_[i - 1] - 0.001f) {
            decreasing = false;
        }
    }

    return increasing || decreasing;  // Monotonic if consistently going one direction
}

LStarToInkLUT build_lut(const ScanResult& measurements,
                        const InkLimitResult& ink_limit,
                        float max_ink) {
    if (!measurements.success || measurements.patches.empty()) {
        throw std::invalid_argument("Invalid measurements");
    }

    // Filter measurements to respect ink limit
    std::vector<PatchMeasurement> filtered;
    for (const auto& m : measurements.patches) {
        if (m.ink_density <= ink_limit.ink_limit) {
            filtered.push_back(m);
        }
    }

    if (filtered.size() < 2) {
        throw std::invalid_argument("Not enough valid measurements after filtering");
    }

    return LStarToInkLUT(filtered);
}

LUTValidation validate_lut(const LStarToInkLUT& lut) {
    LUTValidation result;

    result.monotonic = lut.is_monotonic();
    if (!result.monotonic) {
        result.error_message = "LUT is not monotonic";
        return result;
    }

    // Check smoothness by examining derivatives
    float prev_val = lut.lookup(0.0f);
    float max_delta = 0.0f;
    for (int i = 1; i <= 100; ++i) {
        float lstar = static_cast<float>(i);
        float val = lut.lookup(lstar);
        float delta = std::abs(val - prev_val);
        max_delta = std::max(max_delta, delta);
        prev_val = val;
    }

    result.max_derivative = max_delta;
    result.smooth = max_delta < 0.05f;  // Max change per L* unit
    if (!result.smooth) {
        result.error_message = "LUT has discontinuities";
    }

    return result;
}

}  // namespace workflow
}  // namespace jcolour