#pragma once

#include "spectral.h"
#include <vector>
#include <array>
#include <stdexcept>

namespace jcolour {
namespace forward {

// Abstract base for all forward models (K-M, CYNSN, etc.)
class ForwardModel {
public:
    virtual ~ForwardModel() = default;

    // Predict reflectance spectrum from ink coverage values [0,1] per channel.
    virtual spectral::Spectrum predict(const std::vector<double>& inks) const = 0;

    virtual int num_inks() const = 0;
};

// Kubelka-Munk single-ink K/S coefficients for one spectral channel.
struct KMInk {
    double K;  // absorption coefficient
    double S;  // scattering coefficient
};

// Kubelka-Munk forward model.
//
// Predicts reflectance by mixing K/S coefficients of substrate and inks,
// then converting back to reflectance via the K-M formula.
//
// Usage:
//   KMForwardModel model(substrate_spectrum);
//   model.add_ink(ink_full_tone_spectrum);  // repeat for each ink
//   auto R = model.predict({0.5, 0.3});     // 2-ink mix
class KMForwardModel : public ForwardModel {
public:
    // substrate: paper/media reflectance spectrum (full white = no ink)
    explicit KMForwardModel(const spectral::Spectrum& substrate);

    // Add an ink defined by its full-tone (100%) reflectance spectrum.
    // Call once per ink channel, in order.
    void add_ink(const spectral::Spectrum& full_tone);

    spectral::Spectrum predict(const std::vector<double>& inks) const override;

    int num_inks() const override { return static_cast<int>(inks_.size()); }

    // Expose substrate K/S for inspection/testing
    const std::array<KMInk, spectral::NUM_CHANNELS>& substrate_ks() const { return substrate_ks_; }

private:
    static KMInk reflectance_to_ks(double R);
    static double ks_to_reflectance(double K, double S);

    std::array<KMInk, spectral::NUM_CHANNELS> substrate_ks_;
    std::vector<std::array<KMInk, spectral::NUM_CHANNELS>> inks_;
};

}  // namespace forward
}  // namespace jcolour
