#pragma once

#include <vector>
#include <array>
#include <string>
#include <memory>

namespace jcolour {
namespace workflow {

// Forward declaration
struct Wf3ChartLayout;
enum class Wf3Channel : int;

// Per-patch measurement.
//
// `channel` and `ink_density` describe what was printed (for single-channel
// ramp patches) — fit_wf3_km_model uses these to extract per-channel
// reflectance curves at known concentrations. For multi-channel patches
// (Neugebauer combos, cellular CYNSN samples), `channel` is the dominant
// channel and `ink_density` is the dominant density; downstream code that
// needs the full composition should reference the chart's patches by
// `patch_index`.
struct WF3Measurement {
    int patch_index = 0;
    std::string label;
    std::array<float, 36> spectrum;  // 36-channel spectral reflectance (380–730nm @ 10nm)
    float L_star = 0.0f;
    float a_star = 0.0f;
    float b_star = 0.0f;
    // Audit F8 fix: carry the chart's channel + density so K-M fitting can
    // actually use measurement data instead of hardcoded constants.
    int channel = 0;
    float ink_density = 0.0f;
};

// Workflow 3 CYNSN model parameters.
//
// Single-constant Kubelka–Munk plus Yule–Nielsen, stored per wavelength so
// the model produces a real spectrum (not the flat scalar output the
// audit F7 surfaced). For each active channel we store K/S at full ink
// coverage, indexed by wavelength.
struct WF3CYNSNParams {
    float n = 2.0f;  // Yule-Nielsen factor (typical 1.5–4.0)

    // Per-channel, per-wavelength K/S at full (c = 1.0) ink coverage.
    // 6 active wf3 channels × 36 spectral bands.
    std::array<std::array<float, 36>, 6> km_ks_full{};

    // Spectral reflectance clamping bounds (keep numerics sane).
    float R_min = 0.001f;
    float R_max = 0.99f;
};

// Analyze ink limits from single-channel ramp measurements
struct WF3InkLimits {
    struct PerChannelLimit {
        int channel = 0;
        float max_density = 1.0f;
        float D_max = 0.0f;      // Maximum density
        float D_min = 0.0f;      // Minimum density (paper)
    };
    std::vector<PerChannelLimit> limits;
};

// Workflow 3 CYNSN Model
struct WF3KMModel {
    std::unique_ptr<WF3CYNSNParams> params;
    std::array<float, 36> paper_white{};
};

// Predict spectral reflectance for a given ink combination.
//
// Single-constant K–M with Yule–Nielsen:
//   K/S(λ) = K/S_paper(λ) + Σ_i c_i · (K/S_full[i](λ) − K/S_paper(λ))
//   R(λ)   = 1 + K/S(λ) − √(K/S(λ)² + 2·K/S(λ))
//   R(λ)   = clamp(R^n_adjusted, R_min, R_max)
std::array<float, 36> predict_wf3_spectrum(
    const std::array<float, 6>& ink_densities,
    const WF3KMModel& model);

// Fit K–M coefficients from single-channel ramp measurements.
// For each requested channel, picks the highest-density single-channel
// patch as the "full coverage" approximation and computes K/S(λ) from
// its measured reflectance. The paper white is taken from any zero-ink
// patch (or paper_white_override when provided).
WF3KMModel fit_wf3_km_model(
    const std::vector<WF3Measurement>& single_channel_measurements,
    const std::vector<int>& channel_indices,
    const std::array<float, 36>* paper_white_override = nullptr);

// Fit Yule–Nielsen n parameter by grid search minimising the actual
// spectral RMS residual between model predictions and measurements.
float fit_wf3_yule_nielsen_n(
    const WF3KMModel& model,
    const std::vector<WF3Measurement>& measurements);

// Analyze ink limits from ramp measurements
WF3InkLimits analyze_wf3_ink_limits(
    const std::vector<WF3Measurement>& ramp_measurements);

// Run full workflow 3 calibration
bool run_wf3_calibration(
    const Wf3ChartLayout& chart,
    const std::vector<WF3Measurement>& measurements,
    WF3KMModel& out_model,
    WF3InkLimits& out_limits);

// Simulate measurements for testing
std::vector<WF3Measurement> simulate_wf3_measurements(
    const Wf3ChartLayout& chart);

}
}
