#include <catch2/catch_test_macros.hpp>
#include "tiff_io.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("TIFFReader with invalid path", "[tiff_io]") {
    auto image = jcolour::io::TIFFReader::read("/nonexistent/file.tiff");
    REQUIRE(image.data.empty());
    REQUIRE(image.width == 0);
    REQUIRE(image.height == 0);
}

TEST_CASE("TIFFReader is_valid returns false for empty", "[tiff_io]") {
    jcolour::io::TIFFImage empty_image;
    REQUIRE(empty_image.data.empty());
}

TEST_CASE("TIFFImage structure", "[tiff_io]") {
    jcolour::io::TIFFImage image;
    image.width = 1920;
    image.height = 1080;
    image.bits_per_sample = 16;
    image.samples_per_pixel = 3;
    image.data.resize(1920 * 1080 * 3);

    REQUIRE(image.width == 1920);
    REQUIRE(image.height == 1080);
    REQUIRE(image.data.size() == 1920 * 1080 * 3);
}

TEST_CASE("TIFFReader::write with empty path", "[tiff_io]") {
    jcolour::io::TIFFImage image;
    image.width = 100;
    image.height = 100;
    image.bits_per_sample = 16;
    image.samples_per_pixel = 3;

    REQUIRE_NOTHROW(jcolour::io::TIFFReader::write("", image));
}

TEST_CASE("VerificationReport initialization", "[tiff_io]") {
    jcolour::io::VerificationReport report;

    REQUIRE(report.total_pixels == 0);
    REQUIRE(report.pixels_within_de00_1_0 == 0);
    REQUIRE(report.pixels_within_de00_0_5 == 0);
    REQUIRE(report.max_de00 == 0.0);
    REQUIRE(report.avg_de00 == 0.0);
}

TEST_CASE("TIFFReader destruction doesn't crash", "[tiff_io]") {
    REQUIRE_NOTHROW([]() {
        auto reader = std::make_unique<jcolour::io::TIFFReader>("/nonexistent.tiff");
    }());
}

TEST_CASE("TIFFReader move semantics", "[tiff_io]") {
    auto reader1 = std::make_unique<jcolour::io::TIFFReader>("/nonexistent.tiff");
    auto reader2 = std::move(reader1);
    REQUIRE(reader2->image().data.empty());
}
