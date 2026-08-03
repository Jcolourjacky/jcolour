#pragma once

#include "spectral.h"
#include <string>
#include <memory>
#include <functional>
#include <vector>

namespace jcolour {
namespace i1sdk {

constexpr int SPECTRAL_CHANNELS = 36;
constexpr double WAVELENGTH_START = 380.0;
constexpr double WAVELENGTH_END = 730.0;
constexpr double WAVELENGTH_STEP = 10.0;

enum class DeviceType {
    Unknown = 0,
    i1Pro3 = 1,
    i1Pro3Plus = 2,
    i1iO = 3,
    i1iO2 = 4
};

enum class MeasurementMode {
    SpotReflectance = 0,
    SpotTransmittance = 1,
    ScanReflectance = 2,
    ScanTransmittance = 3
};

// How the app talks to the instrument — user-selectable in 设置:
//   XRiteSdk : i1Pro3.framework loaded at runtime (i1pro3_driver.h)
//   Argyll   : spotread subprocess (i1Pro3 needs -N to skip the forced
//              white calibration; stray spotread is SIGKILLed to free USB)
//   Auto     : prefer the SDK when present, else Argyll, else simulation
enum class Backend { Auto = 0, XRiteSdk = 1, Argyll = 2 };

void        set_preferred_backend(Backend b);   // wired from app 设置
Backend     preferred_backend();
const char* backend_name(Backend b);

// Status snapshot of both integration paths, for the 设置 page.
struct BackendStatus {
    bool        sdk_available = false;     // framework loaded
    std::string sdk_version;
    std::string sdk_path;
    int         sdk_devices = 0;
    bool        argyll_available = false;  // spotread on disk
    std::string argyll_version;
    std::string argyll_spotread_path;
};
BackendStatus probe_backends();

struct MeasurementResult {
    bool success = false;
    std::string error_message;
    DeviceType device = DeviceType::Unknown;
    MeasurementMode mode = MeasurementMode::SpotReflectance;

    // 36-channel spectral data (380-730nm @ 10nm)
    spectral::Spectrum spectral_data = {};

    // Computed values
    double reflectance = 0.0;       // 0-1
    double X = 0.0, Y = 0.0, Z = 0.0;          // CIE XYZ
    spectral::Lab lab = {};       // CIE Lab

    // Metadata
    double opacity = 0.0;
    double gloss = 0.0;
    double temperature = 0.0;       // Celsius
    std::string timestamp;
};

struct CalibrationResult {
    bool success = false;
    std::string error_message;
    double white_reference_value = 0.0;
    double dark_reference_value = 0.0;
};

struct DeviceInfo {
    DeviceType type = DeviceType::Unknown;
    std::string serial_number;
    std::string firmware_version;
    bool is_calibrated = false;
    bool is_ready = false;
    std::string status_message;
    spectral::Spectrum spectral_data = {};
};

class I1Device {
public:
    using ProgressCallback = std::function<void(int percent, const char* message)>;

    I1Device();
    ~I1Device();

    // Connection
    bool connect();
    bool disconnect();
    bool is_connected() const;

    // Device discovery
    static std::vector<DeviceInfo> discover_devices();
    static DeviceType detect_device_type(const char* device_path);

    // Calibration
    CalibrationResult calibrate_white();
    CalibrationResult calibrate_dark();
    bool is_calibration_valid() const;

    // Measurement
    MeasurementResult measure_spot(MeasurementMode mode = MeasurementMode::SpotReflectance);
    MeasurementResult measure_sequence(
        int num_measurements,
        ProgressCallback callback = nullptr);

    // Device info
    DeviceInfo get_device_info() const;
    Backend    active_backend() const;   // which path this connection uses

    // Sandbox entitlements check
    static bool check_sandbox_entitlements();
    static bool requires_special_entitlements();

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

class MeasurementTarget {
public:
    struct ColorCheckerPatch {
        int row;
        int column;
        spectral::Lab expected_lab;
        std::string name;
    };

    MeasurementTarget();
    ~MeasurementTarget();

    void add_patch(const ColorCheckerPatch& patch);
    void clear_patches();

    static MeasurementTarget create_colorchecker_24();
    static MeasurementTarget create_gray_ramp(int num_patches = 21);
    static MeasurementTarget create_gray_ramp_advanced();

    const std::vector<ColorCheckerPatch>& patches() const { return patches_; }

private:
    std::vector<ColorCheckerPatch> patches_;
};

class SpectrumCapture {
public:
    SpectrumCapture();
    ~SpectrumCapture();

    // Capture from device
    spectral::Spectrum capture_spectrum(MeasurementMode mode);
    std::vector<spectral::Spectrum> capture_sequence(int count);

    // Strip/scanning mode for measuring printed charts (B10)
    struct StripScanOptions {
        int num_patches;           // Expected number of patches
        float patch_spacing_mm;    // Spacing between patches
        bool auto_detect_patches;  // Auto-detect patch boundaries
        int scan_speed;            // 1=normal, 2=slow for higher accuracy
        int integration_time_ms;   // Integration time

        StripScanOptions() :
            num_patches(21),
            patch_spacing_mm(10.0f),
            auto_detect_patches(true),
            scan_speed(1),
            integration_time_ms(100) {}
    };

    struct ScanResult {
        bool success = false;
        std::string error_message;
        std::vector<spectral::Spectrum> spectra;      // 36ch spectral data per patch
        std::vector<double> reflectance_values;        // Reflectance 0-1 per patch
        std::vector<spectral::Lab> lab_values;        // D50 Lab per patch
        int patches_scanned = 0;
    };

    // Scan a strip of color patches (main scanning function for B10)
    ScanResult scan_strip(const StripScanOptions& options = StripScanOptions());

    // Convert to other formats
    spectral::XYZ spectrum_to_xyz(const spectral::Spectrum& spectrum);
    spectral::Lab spectrum_to_lab(const spectral::Spectrum& spectrum);

    // Validation
    bool validate_spectrum(const spectral::Spectrum& spectrum);
    bool compare_to_target(const spectral::Spectrum& measured,
                         const spectral::Lab& target,
                         double max_delta_e);
};

}
}
