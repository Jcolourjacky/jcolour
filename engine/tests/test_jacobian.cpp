#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "jacobian.h"
#include <cmath>

using namespace jcolour::optimizer;

// f(x,y) = [x^2 + y, sin(x)*y]
// J = [[2x, 1], [cos(x)*y, sin(x)]]
static std::vector<double> fn_vec(const std::vector<double>& x) {
    return {x[0]*x[0] + x[1], std::sin(x[0])*x[1]};
}

static std::vector<double> analytic_jacobian(const std::vector<double>& x) {
    return {
        2.0*x[0], 1.0,
        std::cos(x[0])*x[1], std::sin(x[0])
    };
}

// f(x,y,z) = x^3 + 2*y^2 - z
// grad = [3x^2, 4y, -1]
static double fn_scalar(const std::vector<double>& x) {
    return x[0]*x[0]*x[0] + 2.0*x[1]*x[1] - x[2];
}

static std::vector<double> analytic_gradient(const std::vector<double>& x) {
    return {3.0*x[0]*x[0], 4.0*x[1], -1.0};
}

TEST_CASE("jacobian: central difference vs analytic < 1e-5", "[jacobian]") {
    std::vector<double> x = {1.5, 0.7};
    auto J_num = jacobian(fn_vec, x, DiffMode::Central);
    auto J_ref = analytic_jacobian(x);

    REQUIRE(J_num.size() == 4);
    for (int i = 0; i < 4; ++i)
        REQUIRE(J_num[i] == Catch::Approx(J_ref[i]).margin(1e-5));
}

TEST_CASE("jacobian: forward difference vs analytic < 1e-5", "[jacobian]") {
    std::vector<double> x = {1.5, 0.7};
    auto J_num = jacobian(fn_vec, x, DiffMode::Forward);
    auto J_ref = analytic_jacobian(x);

    REQUIRE(J_num.size() == 4);
    for (int i = 0; i < 4; ++i)
        REQUIRE(J_num[i] == Catch::Approx(J_ref[i]).margin(1e-5));
}

TEST_CASE("jacobian: central gradient vs analytic < 1e-5", "[jacobian]") {
    std::vector<double> x = {2.0, -1.5, 3.0};
    auto g_num = gradient(fn_scalar, x, DiffMode::Central);
    auto g_ref = analytic_gradient(x);

    REQUIRE(g_num.size() == 3);
    for (int i = 0; i < 3; ++i)
        REQUIRE(g_num[i] == Catch::Approx(g_ref[i]).margin(1e-5));
}

TEST_CASE("jacobian: forward gradient vs analytic < 1e-5", "[jacobian]") {
    std::vector<double> x = {2.0, -1.5, 3.0};
    auto g_num = gradient(fn_scalar, x, DiffMode::Forward);
    auto g_ref = analytic_gradient(x);

    REQUIRE(g_num.size() == 3);
    for (int i = 0; i < 3; ++i)
        REQUIRE(g_num[i] == Catch::Approx(g_ref[i]).margin(1e-5));
}

TEST_CASE("jacobian: adaptive step handles large-magnitude x", "[jacobian]") {
    // x = 1e6 — step must scale with |x| to avoid cancellation
    std::vector<double> x = {1e6};
    // f(x) = x^2, grad = 2x = 2e6
    auto g = gradient([](const std::vector<double>& v){ return v[0]*v[0]; },
                      x, DiffMode::Central);
    REQUIRE(g[0] == Catch::Approx(2e6).epsilon(1e-5));
}

TEST_CASE("jacobian: central is more accurate than forward", "[jacobian]") {
    // f(x) = sin(x), f'(x) = cos(x)
    std::vector<double> x = {1.0};
    double ref = std::cos(1.0);

    auto g_fwd = gradient([](const std::vector<double>& v){ return std::sin(v[0]); },
                           x, DiffMode::Forward);
    auto g_cen = gradient([](const std::vector<double>& v){ return std::sin(v[0]); },
                           x, DiffMode::Central);

    double err_fwd = std::abs(g_fwd[0] - ref);
    double err_cen = std::abs(g_cen[0] - ref);

    REQUIRE(err_cen < err_fwd);
    REQUIRE(err_cen < 1e-10);
}
