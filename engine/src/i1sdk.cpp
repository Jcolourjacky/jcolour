#include "i1sdk.h"
#include "i1pro3_driver.h"
#include "spectral.h"
#include "delta_e.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <iostream>
#include <algorithm>
#include <random>
#include <cstring>

#include <atomic>
#include <array>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>

namespace jcolour {
namespace i1sdk {

// ---------------------------------------------------------------------------
// Backend preference (set from app 设置, default Auto)
// ---------------------------------------------------------------------------

namespace {
std::atomic<int> g_preferred_backend{static_cast<int>(Backend::Auto)};
}

void set_preferred_backend(Backend b) { g_preferred_backend = static_cast<int>(b); }
Backend preferred_backend() { return static_cast<Backend>(g_preferred_backend.load()); }

const char* backend_name(Backend b) {
    switch (b) {
        case Backend::XRiteSdk: return "X-Rite SDK";
        case Backend::Argyll:   return "Argyll";
        default:                return "自动";
    }
}

// ---------------------------------------------------------------------------
// Argyll spotread backend (self-contained subprocess, no library dependency).
// i1Pro3 notes (field-proven): forced white calibration fails with "Wrong
// Sensor Position" → always run spotread with -N; stray spotread keeps the
// USB claim, so leftovers are SIGKILLed before/after each run.
// ---------------------------------------------------------------------------

namespace argyll_backend {

bool file_executable(const std::string& p) {
    struct stat st{};
    return ::stat(p.c_str(), &st) == 0 && (st.st_mode & S_IXUSR);
}

std::string find_spotread() {
    const char* home = std::getenv("HOME");
    std::vector<std::string> candidates = {
        "/opt/homebrew/bin/spotread", "/usr/local/bin/spotread"};
    if (home) candidates.push_back(std::string(home) + "/bin/spotread");
    if (const char* path = std::getenv("PATH")) {
        std::stringstream ss(path);
        std::string dir;
        while (std::getline(ss, dir, ':'))
            if (!dir.empty()) candidates.push_back(dir + "/spotread");
    }
    for (const auto& c : candidates)
        if (file_executable(c)) return c;
    return {};
}

void kill_stray_spotread() {
    // SIGKILL (not TERM) — spotread must die for the i1Pro USB claim to drop.
    ::system("pkill -9 -x spotread 2>/dev/null");
}

struct SpotResult {
    bool ok = false;
    std::string error;
    double L = 0, a = 0, b = 0;
    bool has_spectrum = false;
    std::array<double, 36> spectrum{};
};

// One spot measurement: spotread -N [-s], feed a trigger key + 'q', parse
// "Result is ... Lab: L a b" and the optional 36-step spectrum line.
SpotResult measure_spot(int timeout_sec = 45) {
    SpotResult r;
    const std::string tool = find_spotread();
    if (tool.empty()) { r.error = "spotread not found (install ArgyllCMS)"; return r; }

    kill_stray_spotread();

    int inPipe[2], outPipe[2];
    if (pipe(inPipe) != 0 || pipe(outPipe) != 0) { r.error = "pipe() failed"; return r; }
    const pid_t pid = fork();
    if (pid < 0) { r.error = "fork() failed"; return r; }
    if (pid == 0) {
        dup2(inPipe[0], STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);
        dup2(outPipe[1], STDERR_FILENO);
        close(inPipe[0]); close(inPipe[1]);
        close(outPipe[0]); close(outPipe[1]);
        execl(tool.c_str(), "spotread", "-N", "-s", static_cast<char*>(nullptr));
        _exit(127);
    }
    close(inPipe[0]);
    close(outPipe[1]);
    // trigger one reading, then quit
    (void)!write(inPipe[1], " q", 2);
    close(inPipe[1]);

    std::string out;
    char buf[4096];
    const auto deadline = time(nullptr) + timeout_sec;
    int status = 0;
    bool exited = false;
    while (time(nullptr) < deadline) {
        struct pollfd pfd{outPipe[0], POLLIN, 0};
        const int pr = poll(&pfd, 1, 250);
        if (pr > 0 && (pfd.revents & POLLIN)) {
            const ssize_t n = read(outPipe[0], buf, sizeof(buf));
            if (n > 0) out.append(buf, static_cast<size_t>(n));
            else break;                       // EOF
        }
        if (waitpid(pid, &status, WNOHANG) == pid) { exited = true; break; }
    }
    if (!exited) { kill(pid, SIGKILL); waitpid(pid, &status, 0); r.error = "spotread timeout"; }
    // drain remainder
    for (;;) {
        const ssize_t n = read(outPipe[0], buf, sizeof(buf));
        if (n <= 0) break;
        out.append(buf, static_cast<size_t>(n));
    }
    close(outPipe[0]);
    kill_stray_spotread();

    // parse "... Lab: 51.234 -2.345 1.234"
    const auto labPos = out.rfind("Lab:");
    if (labPos != std::string::npos) {
        std::stringstream ls(out.substr(labPos + 4));
        if (ls >> r.L >> r.a >> r.b) r.ok = true;
    }
    // parse spectrum block: "Spectrum from 380.0 to 730.0 nm in 36 steps" then values
    const auto spPos = out.find("Spectrum from");
    if (spPos != std::string::npos) {
        const auto nl = out.find('\n', spPos);
        if (nl != std::string::npos) {
            std::stringstream ss(out.substr(nl + 1));
            int i = 0;
            double v = 0;
            while (i < 36 && (ss >> v)) r.spectrum[static_cast<size_t>(i++)] = v / 100.0;
            r.has_spectrum = (i == 36);
        }
    }
    if (!r.ok && r.error.empty())
        r.error = "spotread produced no Lab result" +
                  (out.empty() ? std::string() : (": " + out.substr(0, 200)));
    return r;
}

std::string version() {
    const std::string tool = find_spotread();
    if (tool.empty()) return {};
    // `spotread -?` prints "Spotread, Version x.y.z" on stderr
    FILE* p = ::popen((tool + " -? 2>&1 | head -1").c_str(), "r");
    if (!p) return {};
    char line[256] = {};
    (void)!fgets(line, sizeof(line), p);
    ::pclose(p);
    std::string s(line);
    const auto v = s.find("Version");
    if (v == std::string::npos) return {};
    auto ver = s.substr(v + 7);
    while (!ver.empty() && (ver.front() == ' ')) ver.erase(ver.begin());
    while (!ver.empty() && (ver.back() == '\n' || ver.back() == '\r' || ver.back() == ' '))
        ver.pop_back();
    return ver;
}

}  // namespace argyll_backend

BackendStatus probe_backends() {
    BackendStatus s;
    auto& drv = I1Pro3Driver::instance();
    s.sdk_available = drv.available();
    if (s.sdk_available) {
        s.sdk_version = drv.sdkVersion();
        s.sdk_path    = drv.frameworkPath();
        s.sdk_devices = drv.deviceCount();
    }
    s.argyll_spotread_path = argyll_backend::find_spotread();
    s.argyll_available     = !s.argyll_spotread_path.empty();
    if (s.argyll_available) s.argyll_version = argyll_backend::version();
    return s;
}

struct I1Device::Impl {
    io_connect_t connection = 0;
    bool connected = false;
    DeviceType device_type = DeviceType::Unknown;
    DeviceInfo device_info{};
    bool calibration_valid = false;
    enum class Active { Sim, Sdk, Argyll };
    Active active = Active::Sim;

    bool connect_usb() {
        kern_return_t kr;

        CFMutableDictionaryRef matching = IOServiceMatching("AppleUserHIDDevices");
        if (!matching) {
            return false;
        }

        io_iterator_t iterator;
        kr = IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator);
        if (kr != KERN_SUCCESS) {
            return false;
        }

        io_object_t device;
        bool found = false;
        while ((device = IOIteratorNext(iterator)) != 0) {
            char name[256] = {};
            if (IOObjectGetClass(device, name) == KERN_SUCCESS) {
                if (strstr(name, "X-Rite") || strstr(name, "i1")) {
                    kr = IOServiceOpen(device, mach_task_self(), 0, &connection);
                    if (kr == KERN_SUCCESS) {
                        found = true;
                    }
                }
            }
            IOObjectRelease(device);
            if (found) break;
        }

        IOObjectRelease(iterator);
        connected = found;
        return found;
    }

    void disconnect_usb() {
        if (connection) {
            IOServiceClose(connection);
            connection = 0;
        }
        connected = false;
    }

    ~Impl() {
        disconnect_usb();
    }
};

I1Device::I1Device() : pImpl_(new Impl) {}

I1Device::~I1Device() = default;

bool I1Device::connect() {
    const Backend pref = preferred_backend();
    auto& drv = I1Pro3Driver::instance();

    // ① X-Rite SDK path (framework direct)
    if (pref == Backend::XRiteSdk || pref == Backend::Auto) {
        if (drv.available() && drv.openFirst()) {
            drv.setOption("MeasurementMode", "ReflectanceSpot");
            pImpl_->active = Impl::Active::Sdk;
            pImpl_->connected = true;
            pImpl_->device_type = DeviceType::i1Pro3;
            pImpl_->device_info.type = DeviceType::i1Pro3;
            pImpl_->device_info.serial_number = drv.serialNumber();
            pImpl_->device_info.is_ready = true;
            pImpl_->device_info.status_message =
                "X-Rite SDK " + drv.sdkVersion();
            return true;
        }
        if (pref == Backend::XRiteSdk) {
            pImpl_->connected = false;
            pImpl_->device_info.status_message =
                drv.available() ? ("SDK: no device (" + drv.lastError() + ")")
                                : "i1Pro3.framework not found";
            return false;
        }
    }

    // ② Argyll spotread path
    if (pref == Backend::Argyll || pref == Backend::Auto) {
        const std::string tool = argyll_backend::find_spotread();
        if (!tool.empty()) {
            pImpl_->active = Impl::Active::Argyll;
            pImpl_->connected = true;
            pImpl_->device_type = DeviceType::i1Pro3;
            pImpl_->device_info.type = DeviceType::i1Pro3;
            pImpl_->device_info.is_ready = true;
            pImpl_->device_info.status_message =
                "Argyll spotread (-N) · " + tool;
            return true;
        }
        if (pref == Backend::Argyll) {
            pImpl_->connected = false;
            pImpl_->device_info.status_message = "spotread not found";
            return false;
        }
    }

    // ③ simulation fallback (legacy IOKit probe)
    pImpl_->active = Impl::Active::Sim;
    return pImpl_->connect_usb();
}

bool I1Device::disconnect() {
    switch (pImpl_->active) {
        case Impl::Active::Sdk:    I1Pro3Driver::instance().close(); break;
        case Impl::Active::Argyll: argyll_backend::kill_stray_spotread(); break;
        case Impl::Active::Sim:    pImpl_->disconnect_usb(); break;
    }
    pImpl_->connected = false;
    return true;
}

Backend I1Device::active_backend() const {
    switch (pImpl_->active) {
        case Impl::Active::Sdk:    return Backend::XRiteSdk;
        case Impl::Active::Argyll: return Backend::Argyll;
        default:                   return Backend::Auto;
    }
}

bool I1Device::is_connected() const {
    return pImpl_->connected;
}

std::vector<DeviceInfo> I1Device::discover_devices() {
    std::vector<DeviceInfo> devices;

    // ① X-Rite SDK enumeration (authoritative when the framework is present)
    auto& drv = I1Pro3Driver::instance();
    if (drv.available()) {
        const int n = drv.deviceCount();
        for (int i = 0; i < n; ++i) {
            DeviceInfo info;
            info.type = DeviceType::i1Pro3;
            info.is_ready = true;
            info.status_message = "i1Pro3 · X-Rite SDK " + drv.sdkVersion();
            if (i == 0 && drv.openFirst()) {
                info.serial_number = drv.serialNumber();
                drv.close();
            }
            devices.push_back(info);
        }
        if (n > 0) return devices;
    }

    // ② Argyll probe: spotread installed counts as an available path
    if (!argyll_backend::find_spotread().empty()) {
        DeviceInfo info;
        info.type = DeviceType::i1Pro3;
        info.is_ready = true;
        info.status_message = "i1 · Argyll spotread (-N)";
        devices.push_back(info);
    }

    kern_return_t kr;
    // Use IOUSBDevice class name for macOS USB device discovery
    CFMutableDictionaryRef matching = IOServiceMatching("IOUSBDevice");
    if (!matching) {
        return devices;
    }

    io_iterator_t iterator;
    kr = IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator);
    if (kr != KERN_SUCCESS) {
        return devices;
    }

    io_object_t device;
    while ((device = IOIteratorNext(iterator)) != 0) {
        DeviceInfo info;
        info.type = DeviceType::Unknown;
        info.is_calibrated = false;
        info.is_ready = true;

        char class_name[256] = {};
        if (IOObjectGetClass(device, class_name) == KERN_SUCCESS) {
            if (strstr(class_name, "X-Rite") || strstr(class_name, "i1")) {
                info.status_message = "Found";
                devices.push_back(info);
            }
        }

        IOObjectRelease(device);
    }

    IOObjectRelease(iterator);
    return devices;
}

DeviceType I1Device::detect_device_type(const char* device_path) {
    return DeviceType::i1Pro3;
}

CalibrationResult I1Device::calibrate_white() {
    CalibrationResult result{};
    if (!pImpl_->connected) {
        result.error_message = "Device not connected";
        return result;
    }

    if (pImpl_->active == Impl::Active::Sdk) {
        auto& drv = I1Pro3Driver::instance();
        result.success = drv.calibrate();          // instrument on white tile
        if (!result.success)
            result.error_message = "白板校准失败: " + drv.lastError();
        else {
            result.white_reference_value = 1.0;
            pImpl_->calibration_valid = true;
        }
        return result;
    }

    if (pImpl_->active == Impl::Active::Argyll) {
        // spotread runs with -N (i1Pro3 forced white cal fails with
        // "Wrong Sensor Position"); calibration is handled per-run.
        result.success = true;
        result.white_reference_value = 1.0;
        pImpl_->calibration_valid = true;
        return result;
    }

    // simulation
    result.success = true;
    std::fill(pImpl_->device_info.spectral_data.begin(),
              pImpl_->device_info.spectral_data.end(), 1.0);
    result.white_reference_value = 1.0;
    pImpl_->calibration_valid = true;
    return result;
}

CalibrationResult I1Device::calibrate_dark() {
    CalibrationResult result{};
    result.success = pImpl_->connected;

    if (result.success) {
        result.dark_reference_value = 0.0;
        pImpl_->calibration_valid = true;
    } else {
        result.error_message = "Device not connected";
    }

    return result;
}

bool I1Device::is_calibration_valid() const {
    return pImpl_->calibration_valid;
}

MeasurementResult I1Device::measure_spot(MeasurementMode mode) {
    MeasurementResult result{};
    result.mode = mode;
    result.device = pImpl_->device_type;

    if (!pImpl_->connected) {
        result.success = false;
        result.error_message = "Device not connected";
        return result;
    }

    if (!pImpl_->calibration_valid && pImpl_->active != Impl::Active::Argyll) {
        result.success = false;
        result.error_message = "Device not calibrated";
        return result;
    }

    // ① X-Rite SDK: trigger + 36ch spectrum straight from the instrument
    if (pImpl_->active == Impl::Active::Sdk) {
        auto& drv = I1Pro3Driver::instance();
        std::array<float, 36> sp{};
        if (!drv.triggerMeasurement() || !drv.spectrum(sp)) {
            result.success = false;
            result.error_message = "测量失败: " + drv.lastError();
            return result;
        }
        for (size_t i = 0; i < sp.size(); ++i)
            result.spectral_data[i] = static_cast<double>(sp[i]);
        result.success = true;
    }
    // ② Argyll: spotread -N -s subprocess
    else if (pImpl_->active == Impl::Active::Argyll) {
        auto spot = argyll_backend::measure_spot();
        if (!spot.ok) {
            result.success = false;
            result.error_message = spot.error;
            return result;
        }
        if (spot.has_spectrum)
            for (size_t i = 0; i < spot.spectrum.size(); ++i)
                result.spectral_data[i] = spot.spectrum[i];
        result.lab.L = spot.L;
        result.lab.a = spot.a;
        result.lab.b = spot.b;
        result.success = true;
        if (!spot.has_spectrum) {
            // Lab came straight from spotread; reflectance approximated from L*
            result.reflectance = spot.L / 100.0;
            return result;
        }
    }
    // ③ simulation
    else {
        result.success = true;
        auto d50 = spectral::Illuminant::D50();
        std::copy(d50.begin(), d50.end(), result.spectral_data.begin());
    }

    auto xyz = spectral::spectrum_to_xyz(result.spectral_data);
    result.X = xyz.X;
    result.Y = xyz.Y;
    result.Z = xyz.Z;

    result.lab = spectral::xyz_to_lab(xyz);
    result.reflectance = xyz.Y / 100.0;

    return result;
}

MeasurementResult I1Device::measure_sequence(int num_measurements, ProgressCallback callback) {
    MeasurementResult last_result{};
    last_result.device = pImpl_->device_type;

    for (int i = 0; i < num_measurements; ++i) {
        auto result = measure_spot();
        last_result = result;

        if (callback) {
            callback((i + 1) * 100 / num_measurements,
                    ("Measuring patch " + std::to_string(i + 1)).c_str());
        }
    }

    return last_result;
}

DeviceInfo I1Device::get_device_info() const {
    return pImpl_->device_info;
}

bool I1Device::check_sandbox_entitlements() {
    return false;
}

bool I1Device::requires_special_entitlements() {
    return true;
}

MeasurementTarget::MeasurementTarget() {}

MeasurementTarget::~MeasurementTarget() {}

void MeasurementTarget::add_patch(const ColorCheckerPatch& patch) {
    patches_.push_back(patch);
}

void MeasurementTarget::clear_patches() {
    patches_.clear();
}

MeasurementTarget MeasurementTarget::create_colorchecker_24() {
    MeasurementTarget target;

    constexpr double colorchecker_24_lab[][3] = {
        {59.6, 38.4, 36.0},    // A1 Dark skin
        {64.5, 20.4, 44.8},    // A2 Light skin
        {49.3, 9.1, -37.3},    // A3 Blue sky
        {42.1, 15.8, -41.2},   // A4 Foliage
        {66.1, 12.7, 50.2},    // A5 Blue flower
        {52.5, 15.9, -3.4},    // A6 Purplish blue
        {40.3, 17.6, -42.8},   // B1 Moderate red
        {70.0, 10.2, 61.1},    // B2 Orange
        {71.4, -10.2, 69.1},   // B3 Yellow
        {55.9, 8.5, 44.9},     // B4 Moderate green
        {48.7, -0.1, -0.2},    // B5 Light cyan
        {50.9, 49.0, -14.6},   // B6 Blue
        {42.2, 20.0, -45.0},   // C1 Dark orange
        {54.8, 31.2, 40.1},    // C2 Purplish pink
        {71.7, 0.0, 62.0},     // C3 Yellowish pink
        {51.0, -1.4, -26.1},   // C4 Yellowish green
        {41.5, 2.7, -21.6},    // C5 Violet
        {33.3, 13.4, -21.0},   // C6 Moderate orange yellow
        {72.9, 12.5, 66.5},    // D1 Light purple
        {63.5, 21.0, 35.1},    // D2 Light skin
        {42.5, 10.0, -42.0},   // D3 Moderate olive
        {45.7, 15.8, -39.5},   // D4 Dark skin
        {60.1, 9.8, -44.6},    // D5 Moderate blue
        {49.2, 23.5, 23.0}     // D6 Moderate red
    };

    for (int i = 0; i < 24; ++i) {
        ColorCheckerPatch patch;
        patch.row = i / 6;
        patch.column = i % 6;
        patch.expected_lab.L = colorchecker_24_lab[i][0];
        patch.expected_lab.a = colorchecker_24_lab[i][1];
        patch.expected_lab.b = colorchecker_24_lab[i][2];
        patch.name = "Patch " + std::to_string(i + 1);
        target.add_patch(patch);
    }

    return target;
}

MeasurementTarget MeasurementTarget::create_gray_ramp(int num_patches) {
    MeasurementTarget target;

    for (int i = 0; i < num_patches; ++i) {
        ColorCheckerPatch patch;
        patch.row = 0;
        patch.column = i;
        patch.expected_lab.L = i * 100.0 / (num_patches - 1);
        patch.expected_lab.a = 0;
        patch.expected_lab.b = 0;
        patch.name = "Gray " + std::to_string(i + 1);
        target.add_patch(patch);
    }

    return target;
}

MeasurementTarget MeasurementTarget::create_gray_ramp_advanced() {
    return create_gray_ramp(256);
}

SpectrumCapture::SpectrumCapture() {}

SpectrumCapture::~SpectrumCapture() {}

SpectrumCapture::ScanResult SpectrumCapture::scan_strip(const StripScanOptions& options) {
    ScanResult result;

    // B10 stub: simulate strip scanning
    // In real implementation, this would:
    // 1. Initialize the scanning engine
    // 2. Move the strip past the reading head
    // 3. Take measurements at each patch position
    // 4. Return the spectral data for each patch

    result.patches_scanned = options.num_patches;
    result.success = true;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> noise(0.0, 0.01);

    auto d50 = spectral::Illuminant::D50();

    for (int i = 0; i < options.num_patches; ++i) {
        // Generate simulated spectral data
        spectral::Spectrum spectrum;
        for (int ch = 0; ch < spectral::NUM_CHANNELS; ++ch) {
            spectrum[ch] = d50[ch] * (1.0 + noise(gen));
        }
        result.spectra.push_back(spectrum);

        // Compute reflectance (simplified: use Y channel)
        auto xyz = spectral::spectrum_to_xyz(spectrum);
        double reflectance = xyz.Y / 100.0;
        result.reflectance_values.push_back(reflectance);

        // Compute Lab
        auto lab = spectral::xyz_to_lab(xyz);
        result.lab_values.push_back(lab);
    }

    return result;
}

spectral::Spectrum SpectrumCapture::capture_spectrum(MeasurementMode mode) {
    spectral::Spectrum spectrum;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> noise(0, 0.01);

    auto d65 = spectral::Illuminant::D65();
    for (int i = 0; i < spectral::NUM_CHANNELS; ++i) {
        spectrum[i] = d65[i] * (1.0 + noise(gen));
    }

    return spectrum;
}

std::vector<spectral::Spectrum> SpectrumCapture::capture_sequence(int count) {
    std::vector<spectral::Spectrum> spectra;
    for (int i = 0; i < count; ++i) {
        spectra.push_back(capture_spectrum(MeasurementMode::SpotReflectance));
    }
    return spectra;
}

spectral::XYZ SpectrumCapture::spectrum_to_xyz(const spectral::Spectrum& spectrum) {
    return spectral::spectrum_to_xyz(spectrum);
}

spectral::Lab SpectrumCapture::spectrum_to_lab(const spectral::Spectrum& spectrum) {
    auto xyz = spectrum_to_xyz(spectrum);
    return spectral::xyz_to_lab(xyz);
}

bool SpectrumCapture::validate_spectrum(const spectral::Spectrum& spectrum) {
    for (int i = 0; i < spectral::NUM_CHANNELS; ++i) {
        if (spectrum[i] < 0 || spectrum[i] > 1000) {
            return false;
        }
    }
    return true;
}

bool SpectrumCapture::compare_to_target(const spectral::Spectrum& measured,
                                       const spectral::Lab& target,
                                       double max_delta_e) {
    auto measured_lab = spectrum_to_lab(measured);
    auto de = delta_e::delta_e_2000(measured_lab, target);
    return de <= max_delta_e;
}

}
}