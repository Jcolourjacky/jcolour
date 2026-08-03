#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "constraints.h"
#include "sqp.h"
#include "km.h"
#include <cmath>

using namespace jcolour::optimizer;
using namespace jcolour::spectral;
using jcolour::forward::KMForwardModel;

static Spectrum flat(double v) { Spectrum s; s.fill(v); return s; }

TEST_CASE("constraints: TILConstraint satisfied when sum <= limit", "[constraints]") {
    TILConstraint til(2.0);
    auto g = til({0.4, 0.5, 0.6, 0.4});  // sum = 1.9 < 2.0
    REQUIRE(g.size() == 1);
    REQUIRE(g[0] < 0.0);
}

TEST_CASE("constraints: TILConstraint violated when sum > limit", "[constraints]") {
    TILConstraint til(2.0);
    auto g = til({0.6, 0.7, 0.8, 0.5});  // sum = 2.6 > 2.0
    REQUIRE(g.size() == 1);
    REQUIRE(g[0] > 0.0);
}

TEST_CASE("constraints: TILConstraint zero at exact limit", "[constraints]") {
    TILConstraint til(2.0);
    auto g = til({0.5, 0.5, 0.5, 0.5});  // sum = 2.0
    REQUIRE(g[0] == Catch::Approx(0.0).margin(1e-12));
}

TEST_CASE("constraints: LStarMatch residual zero at target", "[constraints]") {
    // Build a KM model with known substrate
    Spectrum sub = flat(0.5);
    auto model = std::make_unique<KMForwardModel>(sub);

    // Predict at zero coverage = substrate
    auto predicted = model->predict({});
    auto xyz = spectrum_to_xyz(predicted, Illuminant::D50());
    auto lab = xyz_to_lab(xyz);

    LStarMatch lm([&](const std::vector<double>& c) { return model->predict(c); },
                  lab.L);

    auto h = lm({});
    REQUIRE(h.size() == 1);
    REQUIRE(h[0] == Catch::Approx(0.0).margin(1e-6));
}

TEST_CASE("constraints: LStarMatch residual nonzero off target", "[constraints]") {
    Spectrum sub = flat(0.5);
    auto model = std::make_unique<KMForwardModel>(sub);
    model->add_ink(flat(0.1));

    LStarMatch lm([&](const std::vector<double>& c) { return model->predict(c); },
                  80.0);  // arbitrary target L*

    auto h = lm({0.0});
    REQUIRE(std::abs(h[0]) > 0.1);
}

TEST_CASE("constraints: LabMatch residual zero at target", "[constraints]") {
    Spectrum sub = flat(0.6);
    auto model = std::make_unique<KMForwardModel>(sub);

    auto predicted = model->predict({});
    auto xyz = spectrum_to_xyz(predicted, Illuminant::D50());
    auto lab = xyz_to_lab(xyz);

    LabMatch lm([&](const std::vector<double>& c) { return model->predict(c); }, lab);

    auto h = lm({});
    REQUIRE(h.size() == 3);
    REQUIRE(h[0] == Catch::Approx(0.0).margin(1e-6));
    REQUIRE(h[1] == Catch::Approx(0.0).margin(1e-6));
    REQUIRE(h[2] == Catch::Approx(0.0).margin(1e-6));
}

TEST_CASE("constraints: 4D box + TIL inequality converges", "[constraints]") {
    // Minimize sum((x[i]-0.8)^2) in [0,1]^4 with TIL <= 2.0
    // Unconstrained min at (0.8,0.8,0.8,0.8), sum=3.2 > 2.0
    // Constrained min: each x[i] = 0.5 (sum=2.0, symmetric)
    auto fn = [](const std::vector<double>& x) {
        double s = 0.0;
        for (double xi : x) s += (xi - 0.8) * (xi - 0.8);
        return s;
    };

    TILConstraint til(2.0);
    auto ineq = [&](const std::vector<double>& x) { return til(x); };

    SQPOptions opts;
    opts.barrier_mu    = 0.1;
    opts.barrier_decay = 0.05;
    opts.barrier_iters = 10;
    opts.max_iter      = 150;

    auto res = minimize(fn, nullptr,
                        {0.5, 0.5, 0.5, 0.5},
                        {0.0, 0.0, 0.0, 0.0},
                        {1.0, 1.0, 1.0, 1.0},
                        ineq, nullptr, opts);

    double total = 0.0;
    for (double xi : res.x) total += xi;
    REQUIRE(total <= 2.0 + 0.02);  // feasible (small tolerance for barrier)
    for (double xi : res.x)
        REQUIRE(xi == Catch::Approx(0.5).margin(0.02));
}

TEST_CASE("constraints: 4D box + LStarMatch equality converges", "[constraints]") {
    // Use a KM model with 4 inks; find coverages that match a target L*
    Spectrum sub = flat(0.9);
    auto model = std::make_unique<KMForwardModel>(sub);
    model->add_ink(flat(0.3));
    model->add_ink(flat(0.4));
    model->add_ink(flat(0.5));
    model->add_ink(flat(0.6));

    // Target: predict at (0.2, 0.2, 0.2, 0.2)
    auto target_spec = model->predict({0.2, 0.2, 0.2, 0.2});
    auto target_xyz  = spectrum_to_xyz(target_spec, Illuminant::D50());
    auto target_lab  = xyz_to_lab(target_xyz);

    LStarMatch lm([&](const std::vector<double>& c) { return model->predict(c); },
                  target_lab.L);

    auto eq = [&](const std::vector<double>& x) { return lm(x); };

    // Minimize GrainCost subject to L* == target_lab.L
    auto fn = [](const std::vector<double>& x) {
        double s = 0.0;
        for (double xi : x) s += xi * xi;
        return s;
    };

    SQPOptions opts;
    opts.aug_rho       = 20.0;
    opts.aug_rho_scale = 3.0;
    opts.barrier_iters = 8;
    opts.max_iter      = 150;

    auto res = minimize(fn, nullptr,
                        {0.3, 0.3, 0.3, 0.3},
                        {0.0, 0.0, 0.0, 0.0},
                        {1.0, 1.0, 1.0, 1.0},
                        nullptr, eq, opts);

    // Check L* constraint is satisfied
    auto pred_spec = model->predict(res.x);
    auto pred_xyz  = spectrum_to_xyz(pred_spec, Illuminant::D50());
    auto pred_lab  = xyz_to_lab(pred_xyz);
    REQUIRE(pred_lab.L == Catch::Approx(target_lab.L).margin(1.0));
}
