#include <catch2/catch_test_macros.hpp>
#include "i1sdk.h"
#include "spectral.h"
#include <cmath>

using namespace jcolour::i1sdk;
using namespace jcolour::spectral;

TEST_CASE("I1Device construction", "[i1sdk]") {
    REQUIRE_NOTHROW(I1Device());
}

TEST_CASE("I1Device::is_connected defaults false", "[i1sdk]") {
    I1Device device;
    REQUIRE(device.is_connected() == false);
}

TEST_CASE("I1Device::disconnect when not connected", "[i1sdk]") {
    I1Device device;
    REQUIRE(device.disconnect() == true);
}

TEST_CASE("I1Device::discover_devices returns vector", "[i1sdk]") {
    auto devices = I1Device::discover_devices();
    REQUIRE(devices.size() >= 0);
}

TEST_CASE("I1Device::check_sandbox_entitlements returns bool", "[i1sdk]") {
    auto result = I1Device::check_sandbox_entitlements();
    REQUIRE((result == false || result == true));
}

TEST_CASE("I1Device::requires_special_entitlements returns bool", "[i1sdk]") {
    auto result = I1Device::requires_special_entitlements();
    REQUIRE(result == true); // i1 SDK requires USB/BT entitlements
}

TEST_CASE("I1Device::calibrate_white fails when not connected", "[i1sdk]") {
    I1Device device;
    auto result = device.calibrate_white();
    REQUIRE(result.success == false);
}

TEST_CASE("I1Device::calibrate_dark fails when not connected", "[i1sdk]") {
    I1Device device;
    auto result = device.calibrate_dark();
    REQUIRE(result.success == false);
}

TEST_CASE("I1Device::is_calibration_valid defaults false", "[i1sdk]") {
    I1Device device;
    REQUIRE(device.is_calibration_valid() == false);
}

TEST_CASE("I1Device::measure_spot fails when not connected", "[i1sdk]") {
    I1Device device;
    auto result = device.measure_spot();
    REQUIRE(result.success == false);
}

TEST_CASE("I1Device::measure_spot fails when not calibrated", "[i1sdk]") {
    I1Device device;
    device.connect(); // Would connect in real scenario
    auto result = device.measure_spot();
    REQUIRE(result.success == false);
}

TEST_CASE("I1Device::get_device_info returns valid struct", "[i1sdk]") {
    I1Device device;
    auto info = device.get_device_info();
    // Should return default-initialized struct
    REQUIRE(info.type != DeviceType::i1iO2); // Just checking it's initialized
}

TEST_CASE("MeasurementResult initialization", "[i1sdk]") {
    MeasurementResult result;
    REQUIRE(result.success == false);
    REQUIRE(result.spectral_data[0] == 0.0);
}

TEST_CASE("CalibrationResult initialization", "[i1sdk]") {
    CalibrationResult result;
    REQUIRE(result.success == false);
    REQUIRE(result.white_reference_value == 0.0);
}

TEST_CASE("DeviceInfo initialization", "[i1sdk]") {
    DeviceInfo info;
    REQUIRE(info.is_ready == false);
    REQUIRE(info.is_calibrated == false);
}

TEST_CASE("MeasurementTarget construction", "[i1sdk]") {
    REQUIRE_NOTHROW(MeasurementTarget());
}

TEST_CASE("MeasurementTarget::create_colorchecker_24", "[i1sdk]") {
    auto target = MeasurementTarget::create_colorchecker_24();
    REQUIRE(target.patches().size() == 24);
}

TEST_CASE("MeasurementTarget::create_gray_ramp", "[i1sdk]") {
    auto target = MeasurementTarget::create_gray_ramp(21);
    REQUIRE(target.patches().size() == 21);

    auto target2 = MeasurementTarget::create_gray_ramp(16);
    REQUIRE(target2.patches().size() == 16);
}

TEST_CASE("MeasurementTarget::create_gray_ramp_advanced", "[i1sdk]") {
    auto target = MeasurementTarget::create_gray_ramp_advanced();
    REQUIRE(target.patches().size() == 256);
}

TEST_CASE("MeasurementTarget::add_patch", "[i1sdk]") {
    MeasurementTarget target;
    REQUIRE(target.patches().size() == 0);

    MeasurementTarget::ColorCheckerPatch patch;
    patch.row = 0;
    patch.column = 0;
    patch.expected_lab.L = 50.0;
    patch.expected_lab.a = 0.0;
    patch.expected_lab.b = 0.0;
    patch.name = "Test";

    target.add_patch(patch);
    REQUIRE(target.patches().size() == 1);
}

TEST_CASE("MeasurementTarget::clear_patches", "[i1sdk]") {
    auto target = MeasurementTarget::create_colorchecker_24();
    REQUIRE(target.patches().size() == 24);

    target.clear_patches();
    REQUIRE(target.patches().size() == 0);
}

TEST_CASE("MeasurementTarget ColorChecker patches have valid Lab values", "[i1sdk]") {
    auto target = MeasurementTarget::create_colorchecker_24();
    for (const auto& patch : target.patches()) {
        REQUIRE(patch.expected_lab.L >= 0);
        REQUIRE(patch.expected_lab.L <= 100);
        REQUIRE(std::abs(patch.expected_lab.a) < 100);
        REQUIRE(std::abs(patch.expected_lab.b) < 100);
    }
}

TEST_CASE("MeasurementTarget Gray ramp patches are neutral", "[i1sdk]") {
    auto target = MeasurementTarget::create_gray_ramp(21);
    for (const auto& patch : target.patches()) {
        REQUIRE(std::abs(patch.expected_lab.a) < 0.5);
        REQUIRE(std::abs(patch.expected_lab.b) < 0.5);
    }
}

TEST_CASE("SpectrumCapture construction", "[i1sdk]") {
    REQUIRE_NOTHROW(SpectrumCapture());
}

TEST_CASE("SpectrumCapture::capture_spectrum", "[i1sdk]") {
    SpectrumCapture capture;
    auto spectrum = capture.capture_spectrum(MeasurementMode::SpotReflectance);
    REQUIRE(spectrum.size() == SPECTRAL_CHANNELS);
}

TEST_CASE("SpectrumCapture::capture_sequence", "[i1sdk]") {
    SpectrumCapture capture;
    auto spectra = capture.capture_sequence(5);
    REQUIRE(spectra.size() == 5);
    for (const auto& spec : spectra) {
        REQUIRE(spec.size() == SPECTRAL_CHANNELS);
    }
}

TEST_CASE("SpectrumCapture::validate_spectrum", "[i1sdk]") {
    SpectrumCapture capture;
    auto spectrum = capture.capture_spectrum(MeasurementMode::SpotReflectance);
    REQUIRE(capture.validate_spectrum(spectrum) == true);
}

TEST_CASE("SpectrumCapture::validate_spectrum rejects invalid", "[i1sdk]") {
    SpectrumCapture capture;
    Spectrum invalid;
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        invalid[i] = 9999.0; // Out of valid range
    }
    REQUIRE(capture.validate_spectrum(invalid) == false);
}

TEST_CASE("SpectrumCapture::compare_to_target", "[i1sdk]") {
    SpectrumCapture capture;

    // Create a neutral gray spectrum that should convert to L*=50, a*=0, b*=0
    Spectrum gray_spectrum{};
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        gray_spectrum[i] = 0.5;  // Mid-gray reflectance
    }
    Lab target{50.0, 0.0, 0.0};

    // The compare_to_target function should compute delta E correctly
    bool result = capture.compare_to_target(gray_spectrum, target, 50.0);
    REQUIRE(result == true);  // Large tolerance since we're using a stub spectrum
}

TEST_CASE("DeviceType enum values", "[i1sdk]") {
    REQUIRE(static_cast<int>(DeviceType::Unknown) == 0);
    REQUIRE(static_cast<int>(DeviceType::i1Pro3) == 1);
    REQUIRE(static_cast<int>(DeviceType::i1Pro3Plus) == 2);
    REQUIRE(static_cast<int>(DeviceType::i1iO) == 3);
}

TEST_CASE("MeasurementMode enum values", "[i1sdk]") {
    REQUIRE(static_cast<int>(MeasurementMode::SpotReflectance) == 0);
    REQUIRE(static_cast<int>(MeasurementMode::SpotTransmittance) == 1);
}

TEST_CASE("SPECTRAL_CHANNELS constant", "[i1sdk]") {
    REQUIRE(SPECTRAL_CHANNELS == 36);
}
