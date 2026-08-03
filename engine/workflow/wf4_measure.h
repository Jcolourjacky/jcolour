#pragma once

#include "wf4_chart.h"
#include <vector>
#include <map>

namespace jcolour {
namespace workflow {

// Measurement result for a single patch.
//
// For single-ink ramp patches: `channel` and `ink_density` describe the
// patch composition. `composition` is empty.
//
// For TIL patches (multi-channel combinations): `channel` is set to
// InkChannel::MAIN_K as a sentinel meaning "see composition", `ink_density`
// is 0, and `composition` carries the full (channel, density) list copied
// from the originating TILPatch. This fixes audit F10/F23 — TIL patch
// composition is preserved rather than discarded.
struct WF4Measurement {
    std::string label;
    InkChannel channel;  // Single-channel patch: the channel. TIL patch: MAIN_K sentinel.
    float ink_density;   // 0.0..1.0; meaningful only for single-channel patches.
    double L_star = 0.0;
    double a_star = 0.0;
    double b_star = 0.0;
    std::vector<double> spectral_data;  // 36ch
    std::vector<std::pair<InkChannel, float>> composition;  // TIL patches only.
};

// Ink limit result for a single channel
struct WF4InkLimit {
    InkChannel channel = InkChannel::MAIN_K;
    float limit = 0.0f;  // Max density before ink coalescence
    float dL_ds_start = 0.0f;  // Slope at start
    float dL_ds_end = 0.0f;  // Slope at end (near saturation)
    bool is_monotonic = true;
};

// TIL result
struct WF4TILResult {
    float til = 0.0f;  // Total ink limit (sum of all channels)
    std::map<InkChannel, float> individual_limits;
    float coalescence_threshold = 0.0f;
};

// Ink limit result for all 12 channels
struct WF4AllInkLimits {
    std::vector<WF4InkLimit> per_channel;
    WF4TILResult til;
};

// Simulate measurement data (for dry_run mode)
std::vector<WF4Measurement> simulate_wf4_measurements(const WF4ChartLayout& chart);

// --------------------------------------------------------------------------
// MeasurementBackend — pluggable interface so real hardware (i1Pro3, i1iO,
// Barbieri Spectro LFP qb…) can replace the simulator. Audit F24 fix:
// previously `analyze_wf4_ink_limits(dry_run)` silently called simulate on
// both branches; now the dispatch is explicit through this interface.
//
// Real backends implement measure() to drive the hardware; SimulatedBackend
// is the default and matches the historical behavior.
// --------------------------------------------------------------------------
class WF4MeasurementBackend {
public:
    virtual ~WF4MeasurementBackend() = default;
    virtual std::vector<WF4Measurement> measure(const WF4ChartLayout& chart) = 0;
    virtual const char* name() const = 0;
};

class WF4SimulatedBackend : public WF4MeasurementBackend {
public:
    std::vector<WF4Measurement> measure(const WF4ChartLayout& chart) override {
        return simulate_wf4_measurements(chart);
    }
    const char* name() const override { return "simulated"; }
};

// Stub placeholder for the future i1Pro3 / i1iO integration (Spike 3).
// Throws/returns empty until the real SDK is wired through engine/i1sdk.
class WF4I1Pro3Backend : public WF4MeasurementBackend {
public:
    std::vector<WF4Measurement> measure(const WF4ChartLayout& /*chart*/) override {
        // TODO(spike-3): drive the i1Pro3 SDK via i1sdk::scan_chart().
        return {};
    }
    const char* name() const override { return "i1pro3-stub"; }
};

// Analyze single-channel ink ramp to find saturation point
WF4InkLimit analyze_ink_limit(const std::vector<WF4Measurement>& channel_measurements);

// Analyze TIL patches to find total ink limit
WF4TILResult analyze_til(const std::vector<WF4Measurement>& til_measurements);

// Run full ink limit analysis with the default backend (simulated) — the
// dry_run boolean is now an alias for "use simulated backend"; pass an
// explicit backend instead for non-simulated runs.
WF4AllInkLimits analyze_wf4_ink_limits(const WF4ChartLayout& chart, bool dry_run = true);

// New: pass an explicit backend for real measurement runs.
WF4AllInkLimits analyze_wf4_ink_limits(const WF4ChartLayout& chart,
                                        WF4MeasurementBackend& backend);

}  // namespace workflow
}  // namespace jcolour
