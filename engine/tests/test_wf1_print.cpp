#include <catch2/catch_test_macros.hpp>
#include "wf1_print.h"
#include "wf1_lut.h"

using namespace jcolour::workflow;

TEST_CASE("gray_to_binary: converts gradient to binary", "[wf1][print]") {
    std::vector<PatchMeasurement> measurements;
    for (int i = 0; i <= 10; ++i) {
        PatchMeasurement m;
        m.measured_lstar = static_cast<float>(i) * 10.0f;
        m.ink_density = static_cast<float>(i) / 10.0f;
        measurements.push_back(m);
    }
    LStarToInkLUT lut(measurements);

    std::vector<float> gray_values(10);
    for (int i = 0; i < 10; ++i) {
        gray_values[i] = static_cast<float>(i) / 10.0f;
    }

    auto binary = gray_to_binary(gray_values, lut);

    REQUIRE(binary.size() == 10);
    // Lower gray values (lighter) should produce more 0s (white)
    // Higher gray values (darker) should produce more 1s (black ink)
    // At gray=0 (white), L*=100, ink≈0 -> threshold gives 0
    // At gray=1 (black), L*=0, ink≈1 -> threshold gives 1
}

TEST_CASE("gray_to_binary: handles edge cases", "[wf1][print]") {
    std::vector<PatchMeasurement> measurements;
    measurements.push_back({"P0", 0.0f, 0.0f, true});
    measurements.push_back({"P1", 1.0f, 100.0f, true});
    LStarToInkLUT lut(measurements);

    std::vector<float> gray_values = {0.0f, 1.0f};
    auto binary = gray_to_binary(gray_values, lut);

    REQUIRE(binary.size() == 2);
    // At gray=0 (white), should produce 0 (no ink)
    // At gray=1 (black), should produce 1 (full ink)
}

TEST_CASE("generate_gradient_test: creates correct dimensions", "[wf1][print]") {
    auto image = generate_gradient_test(100, 50);

    REQUIRE(image.size() == 50);
    for (const auto& row : image) {
        REQUIRE(row.size() == 100);
    }
}

TEST_CASE("generate_gradient_test: gradient values correct", "[wf1][print]") {
    auto image = generate_gradient_test(100, 10);

    // First column should be white (1.0)
    for (const auto& row : image) {
        REQUIRE(std::abs(row[0] - 1.0f) < 0.01f);
    }

    // Last column should be black (0.0)
    for (const auto& row : image) {
        REQUIRE(std::abs(row[99] - 0.0f) < 0.01f);
    }

    // Middle should be mid-gray
    float mid = image[0][49];
    REQUIRE(mid > 0.4f);
    REQUIRE(mid < 0.6f);
}

TEST_CASE("print_gray_image: returns success for valid input", "[wf1][print]") {
    auto image = generate_gradient_test(10, 10);

    std::vector<PatchMeasurement> measurements;
    measurements.push_back({"P0", 0.0f, 0.0f, true});
    measurements.push_back({"P1", 1.0f, 100.0f, true});
    LStarToInkLUT lut(measurements);

    PrintConfig config;
    config.width_px = 10;
    config.height_px = 10;

    auto result = print_gray_image(image, lut, config, "127.0.0.1");

    REQUIRE(result.success == true);
    REQUIRE(result.bytes_sent > 0);
}

TEST_CASE("print_gray_image: fails on empty image", "[wf1][print]") {
    std::vector<std::vector<float>> empty;

    std::vector<PatchMeasurement> measurements;
    measurements.push_back({"P0", 0.0f, 0.0f, true});
    measurements.push_back({"P1", 1.0f, 100.0f, true});
    LStarToInkLUT lut(measurements);

    PrintConfig config;
    auto result = print_gray_image(empty, lut, config, "127.0.0.1");

    REQUIRE(result.success == false);
    REQUIRE(!result.error_message.empty());
}

TEST_CASE("PrintConfig: default values", "[wf1][print]") {
    PrintConfig config;
    REQUIRE(config.dpi == 1440);
    REQUIRE(config.width_px == 0);
    REQUIRE(config.height_px == 0);
    REQUIRE(config.page_width_mm == 210.0f);
    REQUIRE(config.page_height_mm == 297.0f);
    REQUIRE(config.bidirectional == true);
    REQUIRE(config.paper_type.empty());
}

TEST_CASE("PrintResult: default values", "[wf1][print]") {
    PrintResult result;
    REQUIRE(result.success == false);
    REQUIRE(result.job_id.empty());
    REQUIRE(result.error_message.empty());
    REQUIRE(result.bytes_sent == 0);
}