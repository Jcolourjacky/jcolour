#include <catch2/catch_test_macros.hpp>
#include <numeric>
#include "wf3_lut.h"
#include "wf3_measure.h"
#include "wf3_chart.h"

using namespace jcolour;
using namespace jcolour::workflow;

TEST_CASE("[wf3] generate_wf3_lut creates valid LUT", "[wf3]") {
    WF3KMModel model;
    WF3InkLimits limits;
    Wf3SplitConfig config;

    // Setup mock model
    model.paper_white.fill(0.95f);
    auto params = std::make_unique<WF3CYNSNParams>();
    params->n = 2.0f;
    for (int i = 0; i < WF3_NUM_CHANNELS; ++i) {
        // Per-wavelength K/S full coverage. Cheap stub for tests — real
        // values come from fit_wf3_km_model().
        for (int wl = 0; wl < 36; ++wl) {
            params->km_ks_full[i][wl] = 0.5f;
        }
    }
    model.params = std::move(params);

    // Setup ink limits
    for (int i = 0; i < WF3_NUM_CHANNELS; ++i) {
        WF3InkLimits::PerChannelLimit limit;
        limit.channel = i;
        limit.max_density = 1.0f;
        limits.limits.push_back(limit);
    }

    auto lut = generate_wf3_lut(model, limits, config, TonePreset::NEUTRAL);

    REQUIRE(lut.entries.size() == WF3_LUT_SIZE);
    REQUIRE(lut.preset == TonePreset::NEUTRAL);
}

TEST_CASE("[wf3] generate_wf3_lut for all presets", "[wf3]") {
    WF3KMModel model;
    WF3InkLimits limits;
    Wf3SplitConfig config;

    model.paper_white.fill(0.95f);
    auto params = std::make_unique<WF3CYNSNParams>();
    params->n = 2.0f;
    for (int i = 0; i < WF3_NUM_CHANNELS; ++i) {
        // Per-wavelength K/S full coverage. Cheap stub for tests — real
        // values come from fit_wf3_km_model().
        for (int wl = 0; wl < 36; ++wl) {
            params->km_ks_full[i][wl] = 0.5f;
        }
    }
    model.params = std::move(params);

    for (int i = 0; i < WF3_NUM_CHANNELS; ++i) {
        WF3InkLimits::PerChannelLimit limit;
        limit.channel = i;
        limit.max_density = 1.0f;
        limits.limits.push_back(limit);
    }

    auto lut_map = generate_all_wf3_presets(model, limits);

    REQUIRE(lut_map.size() == 4);
    REQUIRE(lut_map.count(TonePreset::NEUTRAL) == 1);
    REQUIRE(lut_map.count(TonePreset::WARM) == 1);
    REQUIRE(lut_map.count(TonePreset::COOL) == 1);
    REQUIRE(lut_map.count(TonePreset::SPLIT) == 1);
}

TEST_CASE("[wf3] wf3_lut_lookup returns ink values", "[wf3]") {
    WF3KMModel model;
    WF3InkLimits limits;
    Wf3SplitConfig config;

    model.paper_white.fill(0.95f);
    auto params = std::make_unique<WF3CYNSNParams>();
    params->n = 2.0f;
    for (int i = 0; i < WF3_NUM_CHANNELS; ++i) {
        // Per-wavelength K/S full coverage. Cheap stub for tests — real
        // values come from fit_wf3_km_model().
        for (int wl = 0; wl < 36; ++wl) {
            params->km_ks_full[i][wl] = 0.5f;
        }
    }
    model.params = std::move(params);

    for (int i = 0; i < WF3_NUM_CHANNELS; ++i) {
        WF3InkLimits::PerChannelLimit limit;
        limit.channel = i;
        limit.max_density = 1.0f;
        limits.limits.push_back(limit);
    }

    auto lut = generate_wf3_lut(model, limits, config, TonePreset::NEUTRAL);
    auto inks = wf3_lut_lookup(lut, 50.0f, 1.0f);

    REQUIRE(inks.size() == WF3_NUM_CHANNELS);
    for (float ink : inks) {
        REQUIRE(ink >= 0.0f);
        REQUIRE(ink <= 1.0f);
    }
}

TEST_CASE("[wf3] enforce_wf3_monotonicity", "[wf3]") {
    Wf3LUT lut;
    lut.entries.resize(10);

    for (int i = 0; i < 10; ++i) {
        lut.entries[i].inks.fill(0.5f + i * 0.05f);
    }

    enforce_wf3_monotonicity(lut);

    // After enforcement, total ink should be non-decreasing
    for (size_t i = 1; i < lut.entries.size(); ++i) {
        float prev_total = std::accumulate(lut.entries[i-1].inks.begin(),
                                          lut.entries[i-1].inks.end(), 0.0f);
        float curr_total = std::accumulate(lut.entries[i].inks.begin(),
                                          lut.entries[i].inks.end(), 0.0f);
        REQUIRE(curr_total >= prev_total - 0.001f);
    }
}

TEST_CASE("[wf3] validate_wf3_lut", "[wf3]") {
    WF3KMModel model;
    WF3InkLimits limits;
    Wf3SplitConfig config;

    model.paper_white.fill(0.95f);
    auto params = std::make_unique<WF3CYNSNParams>();
    params->n = 2.0f;
    for (int i = 0; i < WF3_NUM_CHANNELS; ++i) {
        // Per-wavelength K/S full coverage. Cheap stub for tests — real
        // values come from fit_wf3_km_model().
        for (int wl = 0; wl < 36; ++wl) {
            params->km_ks_full[i][wl] = 0.5f;
        }
    }
    model.params = std::move(params);

    for (int i = 0; i < WF3_NUM_CHANNELS; ++i) {
        WF3InkLimits::PerChannelLimit limit;
        limit.channel = i;
        limit.max_density = 1.0f;
        limits.limits.push_back(limit);
    }

    auto lut = generate_wf3_lut(model, limits, config, TonePreset::NEUTRAL);
    auto validation = validate_wf3_lut(lut, model);

    REQUIRE(validation.avg_delta_e >= 0.0f);
    REQUIRE(validation.max_delta_e >= 0.0f);
}
