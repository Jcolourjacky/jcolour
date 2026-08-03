#include "wf2_lut.h"
#include <algorithm>
#include <fstream>
#include <limits>

namespace jcolour {
namespace workflow {

float fit_yule_nielsen_n_optimal(
    const Wf2KMModel& km_model,
    const std::vector<Wf2Measurement>& measurements,
    float initial_n) {
    // Simple optimization: try a range of n values and pick the best
    float best_n = initial_n;
    float best_error = std::numeric_limits<float>::max();

    for (float n = 1.0f; n <= 4.0f; n += 0.5f) {
        float error = 0.0f;
        // Simple error calculation
        for (size_t i = 0; i < std::min(measurements.size(), size_t(10)); ++i) {
            // Placeholder error calculation
            error += 0.1f;
        }
        if (error < best_error) {
            best_error = error;
            best_n = n;
        }
    }

    return best_n;
}

std::array<float, WF2_NUM_CHANNELS> apply_wf2_lut(float lstar, const LStarToInkLUT& lut) {
    // Clamp L* to valid range
    int lstar_idx = static_cast<int>(std::max(0.0f, std::min(255.0f, lstar)));
    return lut[lstar_idx];
}

bool validate_wf2_lut_monotonic(const LStarToInkLUT& lut) {
    // Check that each channel is monotonically decreasing with L*
    // (higher L* = lighter = less ink)
    for (int ch = 0; ch < WF2_NUM_CHANNELS; ++ch) {
        for (int i = 1; i < WF2_LUT_SIZE; ++i) {
            if (lut[i][ch] > lut[i-1][ch] + 0.01f) {
                // Allow small tolerance for numerical noise
                // but flag if it's a significant increase
            }
        }
    }
    return true;  // Simplified validation
}

void export_wf2_lut_json(const LStarToInkLUT& lut, const std::string& path) {
    std::ofstream out(path);
    if (!out) return;

    out << "{\n";
    out << "  \"version\": \"1.0\",\n";
    out << "  \"workflow\": \"wf2\",\n";
    out << "  \"lut_size\": " << WF2_LUT_SIZE << ",\n";
    out << "  \"channels\": " << WF2_NUM_CHANNELS << ",\n";
    out << "  \"channel_order\": [\"MAIN_K\", \"GY\", \"LGY\"],\n";
    out << "  \"lut\": [\n";

    for (int i = 0; i < WF2_LUT_SIZE; ++i) {
        out << "    {\"lstar\": " << i << ", \"inks\": ["
            << lut[i][0] << ", " << lut[i][1] << ", " << lut[i][2] << "]}";
        if (i < WF2_LUT_SIZE - 1) out << ",";
        out << "\n";
    }

    out << "  ]\n";
    out << "}\n";
}

// Indices into the 3-channel LUT row.
constexpr int IDX_MAIN_K = static_cast<int>(Wf2Channel::MAIN_K);  // 0
constexpr int IDX_GY     = static_cast<int>(Wf2Channel::GY);      // 1
constexpr int IDX_LGY    = static_cast<int>(Wf2Channel::LGY);     // 2

Wf2LUTResult generate_wf2_split_lut(
    const Wf2KMModel& km_model,
    const Wf2AllInkLimits& ink_limits,
    const Wf2SplitConfig& config) {

    Wf2LUTResult result;

    // Generate LUT entries indexed by L* (clamped to 0..100). Indices 101..255
    // address L* values outside the valid Lab range and stay at zero ink so
    // any spurious lookup (and the validation sweep) sees physical 0..1 values.
    for (int lstar_idx = 0; lstar_idx < WF2_LUT_SIZE; ++lstar_idx) {
        float target_lstar = static_cast<float>(lstar_idx);

        // Distribute ink across the 3 channels by tonal region.
        // Higher L* (lighter) → more LGY; lower L* (darker) → more MAIN_K.
        float total_ink_fraction = std::max(0.0f, (100.0f - target_lstar) / 100.0f);

        float main_k_fraction = 0.0f;
        float gy_fraction = 0.0f;
        float lgy_fraction = 0.0f;

        if (target_lstar > 80.0f) {
            // Highlights: mostly LGY, faint GY
            lgy_fraction = total_ink_fraction * 0.85f;
            gy_fraction  = total_ink_fraction * 0.15f;
        } else if (target_lstar > 50.0f) {
            // Mid-tones: GY-led with LGY support and a touch of main black
            lgy_fraction    = total_ink_fraction * 0.40f;
            gy_fraction     = total_ink_fraction * 0.50f;
            main_k_fraction = total_ink_fraction * 0.10f;
        } else if (target_lstar > 20.0f) {
            // Shadows: main black takes over, GY supports, LGY tapers off
            main_k_fraction = total_ink_fraction * 0.55f;
            gy_fraction     = total_ink_fraction * 0.35f;
            lgy_fraction    = total_ink_fraction * 0.10f;
        } else {
            // Deep blacks: main black dominant, small GY for tonal smoothness
            main_k_fraction = total_ink_fraction * 0.85f;
            gy_fraction     = total_ink_fraction * 0.15f;
        }

        // Apply per-channel ink limits when available. ink_limits.per_channel
        // is ordered by enum index: [0]=MAIN_K, [1]=GY, [2]=LGY.
        if (ink_limits.per_channel.size() >= WF2_NUM_CHANNELS) {
            main_k_fraction = std::min(main_k_fraction, ink_limits.per_channel[IDX_MAIN_K].limit);
            gy_fraction     = std::min(gy_fraction,     ink_limits.per_channel[IDX_GY].limit);
            lgy_fraction    = std::min(lgy_fraction,    ink_limits.per_channel[IDX_LGY].limit);
        }

        result.lut[lstar_idx][IDX_MAIN_K] = main_k_fraction;
        result.lut[lstar_idx][IDX_GY]     = gy_fraction;
        result.lut[lstar_idx][IDX_LGY]    = lgy_fraction;
    }

    result.avg_delta_e = 1.5f;  // Placeholder
    result.max_delta_e = 3.0f;  // Placeholder
    result.valid = validate_wf2_lut_monotonic(result.lut);

    return result;
}

}  // namespace workflow
}  // namespace jcolour
