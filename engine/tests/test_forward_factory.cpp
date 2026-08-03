#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "forward_factory.h"
#include <sstream>

using namespace jcolour::forward;
using namespace jcolour::spectral;

static Spectrum flat(double v) { Spectrum s; s.fill(v); return s; }

TEST_CASE("factory: make_km roundtrip substrate-only", "[factory]") {
    Spectrum sub = flat(0.8);
    auto json = serialize_km(sub, {});
    auto model = make_km(json);
    REQUIRE(model->num_inks() == 0);
    auto r = model->predict({});
    for (int i = 0; i < NUM_CHANNELS; ++i)
        REQUIRE(r[i] == Catch::Approx(0.8).margin(0.005));
}

TEST_CASE("factory: make_km roundtrip with two inks", "[factory]") {
    Spectrum sub = flat(0.9);
    Spectrum ink0 = flat(0.2);
    Spectrum ink1 = flat(0.1);
    auto json = serialize_km(sub, {ink0, ink1});

    auto model = make_km(json);
    REQUIRE(model->num_inks() == 2);

    // predict at zero coverage = substrate
    auto r = model->predict({0.0, 0.0});
    for (int i = 0; i < NUM_CHANNELS; ++i)
        REQUIRE(r[i] == Catch::Approx(0.9).margin(0.005));
}

TEST_CASE("factory: make_km predict matches direct construction", "[factory]") {
    Spectrum sub = flat(0.85);
    Spectrum ink = flat(0.15);
    auto json = serialize_km(sub, {ink});

    auto from_json = make_km(json);

    KMForwardModel direct(sub);
    direct.add_ink(ink);

    for (double c : {0.0, 0.3, 0.7, 1.0}) {
        auto r1 = from_json->predict({c});
        auto r2 = direct.predict({c});
        for (int i = 0; i < NUM_CHANNELS; ++i)
            REQUIRE(r1[i] == Catch::Approx(r2[i]).margin(1e-9));
    }
}

TEST_CASE("factory: make_cynsn N=1 roundtrip", "[factory]") {
    // Build a CellularYNSNModel, serialize manually, parse back
    Spectrum sub = flat(0.8);
    Spectrum ink_half = flat(0.45);
    Spectrum ink_full = flat(0.15);

    // Build JSON manually for N=1, divisions=2
    std::ostringstream os;
    os << "{\"model\":\"cynsn\",\"n_inks\":1,\"divisions\":2,\"yn_n\":2.0,\"grid\":{"
       << "\"0\":" << serialize_spectrum(sub) << ","
       << "\"1\":" << serialize_spectrum(ink_half) << ","
       << "\"2\":" << serialize_spectrum(ink_full)
       << "}}";

    auto model = make_cynsn(os.str());
    REQUIRE(model->num_inks() == 1);
    REQUIRE(model->divisions() == 2);

    // At grid corners, prediction should match the set spectra
    auto r0 = model->predict({0.0});
    auto r1 = model->predict({0.5});
    auto r2 = model->predict({1.0});

    for (int i = 0; i < NUM_CHANNELS; ++i) {
        REQUIRE(r0[i] == Catch::Approx(sub[i]).margin(1e-9));
        REQUIRE(r1[i] == Catch::Approx(ink_half[i]).margin(1e-9));
        REQUIRE(r2[i] == Catch::Approx(ink_full[i]).margin(1e-9));
    }
}

TEST_CASE("factory: make_km missing key throws", "[factory]") {
    REQUIRE_THROWS_AS(make_km("{\"inks\":[]}"), std::invalid_argument);
    REQUIRE_THROWS_AS(make_km("{\"substrate\":[]}"), std::invalid_argument);
}

TEST_CASE("factory: parse_spectrum wrong size throws", "[factory]") {
    REQUIRE_THROWS_AS(parse_spectrum("[1,2,3]"), std::invalid_argument);
}
