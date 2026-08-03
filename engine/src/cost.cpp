#include "cost.h"
#include <numeric>
#include <cmath>

namespace jcolour {
namespace optimizer {

// ---- MetamerismCost ----

MetamerismCost::MetamerismCost(const spectral::Spectrum& target_spectrum,
                                std::vector<spectral::Spectrum> illums)
    : target(target_spectrum), illuminants(std::move(illums)) {}

double MetamerismCost::operator()(const spectral::Spectrum& predicted,
                                   const std::vector<double>& /*coverages*/) const {
    if (illuminants.empty())
        return delta_e::max_delta_e_under_standard(predicted, target);
    return delta_e::max_delta_e_under(predicted, target, illuminants);
}

// ---- GrainCost ----

double GrainCost::operator()(const spectral::Spectrum& /*predicted*/,
                               const std::vector<double>& coverages) const {
    double s = 0.0;
    for (double c : coverages) s += c * c;
    return s;
}

// ---- SmoothnessCost ----

double SmoothnessCost::operator()(const spectral::Spectrum& /*predicted*/,
                                   const std::vector<double>& coverages) const {
    double s = 0.0;
    for (size_t i = 1; i < coverages.size(); ++i) {
        double d = coverages[i] - coverages[i - 1];
        s += d * d;
    }
    return s;
}

// ---- TotalInkCost ----

double TotalInkCost::operator()(const spectral::Spectrum& /*predicted*/,
                                 const std::vector<double>& coverages) const {
    if (coverages.empty()) return 0.0;
    double total = 0.0;
    for (double c : coverages) total += c;
    double normalized = total / static_cast<double>(coverages.size());
    return normalized * normalized;
}

// ---- WeightedCost ----

void WeightedCost::add(const CostFn& fn, double weight) {
    terms.push_back({&fn, weight});
}

double WeightedCost::operator()(const spectral::Spectrum& predicted,
                                 const std::vector<double>& coverages) const {
    double total = 0.0;
    for (const auto& t : terms)
        total += t.weight * (*t.fn)(predicted, coverages);
    return total;
}

}  // namespace optimizer
}  // namespace jcolour
