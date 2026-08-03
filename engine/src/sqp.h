#pragma once

// Sequential Quadratic Programming solver.
//
// Minimizes f(x) subject to:
//   lb[i] <= x[i] <= ub[i]   (box constraints, always active)
//   g(x) <= 0                 (inequality constraints, log-barrier interior method)
//   h(x) == 0                 (equality constraints, augmented Lagrangian)
//
// Algorithm:
//   - BFGS quasi-Newton Hessian approximation
//   - Projected gradient for box constraints
//   - Log-barrier penalty for inequality constraints
//   - Augmented Lagrangian for equality constraints
//   - Wolfe-condition line search

#include <functional>
#include <vector>
#include <limits>
#include <cmath>

namespace jcolour {
namespace optimizer {

struct SQPOptions {
    int    max_iter       = 200;
    double grad_tol       = 1e-6;   // stop when ||projected gradient|| < tol
    double step_tol       = 1e-10;  // stop when step size < tol
    double barrier_mu     = 1.0;    // initial log-barrier weight
    double barrier_decay  = 0.1;    // mu *= decay each outer iteration
    int    barrier_iters  = 5;      // inner iterations per barrier level
    double aug_rho        = 10.0;   // augmented Lagrangian penalty
    double aug_rho_scale  = 2.0;    // rho *= scale each outer iteration
    int    aug_iters      = 5;      // inner iterations per augmented step
};

struct SQPResult {
    std::vector<double> x;
    double              f;
    int                 iterations;
    bool                converged;
};

using ObjectiveFn   = std::function<double(const std::vector<double>&)>;
using GradientFn    = std::function<std::vector<double>(const std::vector<double>&)>;
using IneqFn        = std::function<std::vector<double>(const std::vector<double>&)>;
using EqFn          = std::function<std::vector<double>(const std::vector<double>&)>;

// Minimize f(x) with optional gradient, box constraints, inequality and equality constraints.
// If grad_fn is nullptr, a central-difference numerical gradient is used.
// Pass empty vectors for lb/ub to indicate no box constraints (±∞).
// Pass nullptr for ineq_fn / eq_fn to skip those constraint types.
SQPResult minimize(
    ObjectiveFn          obj_fn,
    GradientFn           grad_fn,       // may be nullptr
    const std::vector<double>& x0,
    const std::vector<double>& lb,      // may be empty
    const std::vector<double>& ub,      // may be empty
    IneqFn               ineq_fn,       // may be nullptr; g(x) <= 0
    EqFn                 eq_fn,         // may be nullptr; h(x) == 0
    const SQPOptions&    opts = SQPOptions{}
);

}  // namespace optimizer
}  // namespace jcolour
