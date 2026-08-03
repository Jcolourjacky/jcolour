#include "km.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace jcolour {
namespace forward {

// Clamp reflectance to avoid division by zero or negative K/S.
static double clamp_r(double R) {
    return std::max(1e-6, std::min(1.0 - 1e-6, R));
}

KMInk KMForwardModel::reflectance_to_ks(double R) {
    R = clamp_r(R);
    // K/S = (1-R)^2 / (2R)
    double ks = (1.0 - R) * (1.0 - R) / (2.0 * R);
    // Decompose into K and S with S=1 convention (only ratio matters for prediction)
    return {ks, 1.0};
}

double KMForwardModel::ks_to_reflectance(double K, double S) {
    if (S <= 0.0) return 0.0;
    double r = K / S;
    // K/S is physically non-negative; heavy ink overprints can drive the summed
    // ratio slightly below 0, making r*r+2*r negative → sqrt → NaN (root cause
    // of downstream xyz_to_lab NaN, see multicolor-icc-strategy.md §4). Clamp.
    if (r < 0.0) r = 0.0;
    // Saunderson-free K-M: R = 1 + r - sqrt(r*r + 2*r)
    double R = 1.0 + r - std::sqrt(r * r + 2.0 * r);
    return R < 0.0 ? 0.0 : (R > 1.0 ? 1.0 : R);   // reflectance stays in [0,1]
}

KMForwardModel::KMForwardModel(const spectral::Spectrum& substrate) {
    for (int i = 0; i < spectral::NUM_CHANNELS; ++i) {
        substrate_ks_[i] = reflectance_to_ks(substrate[i]);
    }
}

void KMForwardModel::add_ink(const spectral::Spectrum& full_tone) {
    std::array<KMInk, spectral::NUM_CHANNELS> ink_ks;
    for (int i = 0; i < spectral::NUM_CHANNELS; ++i) {
        // Ink K/S is the difference between ink-on-substrate and bare substrate.
        // K_ink = K_mix - K_substrate, S_ink = S_mix - S_substrate
        // With S=1 convention: K_ink/S_ink = KS_mix - KS_substrate
        double ks_mix = reflectance_to_ks(full_tone[i]).K;
        double ks_sub = substrate_ks_[i].K;
        double ks_ink = ks_mix - ks_sub;
        ink_ks[i] = {ks_ink, 0.0};  // S component absorbed into K/S ratio
    }
    inks_.push_back(ink_ks);
}

spectral::Spectrum KMForwardModel::predict(const std::vector<double>& inks) const {
    if (static_cast<int>(inks.size()) != num_inks()) {
        throw std::invalid_argument("ink count mismatch");
    }

    spectral::Spectrum result = {};
    for (int i = 0; i < spectral::NUM_CHANNELS; ++i) {
        // Mix K/S: K_total/S_total = K_sub + sum(c_j * K_ink_j)
        // (S=1 convention throughout, so we sum K/S ratios weighted by coverage)
        double ks_total = substrate_ks_[i].K;
        for (int j = 0; j < num_inks(); ++j) {
            double c = std::max(0.0, std::min(1.0, inks[j]));
            ks_total += c * inks_[j][i].K;
        }
        result[i] = ks_to_reflectance(ks_total, 1.0);
    }
    return result;
}

}  // namespace forward
}  // namespace jcolour
