#pragma once

// Numerical Jacobian and gradient computation.
//
// Provides forward-difference and central-difference approximations
// with adaptive step size selection.

#include <functional>
#include <vector>

namespace jcolour {
namespace optimizer {

using VectorFn = std::function<std::vector<double>(const std::vector<double>&)>;
using ScalarFn = std::function<double(const std::vector<double>&)>;

enum class DiffMode {
    Forward,   // O(h)  — 1 extra eval per variable
    Central,   // O(h²) — 2 extra evals per variable
};

// Compute the Jacobian of a vector-valued function f: R^n -> R^m.
// Returns a (m x n) matrix stored row-major: J[i*n + j] = df_i/dx_j.
// step_hint: base step size; 0 = auto-select (sqrt(eps_machine) for forward,
//            cbrt(eps_machine) for central).
std::vector<double> jacobian(VectorFn        fn,
                              const std::vector<double>& x,
                              DiffMode        mode      = DiffMode::Central,
                              double          step_hint = 0.0);

// Compute the gradient of a scalar function f: R^n -> R.
std::vector<double> gradient(ScalarFn        fn,
                              const std::vector<double>& x,
                              DiffMode        mode      = DiffMode::Central,
                              double          step_hint = 0.0);

}  // namespace optimizer
}  // namespace jcolour
