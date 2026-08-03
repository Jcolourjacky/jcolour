#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

#include "stp_backend.h"

using jcolour::stp::StpJobRequest;
using jcolour::stp::parse_manifest;
using jcolour::stp::write_manifest;

TEST_CASE("parse_manifest: reads all keys", "[stp]") {
    std::istringstream in(
        "# a comment\n"
        "model=escp2-p9000\n"
        "input=/tmp/in.rgb\n"
        "output=/tmp/out.prn\n"
        "width=1000\n"
        "height=1500\n"
        "dpi=1440\n"
        "media=Premium Luster Photo Paper\n"
        "pagesize=A4\n"
        "resolution=1440x1440dpi\n"
        "quality=4\n"
        "ink_limit=0.85\n");

    StpJobRequest req;
    std::string err;
    REQUIRE(parse_manifest(in, req, err));
    REQUIRE(err.empty());

    REQUIRE(req.model == "escp2-p9000");
    REQUIRE(req.input == "/tmp/in.rgb");
    REQUIRE(req.output == "/tmp/out.prn");
    REQUIRE(req.width_px == 1000);
    REQUIRE(req.height_px == 1500);
    REQUIRE(req.dpi == 1440);
    REQUIRE(req.media == "Premium Luster Photo Paper");
    REQUIRE(req.pagesize == "A4");
    REQUIRE(req.resolution == "1440x1440dpi");
    REQUIRE(req.quality == 4);
    REQUIRE(req.ink_limit == 0.85);
}

TEST_CASE("parse_manifest: blanks and comments are skipped", "[stp]") {
    std::istringstream in("\n  \n# only comments\n\twidth=42\n");
    StpJobRequest req;
    std::string err;
    REQUIRE(parse_manifest(in, req, err));
    REQUIRE(req.width_px == 42);
}

TEST_CASE("parse_manifest: unknown keys are ignored", "[stp]") {
    std::istringstream in("future_key=whatever\nwidth=10\n");
    StpJobRequest req;
    std::string err;
    REQUIRE(parse_manifest(in, req, err));
    REQUIRE(req.width_px == 10);
}

TEST_CASE("parse_manifest: malformed line fails", "[stp]") {
    std::istringstream in("this has no equals sign\n");
    StpJobRequest req;
    std::string err;
    REQUIRE_FALSE(parse_manifest(in, req, err));
    REQUIRE_FALSE(err.empty());
}

TEST_CASE("parse_manifest: bad number fails", "[stp]") {
    std::istringstream in("width=not_a_number\n");
    StpJobRequest req;
    std::string err;
    REQUIRE_FALSE(parse_manifest(in, req, err));
    REQUIRE(err.find("width") != std::string::npos);
}

TEST_CASE("write_manifest -> parse_manifest round-trips", "[stp]") {
    StpJobRequest in;
    in.model = "escp2-p9000";
    in.input = "/data/ramp.rgb";
    in.output = "/data/ramp.prn";
    in.width_px = 2048;
    in.height_px = 4096;
    in.dpi = 2880;
    in.media = "Epson Premium Luster";
    in.pagesize = "A3";
    in.resolution = "2880x1440dpi";
    in.quality = 5;
    in.ink_limit = 0.9;

    std::ostringstream out;
    write_manifest(out, in);

    std::istringstream back(out.str());
    StpJobRequest got;
    std::string err;
    REQUIRE(parse_manifest(back, got, err));

    REQUIRE(got.model == in.model);
    REQUIRE(got.input == in.input);
    REQUIRE(got.output == in.output);
    REQUIRE(got.width_px == in.width_px);
    REQUIRE(got.height_px == in.height_px);
    REQUIRE(got.dpi == in.dpi);
    REQUIRE(got.media == in.media);
    REQUIRE(got.pagesize == in.pagesize);
    REQUIRE(got.resolution == in.resolution);
    REQUIRE(got.quality == in.quality);
    REQUIRE(got.ink_limit == in.ink_limit);
}
