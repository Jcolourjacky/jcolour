#include "i1pro3_driver.h"

#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cstring>
#include <vector>

// ============================================================================
// Verified C API of i1Pro3.framework (no public headers shipped).
//
// Evidence: disassembly of i1Cocoa25M1-i1Pro3.app call sites (x86_64 SysV —
// args in rdi, rsi, rdx, rcx) matched against `nm -gU i1Pro3.framework/i1Pro3`:
//
//   GetDevices      leaq &handles→rdi, leaq &count→rsi          → (DEV**, u32*)
//   OpenDevice      movq handles[0]→rdi                          → (DEV)
//   CloseDevice     movq dev→rdi                                 → (DEV)
//   GetConnectionStatus  movq dev→rdi; cmpl $3,%eax              → int (3 = ok)
//   Calibrate       movq dev→rdi                                 → (DEV)
//   TriggerMeasurement   movq dev→rdi                            → (DEV)
//   GetSpectrum     dev→rdi, float*→rsi, index→edx               → (DEV, f*, u32)
//   GetTriStimulus  dev→rdi, float*→rsi, index→edx               → (DEV, f*, u32)
//   GetNumberOfAvailableSamples  dev→rdi                         → int in eax
//   SetOption       dev→rdi, "key"→rsi, "value"→rdx              → (DEV, c*, c*)
//   GetOption       dev→rdi, key→rsi, buf→rdx, &bufsize→rcx      → (DEV, c*, c*, u32*)
//   GetGlobalOption key→rdi, buf→rsi, &bufsize→rdx               → (c*, c*, u32*)
//
// Option keys observed in the binary: "MeasurementMode" (values
// "ReflectanceSpot" / "ReflectanceScan"), "ColorSpaceDescription.Type"
// (e.g. "CIExyY" / "CIELab"), "Colorimetric.Illumination", "SerialNumber",
// "SDKVersion", "LastError", "TimeUntilCalibrationExpire".
// ============================================================================

namespace jcolour {
namespace i1sdk {

namespace {

using DEV = void*;
using FnGetDevices   = int (*)(DEV**, uint32_t*);
using FnDevOnly      = int (*)(DEV);
using FnGetSpectrum  = int (*)(DEV, float*, uint32_t);
using FnSetOption    = int (*)(DEV, const char*, const char*);
using FnGetOption    = int (*)(DEV, const char*, char*, uint32_t*);
using FnGetGlobalOpt = int (*)(const char*, char*, uint32_t*);

bool fileExists(const std::string& p) {
    struct stat st{};
    return ::stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::vector<std::string> searchPaths() {
    std::vector<std::string> v;
    if (const char* env = std::getenv("JCOLOUR_I1PRO3_FRAMEWORK")) {
        v.emplace_back(std::string(env));                       // binary directly
        v.emplace_back(std::string(env) + "/i1Pro3");           // framework dir
        v.emplace_back(std::string(env) + "/i1Pro3.framework/i1Pro3");
    }
    // bundled next to the executable (.app/Contents/Frameworks)
    char exe[2048];
    uint32_t n = sizeof(exe);
    if (_NSGetExecutablePath(exe, &n) == 0) {
        std::string dir(exe);
        const auto slash = dir.rfind('/');
        if (slash != std::string::npos) dir.resize(slash);
        v.push_back(dir + "/../Frameworks/i1Pro3.framework/i1Pro3");
    }
    v.emplace_back("/Library/Frameworks/i1Pro3.framework/i1Pro3");
    const char* home = std::getenv("HOME");
    const std::string h = home ? home : "";
    if (!h.empty()) {
        v.push_back(h + "/Library/Frameworks/i1Pro3.framework/i1Pro3");
        // dev-convenience drops on this machine
        v.push_back(h + "/Desktop/colour app/i1Pro3_SDK/i1Pro3.framework/i1Pro3");
        v.push_back(h + "/Desktop/i1Pro3.framework/Versions/A/i1Pro3");
    }
    return v;
}

}  // namespace

struct I1Pro3Driver::Impl {
    void*       lib = nullptr;
    std::string path;

    FnGetDevices   GetDevices = nullptr;
    FnDevOnly      OpenDevice = nullptr, CloseDevice = nullptr,
                   GetConnectionStatus = nullptr, Calibrate = nullptr,
                   TriggerMeasurement = nullptr, GetNumSamples = nullptr;
    FnGetSpectrum  GetSpectrum = nullptr, GetTriStimulus = nullptr;
    FnSetOption    SetOption = nullptr;
    FnGetOption    GetOption = nullptr;
    FnGetGlobalOpt GetGlobalOption = nullptr;

    DEV  dev = nullptr;
    bool open = false;

    bool load() {
        for (const auto& p : searchPaths()) {
            if (!fileExists(p)) continue;
            lib = dlopen(p.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (lib) { path = p; break; }
        }
        if (!lib) return false;
        auto sym = [&](const char* s) { return dlsym(lib, s); };
        GetDevices          = reinterpret_cast<FnGetDevices>(sym("I1PRO3_GetDevices"));
        OpenDevice          = reinterpret_cast<FnDevOnly>(sym("I1PRO3_OpenDevice"));
        CloseDevice         = reinterpret_cast<FnDevOnly>(sym("I1PRO3_CloseDevice"));
        GetConnectionStatus = reinterpret_cast<FnDevOnly>(sym("I1PRO3_GetConnectionStatus"));
        Calibrate           = reinterpret_cast<FnDevOnly>(sym("I1PRO3_Calibrate"));
        TriggerMeasurement  = reinterpret_cast<FnDevOnly>(sym("I1PRO3_TriggerMeasurement"));
        GetNumSamples       = reinterpret_cast<FnDevOnly>(sym("I1PRO3_GetNumberOfAvailableSamples"));
        GetSpectrum         = reinterpret_cast<FnGetSpectrum>(sym("I1PRO3_GetSpectrum"));
        GetTriStimulus      = reinterpret_cast<FnGetSpectrum>(sym("I1PRO3_GetTriStimulus"));
        SetOption           = reinterpret_cast<FnSetOption>(sym("I1PRO3_SetOption"));
        GetOption           = reinterpret_cast<FnGetOption>(sym("I1PRO3_GetOption"));
        GetGlobalOption     = reinterpret_cast<FnGetGlobalOpt>(sym("I1PRO3_GetGlobalOption"));
        const bool ok = GetDevices && OpenDevice && CloseDevice && GetConnectionStatus &&
                        Calibrate && TriggerMeasurement && GetNumSamples && GetSpectrum &&
                        GetTriStimulus && SetOption && GetOption && GetGlobalOption;
        if (!ok) { dlclose(lib); lib = nullptr; path.clear(); }
        return ok;
    }
};

I1Pro3Driver& I1Pro3Driver::instance() {
    static I1Pro3Driver inst;
    return inst;
}

I1Pro3Driver::I1Pro3Driver() : d_(new Impl) { d_->load(); }
I1Pro3Driver::~I1Pro3Driver() {
    close();
    if (d_->lib) dlclose(d_->lib);
    delete d_;
}

bool        I1Pro3Driver::available() const     { return d_->lib != nullptr; }
std::string I1Pro3Driver::frameworkPath() const { return d_->path; }
bool        I1Pro3Driver::isOpen() const        { return d_->open; }

std::string I1Pro3Driver::sdkVersion() {
    if (!available()) return {};
    char buf[512] = {};
    uint32_t n = sizeof(buf);
    d_->GetGlobalOption("SDKVersion", buf, &n);
    return buf;
}

std::string I1Pro3Driver::lastError() {
    if (!available()) return "i1Pro3.framework not found";
    char buf[512] = {};
    uint32_t n = sizeof(buf);
    d_->GetGlobalOption("LastError", buf, &n);
    return buf;
}

int I1Pro3Driver::deviceCount() {
    if (!available()) return 0;
    DEV* arr = nullptr;
    uint32_t count = 0;
    if (d_->GetDevices(&arr, &count) != 0) return 0;
    return static_cast<int>(count);
}

bool I1Pro3Driver::openFirst() {
    if (!available()) return false;
    if (d_->open) return true;
    DEV* arr = nullptr;
    uint32_t count = 0;
    if (d_->GetDevices(&arr, &count) != 0 || count == 0 || !arr) return false;
    d_->dev = arr[0];
    if (d_->OpenDevice(d_->dev) != 0 && d_->GetConnectionStatus(d_->dev) != 3) {
        d_->dev = nullptr;
        return false;
    }
    d_->open = true;
    return true;
}

bool I1Pro3Driver::isFunctional() {
    return d_->open && d_->GetConnectionStatus(d_->dev) == 3;
}

void I1Pro3Driver::close() {
    if (d_->open && d_->CloseDevice) d_->CloseDevice(d_->dev);
    d_->dev = nullptr;
    d_->open = false;
}

bool I1Pro3Driver::setOption(const char* key, const char* value) {
    return d_->open && d_->SetOption(d_->dev, key, value) == 0;
}

std::string I1Pro3Driver::getOption(const char* key) {
    if (!d_->open) return {};
    char buf[512] = {};
    uint32_t n = sizeof(buf);
    if (d_->GetOption(d_->dev, key, buf, &n) != 0) return {};
    return buf;
}

bool I1Pro3Driver::calibrate() {
    return d_->open && d_->Calibrate(d_->dev) == 0;
}

bool I1Pro3Driver::triggerMeasurement() {
    return d_->open && d_->TriggerMeasurement(d_->dev) == 0;
}

int I1Pro3Driver::availableSamples() {
    return d_->open ? d_->GetNumSamples(d_->dev) : 0;
}

bool I1Pro3Driver::spectrum(std::array<float, 36>& out, uint32_t index) {
    out.fill(0.f);
    return d_->open && d_->GetSpectrum(d_->dev, out.data(), index) == 0;
}

bool I1Pro3Driver::triStimulus(std::array<float, 3>& out, uint32_t index) {
    out.fill(0.f);
    return d_->open && d_->GetTriStimulus(d_->dev, out.data(), index) == 0;
}

}  // namespace i1sdk
}  // namespace jcolour
