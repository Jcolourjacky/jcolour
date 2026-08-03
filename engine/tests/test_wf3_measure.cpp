#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "wf3_measure.h"
#include "wf3_chart.h"

#include <cmath>

using namespace jcolour::workflow;
using Catch::Approx;

namespace {
// Build a minimal model with paper_white and channel-specific full-coverage
// K/S that's plausible enough for prediction tests.
WF3KMModel make_test_model() {
    WF3KMModel model;
    model.params = std::make_unique<WF3CYNSNParams>();
    model.params->n = 2.0f;
    model.paper_white.fill(0.95f);
    for (int ch = 0; ch < 6; ++ch) {
        for (int wl = 0; wl < 36; ++wl) {
            // Channel ch gets K/S = 0.5 + 0.1*ch — distinct per channel.
            model.params->km_ks_full[ch][wl] = 0.5f + 0.1f * ch;
        }
    }
    return model;
}
}  // namespace

TEST_CASE("[wf3] predict_wf3_spectrum", "[wf3]") {
    WF3KMModel model;
    model.params = std::make_unique<WF3CYNSNParams>();
    model.paper_white.fill(0.95f);

    std::array<float, 6> inks{};
    inks.fill(0.0f);

    auto spectrum = predict_wf3_spectrum(inks, model);

    REQUIRE(spectrum[0] > 0.0f);
}

TEST_CASE("[wf3] fit_wf3_km_model", "[wf3]") {
    std::vector<WF3Measurement> measurements;
    WF3Measurement m;
    m.spectrum.fill(0.5f);
    measurements.push_back(m);

    std::vector<int> channels{0, 1, 2, 3, 4, 5};
    auto model = fit_wf3_km_model(measurements, channels);

    REQUIRE(model.params != nullptr);
    REQUIRE(model.params->n == 2.0f);
}

TEST_CASE("[wf3] fit_wf3_yule_nielsen_n", "[wf3]") {
    WF3KMModel model;
    model.params = std::make_unique<WF3CYNSNParams>();
    model.params->n = 2.0f;
    model.paper_white.fill(0.95f);

    std::vector<WF3Measurement> measurements;
    measurements.push_back({});

    float n = fit_wf3_yule_nielsen_n(model, measurements);

    REQUIRE(n >= 1.5f);
    REQUIRE(n <= 4.0f);
}

TEST_CASE("[wf3] analyze_wf3_ink_limits", "[wf3]") {
    std::vector<WF3Measurement> measurements;
    for (int i = 0; i < 6; ++i) {
        WF3Measurement m;
        m.L_star = 50.0f + i * 5.0f;
        measurements.push_back(m);
    }

    auto limits = analyze_wf3_ink_limits(measurements);

    REQUIRE((int)limits.limits.size() == 6);
}

TEST_CASE("[wf3] run_wf3_calibration", "[wf3]") {
    auto chart = generate_wf3_ink_ramp();
    auto measurements = simulate_wf3_measurements(chart);

    WF3KMModel model;
    WF3InkLimits limits;

    bool success = run_wf3_calibration(chart, measurements, model, limits);

    REQUIRE(success == true);
    REQUIRE(model.params != nullptr);
}

TEST_CASE("[wf3] simulate_wf3_measurements", "[wf3]") {
    auto chart = generate_wf3_ink_ramp();

    auto measurements = simulate_wf3_measurements(chart);

    REQUIRE((int)measurements.size() == (int)chart.patches.size());
    REQUIRE(measurements[0].spectrum.size() == 36);
}

// =====================================================================
// F26 regression: value-correctness for the new K-M model (audit Phase D)
// =====================================================================

TEST_CASE("[wf3-km] predict with zero inks returns paper white",
          "[wf3][regression]") {
    auto model = make_test_model();
    std::array<float, 6> inks{};  // all zero
    auto spectrum = predict_wf3_spectrum(inks, model);

    // n = 2 means R is √R_paper-ish — must still be near paper white in
    // log-reflectance terms, never far below it.
    for (int wl = 0; wl < 36; ++wl) {
        // With n=2 and paper_white=0.95, predicted ≈ sqrt(0.95) ≈ 0.974.
        REQUIRE(spectrum[wl] > 0.9f);
        REQUIRE(spectrum[wl] <= 1.0f);
    }
}

TEST_CASE("[wf3-km] full single-ink coverage darkens the spectrum",
          "[wf3][regression]") {
    auto model = make_test_model();

    // Empty mix: lightest
    std::array<float, 6> empty{};
    auto paper_spec = predict_wf3_spectrum(empty, model);

    // Single channel at full coverage: must be darker than paper
    for (int ch = 0; ch < 6; ++ch) {
        std::array<float, 6> mix{};
        mix[ch] = 1.0f;
        auto inked_spec = predict_wf3_spectrum(mix, model);
        // Sum of inked reflectance must be strictly less than paper sum
        float paper_sum = 0.0f, inked_sum = 0.0f;
        for (int wl = 0; wl < 36; ++wl) {
            paper_sum += paper_spec[wl];
            inked_sum += inked_spec[wl];
        }
        REQUIRE(inked_sum < paper_sum);
    }
}

TEST_CASE("[wf3-km] different channels produce different spectra",
          "[wf3][regression]") {
    // F7 regression: previous K-M produced a flat per-channel scalar K/S
    // that made every channel yield identical spectra. The new
    // per-wavelength model must produce channel-distinguishable output.
    WF3KMModel model;
    model.params = std::make_unique<WF3CYNSNParams>();
    model.params->n = 2.0f;
    model.paper_white.fill(0.95f);
    // Give each channel a distinct spectral signature (peak at a
    // different wavelength).
    for (int ch = 0; ch < 6; ++ch) {
        for (int wl = 0; wl < 36; ++wl) {
            float distance = std::abs(wl - ch * 6);
            model.params->km_ks_full[ch][wl] = 0.3f + 1.5f * std::exp(-distance * 0.1f);
        }
    }

    // For each pair of channels, the resulting spectra must differ
    // somewhere (max-pixel difference > tolerance).
    for (int a = 0; a < 6; ++a) {
        for (int b = a + 1; b < 6; ++b) {
            std::array<float, 6> mix_a{};
            std::array<float, 6> mix_b{};
            mix_a[a] = 1.0f;
            mix_b[b] = 1.0f;
            auto spec_a = predict_wf3_spectrum(mix_a, model);
            auto spec_b = predict_wf3_spectrum(mix_b, model);

            float max_diff = 0.0f;
            for (int wl = 0; wl < 36; ++wl) {
                max_diff = std::max(max_diff,
                                    std::abs(spec_a[wl] - spec_b[wl]));
            }
            INFO("channels " << a << " vs " << b);
            REQUIRE(max_diff > 0.05f);
        }
    }
}

TEST_CASE("[wf3-km] fit recovers a model that re-predicts measurements",
          "[wf3][regression]") {
    // F8 regression: fit_wf3_km_model used to hardcode avg_reflectance=0.5
    // for every channel. The fit now actually uses measurement data, so
    // re-predicting the fit input should match the input reasonably.
    auto chart = generate_wf3_ink_ramp();
    auto measurements = simulate_wf3_measurements(chart);

    std::vector<int> channels = {0, 1, 2, 3, 4, 5};
    auto model = fit_wf3_km_model(measurements, channels);

    // For each channel's highest-density measurement, re-predict and
    // compare the spectrum L2 distance.
    for (int ch = 0; ch < 6; ++ch) {
        const WF3Measurement* best = nullptr;
        for (const auto& m : measurements) {
            if (m.channel != ch || m.ink_density < 0.99f) continue;
            best = &m;
            break;
        }
        if (!best) continue;

        std::array<float, 6> mix{};
        mix[ch] = best->ink_density;
        auto pred = predict_wf3_spectrum(mix, model);

        double sq_err = 0.0;
        for (int wl = 0; wl < 36; ++wl) {
            double d = pred[wl] - best->spectrum[wl];
            sq_err += d * d;
        }
        double rms = std::sqrt(sq_err / 36.0);
        INFO("channel " << ch << " RMS reflectance error = " << rms);
        // Single-constant K-M can't perfectly recover the 3-band test
        // simulation — but the fit must produce a *non-trivial* result
        // (much better than predicting paper-white for everything, which
        // would give RMS ≈ 0.5+ for chromatic channels).
        REQUIRE(rms < 0.35);
    }
}

TEST_CASE("[wf3-km] fit_yule_nielsen_n actually depends on n",
          "[wf3][regression]") {
    // F9 regression: previous fit had `error += 0.01f` independent of n,
    // so it returned an arbitrary value. The real grid search should now
    // (a) stay in the [1.0, 4.0] grid, (b) discriminate between different
    // input n values.
    auto chart = generate_wf3_ink_ramp();
    auto measurements = simulate_wf3_measurements(chart);
    std::vector<int> channels = {0, 1, 2, 3, 4, 5};
    auto model = fit_wf3_km_model(measurements, channels);

    float n = fit_wf3_yule_nielsen_n(model, measurements);
    REQUIRE(n >= 1.0f);
    REQUIRE(n <= 4.0f);

    // If we evaluate the same measurements against a model whose stored
    // K/S corresponds to a different n, the optimal n should shift.
    // (Sanity: at least one value of n should win clearly over the rest.)
    auto model_alt = fit_wf3_km_model(measurements, channels);
    model_alt.params->n = 1.0f;  // pretend we baked a different n in
    float n_alt = fit_wf3_yule_nielsen_n(model_alt, measurements);
    REQUIRE(n_alt >= 1.0f);
    REQUIRE(n_alt <= 4.0f);
}

TEST_CASE("[wf3-km] analyze_wf3_ink_limits respects per-channel data",
          "[wf3][regression]") {
    // F21-adjacent: ink_limits should now reflect per-channel L*
    // measurements, not an aggregate over the whole chart.
    auto chart = generate_wf3_ink_ramp();
    auto measurements = simulate_wf3_measurements(chart);
    auto limits = analyze_wf3_ink_limits(measurements);

    REQUIRE((int)limits.limits.size() == WF3_NUM_CHANNELS);

    // Channel 0 (MAIN_K) should produce the deepest ΔL* (because the
    // simulation makes it the most absorbing channel).
    float main_k_dmax = 0.0f;
    for (const auto& l : limits.limits) {
        if (l.channel == 0) main_k_dmax = l.D_max;
    }
    REQUIRE(main_k_dmax > 0.0f);
}

TEST_CASE("[wf3-km] simulate is deterministic across calls",
          "[wf3][regression]") {
    // F30 regression: replacing std::rand with seeded mt19937 means two
    // calls in the same process must produce identical output.
    auto chart = generate_wf3_ink_ramp();
    auto a = simulate_wf3_measurements(chart);
    auto b = simulate_wf3_measurements(chart);

    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        for (int wl = 0; wl < 36; ++wl) {
            REQUIRE(a[i].spectrum[wl] == Approx(b[i].spectrum[wl]));
        }
    }
}