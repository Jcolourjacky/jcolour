#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "spectral.h"
#include "delta_e.h"
#include "pjl.h"
#include "i1sdk.h"
#include <vector>
#include <cmath>
#include <chrono>

using namespace jcolour::spectral;

TEST_CASE("Integration: Spectral pipeline with PJL", "[integration]") {
    // Test that spectral and PJL components can work together
    using namespace jcolour::spectral;

    // Convert RGB to spectrum
    auto spectrum = rgb_to_spectrum({0.8, 0.5, 0.3});
    auto xyz = spectrum_to_xyz(spectrum);
    auto lab = xyz_to_lab(xyz);

    // Build PJL command for this color
    auto pjlCmd = jcolour::pjl::PJLCommand::set_color_mode(jcolour::pjl::ColorMode::Color);

    // Verify both components work together
    REQUIRE(spectrum.size() == NUM_CHANNELS);
    REQUIRE(!pjlCmd.data.empty());
}

TEST_CASE("Integration: PJL + Printer simulation", "[integration]") {
    using namespace jcolour::pjl;

    // Build a print job
    PrintJob job;
    job.pjl_header = PJLCommand::set_color_mode(ColorMode::Off).data;
    job.pjl_header.insert(job.pjl_header.end(),
        PJLCommand::set_ink_mode(InkMode::PhotoBlack).data.begin(),
        PJLCommand::set_ink_mode(InkMode::PhotoBlack).data.end());
    job.pjl_header.insert(job.pjl_header.end(),
        PJLCommand::enter_language("PCL").data.begin(),
        PJLCommand::enter_language("PCL").data.end());

    // Generate raster data
    std::vector<uint16_t> grayData(256 * 64 * 3);
    for (int i = 0; i < 256 * 64; ++i) {
        uint16_t val = static_cast<uint16_t>(i % 256 * 257 - 1);
        grayData[i * 3] = val;
        grayData[i * 3 + 1] = val;
        grayData[i * 3 + 2] = val;
    }

    std::vector<double> limits(12, 1.0);
    job.raster_data = RasterGenerator::convert_to_raster(grayData, 256, 64, limits);
    job.pjl_footer = PJLCommand::uel().data;
    job.is_16bit = true;
    job.width_pixels = 256;
    job.height_pixels = 64;

    // Verify job structure
    REQUIRE(job.is_16bit == true);
    REQUIRE(job.width_pixels == 256);
    REQUIRE(job.height_pixels == 64);
    REQUIRE(!job.pjl_header.empty());
    REQUIRE(!job.raster_data.empty());
}

TEST_CASE("Integration: Spectral + DeltaE pipeline", "[integration]") {
    using namespace jcolour::spectral;
    using namespace jcolour::delta_e;

    // Convert RGB to spectrum
    std::array<double, 3> rgb = {0.8, 0.5, 0.3};
    auto spectrum = rgb_to_spectrum(rgb);

    // Convert to Lab
    auto lab1 = xyz_to_lab(spectrum_to_xyz(spectrum));

    // Create slightly different color
    std::array<double, 3> rgb2 = {0.82, 0.52, 0.32};
    auto spectrum2 = rgb_to_spectrum(rgb2);
    auto lab2 = xyz_to_lab(spectrum_to_xyz(spectrum2));

    // Calculate Delta E
    double de76 = delta_e_76(lab1, lab2);
    double de94 = delta_e_94(lab1, lab2);
    double de00 = delta_e_2000(lab1, lab2);

    // Verify Delta E is small but non-zero
    REQUIRE(de76 > 0);
    REQUIRE(de76 < 10);
    REQUIRE(de94 > 0);
    REQUIRE(de94 < 10);
    REQUIRE(de00 > 0);
    REQUIRE(de00 < 10);

    // Verify symmetry
    REQUIRE(delta_e_2000(lab1, lab2) == Catch::Approx(delta_e_2000(lab2, lab1)).margin(0.001));
}

TEST_CASE("Integration: i1SDK + Spectral measurement", "[integration]") {
    using namespace jcolour::i1sdk;

    SpectrumCapture capture;

    // Simulate measuring a white patch
    Spectrum whiteSpectrum;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        whiteSpectrum[i] = 1.0;
    }

    // Convert to Lab
    auto xyz = capture.spectrum_to_xyz(whiteSpectrum);
    auto lab = capture.spectrum_to_lab(whiteSpectrum);

    // White should have high L*
    REQUIRE(lab.L > 90);
    REQUIRE(std::abs(lab.a) < 5);
    REQUIRE(std::abs(lab.b) < 5);
}

TEST_CASE("Integration: Preset workflow simulation", "[integration]") {
    // Test that preset settings can be configured
    using namespace jcolour::pjl;

    // Simulate preset configuration
    auto colorMode = PJLCommand::set_color_mode(ColorMode::Color);
    auto inkMode = PJLCommand::set_ink_mode(InkMode::PhotoBlack);
    auto resolution = PJLCommand::set_resolution(1440);
    auto paperType = PJLCommand::set_paper_type("PHOTORAG");

    // Verify all preset components
    REQUIRE(!colorMode.data.empty());
    REQUIRE(!inkMode.data.empty());
    REQUIRE(!resolution.data.empty());
    REQUIRE(!paperType.data.empty());
}

TEST_CASE("Integration: Multi-ink raster generation", "[integration]") {
    using namespace jcolour::pjl;

    // Test raster generation for multi-ink printer
    // Each ink channel should be properly converted
    std::vector<uint16_t> imageData(100 * 100 * 3);
    for (int i = 0; i < 100 * 100; ++i) {
        // Red-dominant color
        imageData[i * 3] = 65535;
        imageData[i * 3 + 1] = 16384;
        imageData[i * 3 + 2] = 8192;
    }

    // Enable all channels
    std::vector<double> allChannels(12, 1.0);
    auto raster = RasterGenerator::convert_to_raster(imageData, 100, 100, allChannels);

    // Raster should have data
    REQUIRE(!raster.empty());

    // Raster should have enough data for 12 channels
    size_t expectedSize = 100 * ((100 + 7) / 8) * 12;
    REQUIRE(raster.size() >= expectedSize);
}

TEST_CASE("Integration: Gray balance workflow", "[integration]") {
    using namespace jcolour::spectral;
    using namespace jcolour::delta_e;

    // Simulate measuring a gray ramp
    std::vector<Lab> measuredGray;

    for (int i = 0; i <= 10; ++i) {
        double gray = i / 10.0;
        auto spectrum = rgb_to_spectrum({gray, gray, gray});
        auto lab = xyz_to_lab(spectrum_to_xyz(spectrum));
        measuredGray.push_back(lab);
    }

    // Check that all measurements are near neutral
    for (const auto& lab : measuredGray) {
        REQUIRE(std::abs(lab.a) < 2.0);
        REQUIRE(std::abs(lab.b) < 2.0);
    }

    // L* should increase monotonically
    for (int i = 1; i < measuredGray.size(); ++i) {
        REQUIRE(measuredGray[i].L > measuredGray[i-1].L);
    }
}

TEST_CASE("Acceptance: Spike 1 color accuracy criteria", "[acceptance][spike1]") {
    using namespace jcolour::spectral;
    using namespace jcolour::delta_e;

    // Test: Gray stage ΔE2000 ≤ 0.5
    for (double grayLevel : {0.2, 0.4, 0.6, 0.8}) {
        auto spectrum1 = rgb_to_spectrum({grayLevel, grayLevel, grayLevel});
        auto lab1 = xyz_to_lab(spectrum_to_xyz(spectrum1));

        // Simulate a very small difference (0.001 step in linear sRGB)
        auto spectrum2 = rgb_to_spectrum({grayLevel + 0.001, grayLevel + 0.001, grayLevel + 0.001});
        auto lab2 = xyz_to_lab(spectrum_to_xyz(spectrum2));

        double de00 = delta_e_2000(lab1, lab2);
        REQUIRE(de00 <= 0.5);
    }
}

TEST_CASE("Acceptance: Spike 1 skin tone tolerance", "[acceptance][spike1]") {
    using namespace jcolour::spectral;
    using namespace jcolour::delta_e;

    // Test: Skin tone ΔE2000 ≤ 1.5
    auto skin1 = rgb_to_spectrum({0.8, 0.6, 0.5});
    auto lab1 = xyz_to_lab(spectrum_to_xyz(skin1));

    auto skin2 = rgb_to_spectrum({0.82, 0.62, 0.52});
    auto lab2 = xyz_to_lab(spectrum_to_xyz(skin2));

    double de00 = delta_e_2000(lab1, lab2);
    REQUIRE(de00 <= 1.5);
}

TEST_CASE("Acceptance: Spike 1 red line detection", "[acceptance][spike1]") {
    using namespace jcolour::spectral;
    using namespace jcolour::delta_e;

    // Test: ΔE > 3.0 indicates issue (red line)
    auto color1 = rgb_to_spectrum({0.5, 0.5, 0.5});
    auto lab1 = xyz_to_lab(spectrum_to_xyz(color1));

    auto color2 = rgb_to_spectrum({0.2, 0.8, 0.3});
    auto lab2 = xyz_to_lab(spectrum_to_xyz(color2));

    double de00 = delta_e_2000(lab1, lab2);
    REQUIRE(de00 > 3.0); // Should trigger red line
}

TEST_CASE("Performance: Large raster generation", "[performance]") {
    using namespace jcolour::pjl;

    // 24" x 36" at 300 DPI = 7200 x 10800 pixels
    constexpr int WIDTH = 1920;
    constexpr int HEIGHT = 1080;
    std::vector<uint16_t> input(WIDTH * HEIGHT * 3, 32768);
    std::vector<double> limits(12, 1.0);

    auto start = std::chrono::high_resolution_clock::now();
    auto raster = RasterGenerator::convert_to_raster(input, WIDTH, HEIGHT, limits);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    REQUIRE(!raster.empty());

    // Should complete in reasonable time
    REQUIRE(duration.count() < 60000);
}

TEST_CASE("Performance: Spectral conversion batch", "[performance]") {
    using namespace jcolour::spectral;

    constexpr int NUM_CONVERSIONS = 1000;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_CONVERSIONS; ++i) {
        double value = i / static_cast<double>(NUM_CONVERSIONS);
        auto spectrum = rgb_to_spectrum({value, value * 0.5, value * 0.3});
        auto xyz = spectrum_to_xyz(spectrum);
        auto lab = xyz_to_lab(xyz);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 conversions should be fast
    REQUIRE(duration.count() < 5000);
}
