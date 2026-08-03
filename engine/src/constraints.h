#pragma once

// Constraint functions for ink-coverage optimization.
//
// These return residual vectors suitable for use as equality (h(x)==0)
// or inequality (g(x)<=0) constraints in the SQP solver.
//
// Each constraint wraps a ForwardModel and maps ink coverages to a
// spectral prediction, then computes the constraint residual.

#include "spectral.h"
#include "delta_e.h"
#include "km.h"
#include <vector>
#include <functional>

namespace jcolour {
namespace optimizer {

// ForwardFn: maps ink coverages -> predicted spectrum.
using ForwardFn = std::function<spectral::Spectrum(const std::vector<double>&)>;

// LStarMatch: equality constraint — predicted L* must equal target L*.
// h(x) = L*(predicted) - L*_target = 0
// Returns a 1-element vector.
struct LStarMatch {
    ForwardFn fn;
    double    target_L;
    spectral::Spectrum illuminant;

    LStarMatch(ForwardFn forward, double L_target,
               spectral::Spectrum illum = spectral::Illuminant::D50());

    std::vector<double> operator()(const std::vector<double>& coverages) const;
};

// LabMatch: equality constraint — predicted Lab must equal target Lab.
// h(x) = [L*-L*t, a*-a*t, b*-b*t] = 0
// Returns a 3-element vector.
struct LabMatch {
    ForwardFn       fn;
    spectral::Lab   target;
    spectral::Spectrum illuminant;

    LabMatch(ForwardFn forward, spectral::Lab lab_target,
             spectral::Spectrum illum = spectral::Illuminant::D50());

    std::vector<double> operator()(const std::vector<double>& coverages) const;
};

// TILConstraint: inequality constraint — total ink load must not exceed limit.
// g(x) = sum(c[i]) - til_limit <= 0
// Returns a 1-element vector.
struct TILConstraint {
    double til_limit;  // e.g. 3.0 for 300% TIL

    explicit TILConstraint(double limit);

    std::vector<double> operator()(const std::vector<double>& coverages) const;
};

}  // namespace optimizer
}  // namespace jcolour
