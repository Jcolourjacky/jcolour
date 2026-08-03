#include "jacobian.h"
#include <cmath>
#include <limits>

namespace jcolour {
namespace optimizer {

static double default_step(DiffMode mode) {
    double eps = std::numeric_limits<double>::epsilon();
    return (mode == DiffMode::Forward) ? std::sqrt(eps) : std::cbrt(eps);
}

// Adaptive step: scale by |x[i]| + 1 to handle large-magnitude variables.
static double step_for(double x_i, double h_base) {
    return h_base * (std::abs(x_i) + 1.0);
}

std::vector<double> jacobian(VectorFn fn,
                              const std::vector<double>& x,
                              DiffMode mode,
                              double step_hint) {
    int n = static_cast<int>(x.size());
    double h_base = (step_hint > 0.0) ? step_hint : default_step(mode);

    auto f0 = (mode == DiffMode::Forward) ? fn(x) : std::vector<double>{};
    int m = (mode == DiffMode::Forward) ? static_cast<int>(f0.size())
                                        : static_cast<int>(fn(x).size());

    std::vector<double> J(m * n);
    std::vector<double> xp = x, xm = x;

    for (int j = 0; j < n; ++j) {
        double h = step_for(x[j], h_base);

        if (mode == DiffMode::Forward) {
            xp[j] = x[j] + h;
            auto fp = fn(xp);
            for (int i = 0; i < m; ++i)
                J[i * n + j] = (fp[i] - f0[i]) / h;
            xp[j] = x[j];
        } else {
            xp[j] = x[j] + h;
            xm[j] = x[j] - h;
            auto fp = fn(xp);
            auto fm = fn(xm);
            for (int i = 0; i < m; ++i)
                J[i * n + j] = (fp[i] - fm[i]) / (2.0 * h);
            xp[j] = x[j];
            xm[j] = x[j];
        }
    }
    return J;
}

std::vector<double> gradient(ScalarFn fn,
                              const std::vector<double>& x,
                              DiffMode mode,
                              double step_hint) {
    int n = static_cast<int>(x.size());
    double h_base = (step_hint > 0.0) ? step_hint : default_step(mode);

    double f0 = (mode == DiffMode::Forward) ? fn(x) : 0.0;

    std::vector<double> g(n);
    std::vector<double> xp = x, xm = x;

    for (int j = 0; j < n; ++j) {
        double h = step_for(x[j], h_base);

        if (mode == DiffMode::Forward) {
            xp[j] = x[j] + h;
            g[j] = (fn(xp) - f0) / h;
            xp[j] = x[j];
        } else {
            xp[j] = x[j] + h;
            xm[j] = x[j] - h;
            g[j] = (fn(xp) - fn(xm)) / (2.0 * h);
            xp[j] = x[j];
            xm[j] = x[j];
        }
    }
    return g;
}

}  // namespace optimizer
}  // namespace jcolour
