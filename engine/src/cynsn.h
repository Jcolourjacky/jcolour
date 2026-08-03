#pragma once

#include "spectral.h"
#include "km.h"
#include <vector>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace jcolour {
namespace forward {

// Demichel dot-overlap probabilities for N independent inks.
//
// For N inks with coverages c[0..N-1], there are 2^N Neugebauer primaries.
// Primary index k is a bitmask: bit j set means ink j is present.
// Demichel probability: product of c[j] if bit j set, else (1-c[j]).
std::vector<double> demichel(const std::vector<double>& coverages);

// Yule-Nielsen Spectral Neugebauer (YNSN) model.
//
// Predicts reflectance by:
//   1. Computing Demichel probabilities for each Neugebauer primary
//   2. Mixing primaries in the 1/n power domain (Yule-Nielsen)
//   3. Raising back to the n-th power
//
// Primaries must be added in binary order: primary 0 = substrate (no ink),
// primary 1 = ink 0 alone, primary 2 = ink 1 alone, primary 3 = ink 0+1, etc.
// Total primaries = 2^N where N = number of inks.
class YNSNModel : public ForwardModel {
public:
    // n: Yule-Nielsen exponent (typically 1.0–4.0; 1.0 = classical Neugebauer)
    explicit YNSNModel(double n = 2.5);

    // Add a Neugebauer primary in binary order (substrate first, then single inks,
    // then combinations). Must add exactly 2^N primaries for N inks.
    void add_primary(const spectral::Spectrum& primary);

    spectral::Spectrum predict(const std::vector<double>& inks) const override;

    int num_inks() const override { return n_inks_; }

    double yn_exponent() const { return n_; }

private:
    double n_;
    int n_inks_ = 0;
    std::vector<spectral::Spectrum> primaries_;
};

// Cellular YNSN model.
//
// Divides [0,1]^N ink space into divisions^N cells. Each cell corner stores a
// measured spectrum. Prediction finds the enclosing cell, then applies YNSN
// using the 2^N corner spectra as local Neugebauer primaries.
//
// Usage:
//   CellularYNSNModel m(4, 2, 2.5);          // 4 inks, 2 divisions, n=2.5
//   m.set_grid_point({0,0,0,0}, substrate);   // all corners must be set
//   m.set_grid_point({1,0,0,0}, ink0_half);
//   ...
//   auto R = m.predict({0.3, 0.7, 0.1, 0.5});
class CellularYNSNModel : public ForwardModel {
public:
    CellularYNSNModel(int n_inks, int divisions = 2, double yn_n = 2.5);

    // Set spectrum at grid point. pos[j] ∈ [0, divisions].
    void set_grid_point(const std::vector<int>& pos, const spectral::Spectrum& spectrum);

    spectral::Spectrum predict(const std::vector<double>& inks) const override;

    int num_inks() const override { return n_inks_; }
    int divisions() const { return divisions_; }
    double yn_exponent() const { return yn_n_; }

private:
    int n_inks_;
    int divisions_;
    double yn_n_;
    std::vector<spectral::Spectrum> grid_;  // (divisions+1)^n_inks entries

    int grid_index(const std::vector<int>& pos) const;
};

}  // namespace forward
}  // namespace jcolour
