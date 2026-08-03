#include "constraints.h"
#include <numeric>

namespace jcolour {
namespace optimizer {

// ---- LStarMatch ----

LStarMatch::LStarMatch(ForwardFn forward, double L_target, spectral::Spectrum illum)
    : fn(std::move(forward)), target_L(L_target), illuminant(illum) {}

std::vector<double> LStarMatch::operator()(const std::vector<double>& coverages) const {
    auto predicted = fn(coverages);
    auto xyz = spectral::spectrum_to_xyz(predicted, illuminant);
    auto lab = spectral::xyz_to_lab(xyz);
    return {lab.L - target_L};
}

// ---- LabMatch ----

LabMatch::LabMatch(ForwardFn forward, spectral::Lab lab_target, spectral::Spectrum illum)
    : fn(std::move(forward)), target(lab_target), illuminant(illum) {}

std::vector<double> LabMatch::operator()(const std::vector<double>& coverages) const {
    auto predicted = fn(coverages);
    auto xyz = spectral::spectrum_to_xyz(predicted, illuminant);
    auto lab = spectral::xyz_to_lab(xyz);
    return {lab.L - target.L, lab.a - target.a, lab.b - target.b};
}

// ---- TILConstraint ----

TILConstraint::TILConstraint(double limit) : til_limit(limit) {}

std::vector<double> TILConstraint::operator()(const std::vector<double>& coverages) const {
    double total = 0.0;
    for (double c : coverages) total += c;
    return {total - til_limit};
}

}  // namespace optimizer
}  // namespace jcolour
