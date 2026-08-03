#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "cost.h"
#include <cmath>

using namespace jcolour::optimizer;
using namespace jcolour::spectral;

static Spectrum flat(double v) { Spectrum s; s.fill(v); return s; }

TEST_CASE("cost: MetamerismCost zero for identical spectra", "[cost]") {
    Spectrum t = flat(0.5);
    MetamerismCost c(t);
    REQUIRE(c(t, {0.5}) == Catch::Approx(0.0).margin(1e-9));
}

TEST_CASE("cost: MetamerismCost positive for different spectra", "[cost]") {
    Spectrum t = flat(0.5);
    Spectrum p = flat(0.3);
    MetamerismCost c(t);
    REQUIRE(c(p, {0.3}) > 0.0);
}

TEST_CASE("cost: MetamerismCost custom illuminants", "[cost]") {
    Spectrum t = flat(0.5);
    Spectrum p = flat(0.4);
    MetamerismCost c(t, {Illuminant::D50(), Illuminant::D65()});
    double val = c(p, {0.4});
    REQUIRE(val > 0.0);
}

TEST_CASE("cost: GrainCost zero for zero coverages", "[cost]") {
    GrainCost c;
    REQUIRE(c(flat(0.5), {0.0, 0.0, 0.0}) == Catch::Approx(0.0).margin(1e-12));
}

TEST_CASE("cost: GrainCost increases with coverage", "[cost]") {
    GrainCost c;
    double low  = c(flat(0.5), {0.1, 0.1});
    double high = c(flat(0.5), {0.8, 0.8});
    REQUIRE(high > low);
}

TEST_CASE("cost: GrainCost value matches sum of squares", "[cost]") {
    GrainCost c;
    std::vector<double> cov = {0.3, 0.5, 0.7};
    double expected = 0.3*0.3 + 0.5*0.5 + 0.7*0.7;
    REQUIRE(c(flat(0.5), cov) == Catch::Approx(expected).margin(1e-12));
}

TEST_CASE("cost: SmoothnessCost zero for uniform coverages", "[cost]") {
    SmoothnessCost c;
    REQUIRE(c(flat(0.5), {0.4, 0.4, 0.4, 0.4}) == Catch::Approx(0.0).margin(1e-12));
}

TEST_CASE("cost: SmoothnessCost positive for non-uniform coverages", "[cost]") {
    SmoothnessCost c;
    REQUIRE(c(flat(0.5), {0.1, 0.9, 0.1, 0.9}) > 0.0);
}

TEST_CASE("cost: SmoothnessCost value matches sum of squared diffs", "[cost]") {
    SmoothnessCost c;
    std::vector<double> cov = {0.1, 0.4, 0.3, 0.8};
    double expected = (0.4-0.1)*(0.4-0.1) + (0.3-0.4)*(0.3-0.4) + (0.8-0.3)*(0.8-0.3);
    REQUIRE(c(flat(0.5), cov) == Catch::Approx(expected).margin(1e-12));
}

TEST_CASE("cost: TotalInkCost zero for zero coverages", "[cost]") {
    TotalInkCost c;
    REQUIRE(c(flat(0.5), {0.0, 0.0, 0.0}) == Catch::Approx(0.0).margin(1e-12));
}

TEST_CASE("cost: TotalInkCost increases with total ink", "[cost]") {
    TotalInkCost c;
    double low  = c(flat(0.5), {0.1, 0.1, 0.1, 0.1});
    double high = c(flat(0.5), {0.9, 0.9, 0.9, 0.9});
    REQUIRE(high > low);
}

TEST_CASE("cost: TotalInkCost value is (mean coverage)^2", "[cost]") {
    TotalInkCost c;
    std::vector<double> cov = {0.2, 0.4, 0.6};
    double mean = (0.2 + 0.4 + 0.6) / 3.0;
    REQUIRE(c(flat(0.5), cov) == Catch::Approx(mean * mean).margin(1e-12));
}

TEST_CASE("cost: WeightedCost combines terms correctly", "[cost]") {
    GrainCost grain;
    TotalInkCost til;
    WeightedCost wc;
    wc.add(grain, 2.0);
    wc.add(til,   3.0);

    std::vector<double> cov = {0.4, 0.6};
    Spectrum pred = flat(0.5);

    double expected = 2.0 * grain(pred, cov) + 3.0 * til(pred, cov);
    REQUIRE(wc(pred, cov) == Catch::Approx(expected).margin(1e-12));
}

TEST_CASE("cost: WeightedCost zero weight term has no effect", "[cost]") {
    GrainCost grain;
    SmoothnessCost smooth;
    WeightedCost wc;
    wc.add(grain,  1.0);
    wc.add(smooth, 0.0);

    std::vector<double> cov = {0.3, 0.7};
    Spectrum pred = flat(0.5);

    REQUIRE(wc(pred, cov) == Catch::Approx(grain(pred, cov)).margin(1e-12));
}
