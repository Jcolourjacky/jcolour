#pragma once

#include <vector>
#include <array>
#include <map>
#include <memory>
#include <string>
#include "wf2_chart.h"
#include "../src/km.h"

namespace jcolour {
namespace workflow {

// Measurement result for a single patch
struct Wf2Measurement {
    std::string label;
    Wf2Channel  channel;
    float       ink_density;
    double      L_star;
    double      a_star;
    double      b_star;
    std::array<double, 36> spectral_data;  // 36-channel spectrum
};

// Ink limit result for a single channel
struct Wf2InkLimit {
    Wf2Channel channel;
    float      limit;
    bool       is_monotonic;
};

// Combined ink limits for all 4 channels
struct Wf2AllInkLimits {
    std::vector<Wf2InkLimit> per_channel;
};

// K-M model for workflow 2
struct Wf2KMModel {
    std::unique_ptr<forward::KMForwardModel> km;  // Kubelka-Munk model
    float yule_nielsen_n = 2.0f;  // Default Y-N factor
};

// Simulate measurements for testing
std::vector<Wf2Measurement> simulate_wf2_measurements(const Wf2ChartLayout& chart);

// Analyze ink limit from channel measurements
Wf2InkLimit analyze_wf2_ink_limit(const std::vector<Wf2Measurement>& measurements);

// Analyze all 4 channels
Wf2AllInkLimits analyze_wf2_ink_limits(const Wf2ChartLayout& chart, bool dry_run);

// Build K-M model from Neugebauer primaries
Wf2KMModel fit_km_model(
    const std::vector<Wf2Measurement>& neugebauer_patches,
    const Wf2NeugebauerPatches& primaries,
    const std::array<double, 36>& paper_white_spectrum);

// Fit Yule-Nielsen n factor
float fit_yule_nielsen_n(
    const Wf2KMModel& km_model,
    const std::vector<Wf2Measurement>& measurements);

// Workflow 2 calibration result
struct Wf2CalibrationResult {
    Wf2AllInkLimits ink_limits;
    Wf2KMModel km_model;
    float yule_nielsen_n;
    float total_ink_limit;
};

// Run full workflow 2 calibration
Wf2CalibrationResult run_wf2_calibration(
    const Wf2ChartLayout& chart,
    const std::vector<Wf2Measurement>& measurements,
    bool dry_run);

}  // namespace workflow
}  // namespace jcolour