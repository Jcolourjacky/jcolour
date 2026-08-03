#include "sqp.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace jcolour {
namespace optimizer {

// ---- vector helpers ----

static double dot(const std::vector<double>& a, const std::vector<double>& b) {
    double s = 0.0;
    for (size_t i = 0; i < a.size(); ++i) s += a[i] * b[i];
    return s;
}

static double norm(const std::vector<double>& v) {
    return std::sqrt(dot(v, v));
}

static std::vector<double> add(const std::vector<double>& a, const std::vector<double>& b, double sb = 1.0) {
    std::vector<double> r(a.size());
    for (size_t i = 0; i < a.size(); ++i) r[i] = a[i] + sb * b[i];
    return r;
}

static std::vector<double> scale(const std::vector<double>& v, double s) {
    std::vector<double> r(v.size());
    for (size_t i = 0; i < v.size(); ++i) r[i] = v[i] * s;
    return r;
}

// Project x into box [lb, ub]; empty lb/ub means unconstrained.
static std::vector<double> project_box(const std::vector<double>& x,
                                        const std::vector<double>& lb,
                                        const std::vector<double>& ub) {
    std::vector<double> r = x;
    for (size_t i = 0; i < x.size(); ++i) {
        if (!lb.empty()) r[i] = std::max(r[i], lb[i]);
        if (!ub.empty()) r[i] = std::min(r[i], ub[i]);
    }
    return r;
}

// Projected gradient: zero out components where x is at a bound and gradient points out.
static std::vector<double> projected_gradient(const std::vector<double>& g,
                                               const std::vector<double>& x,
                                               const std::vector<double>& lb,
                                               const std::vector<double>& ub) {
    std::vector<double> pg = g;
    for (size_t i = 0; i < x.size(); ++i) {
        bool at_lb = !lb.empty() && x[i] <= lb[i] + 1e-14;
        bool at_ub = !ub.empty() && x[i] >= ub[i] - 1e-14;
        if (at_lb && pg[i] > 0.0) pg[i] = 0.0;
        if (at_ub && pg[i] < 0.0) pg[i] = 0.0;
    }
    return pg;
}

// ---- numerical gradient (central difference) ----

static std::vector<double> numerical_gradient(ObjectiveFn fn,
                                               const std::vector<double>& x,
                                               double h = 1e-5) {
    int n = static_cast<int>(x.size());
    std::vector<double> g(n);
    std::vector<double> xp = x, xm = x;
    for (int i = 0; i < n; ++i) {
        xp[i] = x[i] + h;
        xm[i] = x[i] - h;
        g[i] = (fn(xp) - fn(xm)) / (2.0 * h);
        xp[i] = x[i];
        xm[i] = x[i];
    }
    return g;
}

// ---- Wolfe-condition line search ----

static double wolfe_line_search(ObjectiveFn fn,
                                 const std::vector<double>& x,
                                 const std::vector<double>& d,
                                 const std::vector<double>& g,
                                 const std::vector<double>& lb,
                                 const std::vector<double>& ub,
                                 double f0,
                                 double c1 = 1e-4,
                                 double c2 = 0.9) {
    double alpha = 1.0;
    double dg0 = dot(d, g);  // directional derivative at alpha=0
    if (dg0 >= 0.0) return 0.0;  // not a descent direction

    double alpha_lo = 0.0, alpha_hi = std::numeric_limits<double>::infinity();
    for (int iter = 0; iter < 50; ++iter) {
        auto xnew = project_box(add(x, d, alpha), lb, ub);
        double fnew = fn(xnew);

        if (fnew > f0 + c1 * alpha * dg0 || (iter > 0 && fnew >= fn(project_box(add(x, d, alpha_lo), lb, ub)))) {
            alpha_hi = alpha;
        } else {
            // Check curvature (strong Wolfe)
            // Approximate directional derivative at new point via finite diff
            double eps = 1e-7 * alpha;
            auto xnew2 = project_box(add(x, d, alpha + eps), lb, ub);
            double dg_new = (fn(xnew2) - fnew) / eps;
            if (std::abs(dg_new) <= -c2 * dg0) return alpha;
            if (dg_new >= 0.0) alpha_hi = alpha;
            else alpha_lo = alpha;
        }

        if (std::isinf(alpha_hi))
            alpha *= 2.0;
        else
            alpha = 0.5 * (alpha_lo + alpha_hi);

        if (alpha < 1e-14) break;
    }
    return alpha;
}

// ---- BFGS update ----
// H is stored as a flat n×n row-major matrix.

static void bfgs_update(std::vector<double>& H,
                         const std::vector<double>& s,   // x_new - x_old
                         const std::vector<double>& y) { // g_new - g_old
    int n = static_cast<int>(s.size());
    double sy = dot(s, y);
    if (sy < 1e-10 * norm(s) * norm(y)) return;  // skip degenerate update

    double rho = 1.0 / sy;

    // Hy
    std::vector<double> Hy(n, 0.0);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            Hy[i] += H[i * n + j] * y[j];

    double yHy = dot(y, Hy);

    // H += rho*(1 + rho*yHy)*s*s^T - rho*(s*Hy^T + Hy*s^T)
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            H[i * n + j] += rho * (1.0 + rho * yHy) * s[i] * s[j]
                           - rho * (s[i] * Hy[j] + Hy[i] * s[j]);
        }
    }
}

// H * v
static std::vector<double> hv(const std::vector<double>& H,
                                const std::vector<double>& v) {
    int n = static_cast<int>(v.size());
    std::vector<double> r(n, 0.0);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            r[i] += H[i * n + j] * v[j];
    return r;
}

// ---- inner BFGS loop ----

static std::vector<double> bfgs_minimize(ObjectiveFn fn,
                                          GradientFn  gfn,
                                          std::vector<double> x,
                                          const std::vector<double>& lb,
                                          const std::vector<double>& ub,
                                          int max_iter,
                                          double grad_tol,
                                          double step_tol,
                                          int& total_iters) {
    int n = static_cast<int>(x.size());

    // Identity Hessian approximation
    std::vector<double> H(n * n, 0.0);
    for (int i = 0; i < n; ++i) H[i * n + i] = 1.0;

    auto grad = gfn ? gfn(x) : numerical_gradient(fn, x);
    double f = fn(x);

    for (int iter = 0; iter < max_iter; ++iter) {
        ++total_iters;

        auto pg = projected_gradient(grad, x, lb, ub);
        if (norm(pg) < grad_tol) break;

        // Newton direction: d = -H * grad
        auto d = scale(hv(H, grad), -1.0);

        // Project direction to respect active bounds
        for (int i = 0; i < n; ++i) {
            bool at_lb = !lb.empty() && x[i] <= lb[i] + 1e-14;
            bool at_ub = !ub.empty() && x[i] >= ub[i] - 1e-14;
            if ((at_lb && d[i] < 0.0) || (at_ub && d[i] > 0.0)) d[i] = 0.0;
        }

        if (norm(d) < step_tol) break;

        double alpha = wolfe_line_search(fn, x, d, grad, lb, ub, f);
        if (alpha < step_tol) break;

        auto x_new = project_box(add(x, d, alpha), lb, ub);
        auto g_new = gfn ? gfn(x_new) : numerical_gradient(fn, x_new);

        auto s = add(x_new, x, -1.0);
        auto y = add(g_new, grad, -1.0);
        bfgs_update(H, s, y);

        x    = x_new;
        grad = g_new;
        f    = fn(x);
    }
    return x;
}

// ---- main minimize ----

SQPResult minimize(ObjectiveFn          obj_fn,
                   GradientFn           grad_fn,
                   const std::vector<double>& x0,
                   const std::vector<double>& lb,
                   const std::vector<double>& ub,
                   IneqFn               ineq_fn,
                   EqFn                 eq_fn,
                   const SQPOptions&    opts) {
    int n = static_cast<int>(x0.size());
    auto x = project_box(x0, lb, ub);
    int total_iters = 0;

    // Lagrange multipliers for equality constraints
    std::vector<double> lambda;
    if (eq_fn) {
        auto h0 = eq_fn(x);
        lambda.assign(h0.size(), 0.0);
    }

    // Outer loop: barrier decay + augmented Lagrangian update
    double mu  = opts.barrier_mu;
    double rho = opts.aug_rho;

    int outer_iters = (ineq_fn || eq_fn) ? opts.barrier_iters : 1;

    for (int outer = 0; outer < outer_iters; ++outer) {
        // Build augmented objective
        double mu_cap  = mu;
        double rho_cap = rho;
        std::vector<double> lam_cap = lambda;

        auto augmented = [&](const std::vector<double>& xv) -> double {
            double val = obj_fn(xv);

            // Log-barrier for inequality constraints g(x) <= 0
            if (ineq_fn) {
                auto g = ineq_fn(xv);
                for (double gi : g) {
                    if (gi >= 0.0) return 1e30;  // infeasible
                    val -= mu_cap * std::log(-gi);
                }
            }

            // Augmented Lagrangian for equality constraints h(x) == 0
            if (eq_fn) {
                auto h = eq_fn(xv);
                for (size_t k = 0; k < h.size(); ++k) {
                    val += lam_cap[k] * h[k] + 0.5 * rho_cap * h[k] * h[k];
                }
            }
            return val;
        };

        x = bfgs_minimize(augmented, nullptr, x, lb, ub,
                          opts.max_iter, opts.grad_tol, opts.step_tol,
                          total_iters);

        // Update multipliers
        if (eq_fn) {
            auto h = eq_fn(x);
            for (size_t k = 0; k < lambda.size(); ++k)
                lambda[k] += rho * h[k];
        }

        mu  *= opts.barrier_decay;
        rho *= opts.aug_rho_scale;
    }

    // Final polish with original objective (no barrier)
    if (!ineq_fn && !eq_fn) {
        // Already done above in single pass
    } else {
        // One more BFGS pass with tiny barrier
        double mu_final = mu * 0.01;
        std::vector<double> lam_final = lambda;
        double rho_final = rho;

        auto final_obj = [&](const std::vector<double>& xv) -> double {
            double val = obj_fn(xv);
            if (ineq_fn) {
                auto g = ineq_fn(xv);
                for (double gi : g) {
                    if (gi >= 0.0) return 1e30;
                    val -= mu_final * std::log(-gi);
                }
            }
            if (eq_fn) {
                auto h = eq_fn(xv);
                for (size_t k = 0; k < lam_final.size(); ++k)
                    val += lam_final[k] * h[k] + 0.5 * rho_final * h[k] * h[k];
            }
            return val;
        };

        x = bfgs_minimize(final_obj, nullptr, x, lb, ub,
                          opts.max_iter, opts.grad_tol, opts.step_tol,
                          total_iters);
    }

    // Check convergence
    auto g_final = grad_fn ? grad_fn(x) : numerical_gradient(obj_fn, x);
    auto pg = projected_gradient(g_final, x, lb, ub);
    bool converged = norm(pg) < opts.grad_tol * 10.0;

    return SQPResult{x, obj_fn(x), total_iters, converged};
}

}  // namespace optimizer
}  // namespace jcolour
