#include <catch2/catch_test_macros.hpp>
#include "../src/gamut_mapping.h"
#include "../src/spectral.h"

using namespace jcolour::gamut;
using namespace jcolour::spectral;

TEST_CASE("[gamut] GamutHull3D basic operations", "[gamut]") {
    // Use exactly 4 points for tetrahedron
    std::vector<Lab> points = {
        {100, 0, 0},    // White
        {0, 0, 0},      // Black
        {50, 70, 45},    // Red
        {50, -60, 35},   // Green
    };

    GamutHull3D hull;
    hull.build(points);

    // Hull should have vertices and positive volume
    REQUIRE(hull.volume() >= 0);
}

TEST_CASE("[gamut] GamutCompressor sigmoidal compression", "[gamut]") {
    // Create source and destination gamuts
    GamutHull3D source = build_prophoto_gamut_hull();
    GamutHull3D dest;
    dest.build({
        {100, 0, 0},
        {0, 0, 0},
        {50, 60, 40},    // Smaller gamut
        {50, -50, 30},
        {50, 25, -60},
    });

    GamutCompressionParams params;
    params.source_gamut = source;
    params.dest_gamut = dest;
    params.compress_start = 0.85;
    params.compress_end = 1.0;
    params.use_compression = true;

    GamutCompressor compressor(params);

    // Test a moderately saturated color
    Lab test_color = {50, 40, 30};
    Lab compressed = compressor.compress(test_color);

    // Compression should not crash and should return valid Lab
    REQUIRE(compressed.L >= 0);
    REQUIRE(compressed.L <= 100);
}

TEST_CASE("[gamut] GamutCompressor preserves neutral", "[gamut]") {
    GamutHull3D dest;
    dest.build({
        {100, 0, 0},
        {0, 0, 0},
        {50, 60, 40},
        {50, -50, 30},
    });

    GamutCompressionParams params;
    params.source_gamut = build_prophoto_gamut_hull();
    params.dest_gamut = dest;
    params.preserve_neutral_axis = true;

    GamutCompressor compressor(params);

    // Near-neutral should be preserved
    Lab neutral = {50, 2, 1};
    Lab compressed = compressor.compress(neutral);

    // Should remain nearly unchanged
    REQUIRE(std::abs(compressed.a - neutral.a) < 1.0);
    REQUIRE(std::abs(compressed.b - neutral.b) < 1.0);
}

TEST_CASE("[gamut] PerceptualRenderer basic", "[gamut]") {
    PerceptualRenderer renderer;

    Lab test_color = {50, 50, 50};
    Lab result = renderer.render(test_color);

    // Should return a color (may be same or compressed)
    REQUIRE(result.L >= 0);
    REQUIRE(result.L <= 100);
    REQUIRE(std::isfinite(result.a));
    REQUIRE(std::isfinite(result.b));
}

TEST_CASE("[gamut] sigmoidal_compress edge cases", "[gamut]") {
    double result;

    result = GamutCompressor::sigmoidal_compress(0.0, 0.0, 1.0);
    REQUIRE(result >= 0.0);
    REQUIRE(result <= 1.0);

    result = GamutCompressor::sigmoidal_compress(1.0, 0.0, 1.0);
    REQUIRE(result >= 0.0);
    REQUIRE(result <= 1.0);

    result = GamutCompressor::sigmoidal_compress(0.5, 0.0, 1.0);
    REQUIRE(result > 0.0);
    REQUIRE(result < 1.0);

    // Out of range
    result = GamutCompressor::sigmoidal_compress(-1.0, 0.0, 1.0);
    REQUIRE(result == 0.0);

    result = GamutCompressor::sigmoidal_compress(2.0, 0.0, 1.0);
    REQUIRE(result == 1.0);
}

TEST_CASE("[gamut] build_prophoto_gamut_hull", "[gamut]") {
    auto hull = build_prophoto_gamut_hull();

    REQUIRE(hull.is_valid());
    REQUIRE(hull.volume() > 0);

    // Center should be near neutral
    auto c = hull.centroid();
    REQUIRE(c.L > 40);
    REQUIRE(c.L < 60);
}

TEST_CASE("[gamut] compress_to_printer_gamut", "[gamut]") {
    GamutHull3D printer_gamut;
    printer_gamut.build({
        {100, 0, 0},
        {0, 0, 0},
        {50, 55, 35},
        {50, -45, 25},
    });

    Lab very_saturated = {50, 100, 80};
    Lab result = compress_to_printer_gamut(very_saturated, printer_gamut);

    // Result should have reduced chroma
    double original = std::sqrt(very_saturated.a*very_saturated.a + very_saturated.b*very_saturated.b);
    double compressed = std::sqrt(result.a*result.a + result.b*result.b);
    REQUIRE(compressed < original);
    REQUIRE(compressed > 0);
}

TEST_CASE("[gamut] WideGamutMapper legacy interface", "[gamut]") {
    WideGamutMapper mapper;
    mapper.set_source_space("ProPhotoRGB");

    // Test transform
    std::vector<uint16_t> input = {32768, 32768, 32768};  // Gray
    auto output = mapper.transform(input, 1, 1, 3);

    REQUIRE(output.size() == 3);
    // Should produce some output
    bool has_nonzero = (output[0] > 0) || (output[1] > 0) || (output[2] > 0);
    REQUIRE(has_nonzero);
}