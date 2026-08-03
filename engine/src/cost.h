#pragma once

// Cost functions for ink-coverage optimization.
//
// Each cost function takes a predicted spectrum and/or ink coverage vector
// and returns a scalar penalty. They are designed to be composed as a
// weighted sum in the optimizer objective.

#include "spectral.h"
#include "delta_e.h"
#include "km.h"
#include <vector>
#include <functional>

namespace jcolour {
namespace optimizer {

// Base interface for all cost functions.
struct CostFn {
    virtual ~CostFn() = default;
    // Evaluate cost given predicted spectrum and ink coverages.
    virtual double operator()(const spectral::Spectrum& predicted,
                               const std::vector<double>& coverages) const = 0;
};

// MetamerismCost: ΔE2000 between predicted and target under multiple illuminants.
// Returns max ΔE across all illuminants (worst-case metamerism).
struct MetamerismCost : CostFn {
    spectral::Spectrum              target;
    std::vector<spectral::Spectrum> illuminants;  // empty = standard set

    explicit MetamerismCost(const spectral::Spectrum& target_spectrum,
                             std::vector<spectral::Spectrum> illums = {});

    double operator()(const spectral::Spectrum& predicted,
                       const std::vector<double>& coverages) const override;
};

// GrainCost: penalizes high ink coverage to reduce grain/mottle.
// cost = sum(c[i]^2) — quadratic to penalize high individual coverages more.
struct GrainCost : CostFn {
    double operator()(const spectral::Spectrum& predicted,
                       const std::vector<double>& coverages) const override;
};

// SmoothnessCost: penalizes large differences between adjacent ink channels.
// cost = sum((c[i+1] - c[i])^2)
// Encourages smooth ink transitions to reduce banding artifacts.
struct SmoothnessCost : CostFn {
    double operator()(const spectral::Spectrum& predicted,
                       const std::vector<double>& coverages) const override;
};

// TotalInkCost: penalizes total ink load (TIL).
// cost = (sum(c[i]) / n_inks)^2 — normalized and squared.
struct TotalInkCost : CostFn {
    double operator()(const spectral::Spectrum& predicted,
                       const std::vector<double>& coverages) const override;
};

// WeightedCost: linear combination of multiple cost functions.
struct WeightedCost {
    struct Term {
        const CostFn* fn;
        double        weight;
    };
    std::vector<Term> terms;

    void add(const CostFn& fn, double weight);
    double operator()(const spectral::Spectrum& predicted,
                       const std::vector<double>& coverages) const;
};

}  // namespace optimizer
}  // namespace jcolour
