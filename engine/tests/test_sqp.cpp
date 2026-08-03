#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "sqp.h"
#include <cmath>

using namespace jcolour::optimizer;

// Rosenbrock: f(x,y) = (1-x)^2 + 100*(y-x^2)^2, minimum at (1,1)
static double rosenbrock(const std::vector<double>& x) {
    double a = 1.0 - x[0];
    double b = x[1] - x[0] * x[0];
    return a * a + 100.0 * b * b;
}

static std::vector<double> rosenbrock_grad(const std::vector<double>& x) {
    return {
        -2.0 * (1.0 - x[0]) - 400.0 * x[0] * (x[1] - x[0] * x[0]),
         200.0 * (x[1] - x[0] * x[0])
    };
}

TEST_CASE("sqp: Rosenbrock unconstrained converges < 50 iter", "[sqp]") {
    SQPOptions opts;
    opts.max_iter = 200;
    opts.grad_tol = 1e-6;

    auto res = minimize(rosenbrock, rosenbrock_grad, {-1.2, 1.0}, {}, {}, nullptr, nullptr, opts);

    REQUIRE(res.converged);
    REQUIRE(res.iterations < 50);
    REQUIRE(res.x[0] == Catch::Approx(1.0).margin(1e-4));
    REQUIRE(res.x[1] == Catch::Approx(1.0).margin(1e-4));
    REQUIRE(res.f    == Catch::Approx(0.0).margin(1e-8));
}

TEST_CASE("sqp: Rosenbrock numerical gradient converges", "[sqp]") {
    SQPOptions opts;
    opts.max_iter = 200;
    opts.grad_tol = 1e-5;

    auto res = minimize(rosenbrock, nullptr, {-1.2, 1.0}, {}, {}, nullptr, nullptr, opts);

    REQUIRE(res.converged);
    REQUIRE(res.x[0] == Catch::Approx(1.0).margin(1e-3));
    REQUIRE(res.x[1] == Catch::Approx(1.0).margin(1e-3));
}

TEST_CASE("sqp: box-constrained minimum at boundary", "[sqp]") {
    // f(x) = (x-3)^2, box [0,2] => minimum at x=2
    auto fn = [](const std::vector<double>& x) { return (x[0]-3.0)*(x[0]-3.0); };
    auto gn = [](const std::vector<double>& x) -> std::vector<double> { return {2.0*(x[0]-3.0)}; };

    auto res = minimize(fn, gn, {0.5}, {0.0}, {2.0}, nullptr, nullptr);

    REQUIRE(res.converged);
    REQUIRE(res.x[0] == Catch::Approx(2.0).margin(1e-6));
}

TEST_CASE("sqp: box-constrained minimum in interior", "[sqp]") {
    // f(x) = (x-1)^2, box [0,2] => minimum at x=1
    auto fn = [](const std::vector<double>& x) { return (x[0]-1.0)*(x[0]-1.0); };
    auto gn = [](const std::vector<double>& x) -> std::vector<double> { return {2.0*(x[0]-1.0)}; };

    auto res = minimize(fn, gn, {0.5}, {0.0}, {2.0}, nullptr, nullptr);

    REQUIRE(res.converged);
    REQUIRE(res.x[0] == Catch::Approx(1.0).margin(1e-6));
}

TEST_CASE("sqp: inequality constraint g(x) <= 0", "[sqp]") {
    // Minimize f(x,y) = x^2 + y^2 subject to x + y >= 1 (i.e. -(x+y-1) <= 0)
    // Unconstrained min is (0,0); constrained min is (0.5, 0.5)
    auto fn = [](const std::vector<double>& x) { return x[0]*x[0] + x[1]*x[1]; };
    auto ineq = [](const std::vector<double>& x) -> std::vector<double> {
        return {1.0 - x[0] - x[1] - 1e-6};  // small slack to stay strictly feasible
    };

    SQPOptions opts;
    opts.barrier_mu    = 0.1;
    opts.barrier_decay = 0.05;
    opts.barrier_iters = 8;
    opts.max_iter      = 100;

    auto res = minimize(fn, nullptr, {1.0, 1.0}, {}, {}, ineq, nullptr, opts);

    REQUIRE(res.x[0] + res.x[1] == Catch::Approx(1.0).margin(0.02));
    REQUIRE(res.x[0] == Catch::Approx(0.5).margin(0.02));
    REQUIRE(res.x[1] == Catch::Approx(0.5).margin(0.02));
}

TEST_CASE("sqp: equality constraint h(x) == 0", "[sqp]") {
    // Minimize f(x,y) = x^2 + y^2 subject to x + y = 1
    // Minimum at (0.5, 0.5)
    auto fn = [](const std::vector<double>& x) { return x[0]*x[0] + x[1]*x[1]; };
    auto eq = [](const std::vector<double>& x) -> std::vector<double> {
        return {x[0] + x[1] - 1.0};
    };

    SQPOptions opts;
    opts.aug_rho       = 10.0;
    opts.aug_rho_scale = 3.0;
    opts.aug_iters     = 6;
    opts.barrier_iters = 6;
    opts.max_iter      = 100;

    auto res = minimize(fn, nullptr, {0.8, 0.8}, {}, {}, nullptr, eq, opts);

    REQUIRE(res.x[0] == Catch::Approx(0.5).margin(0.01));
    REQUIRE(res.x[1] == Catch::Approx(0.5).margin(0.01));
}

TEST_CASE("sqp: 4D box-constrained quadratic", "[sqp]") {
    // f(x) = sum((x[i] - target[i])^2), box [0,1]^4
    // target = {0.3, 0.7, 0.5, 0.9} — all in box, so unconstrained min is feasible
    std::vector<double> target = {0.3, 0.7, 0.5, 0.9};
    auto fn = [&](const std::vector<double>& x) {
        double s = 0.0;
        for (int i = 0; i < 4; ++i) s += (x[i]-target[i])*(x[i]-target[i]);
        return s;
    };

    auto res = minimize(fn, nullptr, {0.5, 0.5, 0.5, 0.5},
                        {0.0, 0.0, 0.0, 0.0}, {1.0, 1.0, 1.0, 1.0},
                        nullptr, nullptr);

    REQUIRE(res.converged);
    for (int i = 0; i < 4; ++i)
        REQUIRE(res.x[i] == Catch::Approx(target[i]).margin(1e-5));
}
