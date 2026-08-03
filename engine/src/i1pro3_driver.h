#pragma once

// I1Pro3Driver — runtime loader for the X-Rite i1Pro3.framework C API.
//
// X-Rite does not ship public headers with the framework. Every signature
// used here was verified by disassembling a shipping SDK client
// (i1Cocoa25M1-i1Pro3.app, x86_64 SysV call sites) against the framework's
// export table — see the evidence table at the top of i1pro3_driver.cpp.
//
// The framework is loaded with dlopen() at runtime so the engine builds and
// runs (in simulation mode) on machines without the SDK. Search order:
//   1. $JCOLOUR_I1PRO3_FRAMEWORK   (dir of the .framework, or the binary)
//   2. <app>/../Frameworks/i1Pro3.framework/i1Pro3
//   3. /Library/Frameworks  ·  ~/Library/Frameworks
//   4. dev fallbacks under ~/Desktop (SDK drop locations on this machine)
//
// Header stays dependency-free (no Qt, no engine includes) so a bare
// `clang++ probe.cpp i1pro3_driver.cpp` smoke test can drive the instrument.

#include <array>
#include <cstdint>
#include <string>

namespace jcolour {
namespace i1sdk {

class I1Pro3Driver {
public:
    static I1Pro3Driver& instance();

    // framework
    bool        available() const;       // dlopen ok + all symbols resolved
    std::string frameworkPath() const;   // where it was loaded from
    std::string sdkVersion();            // GetGlobalOption("SDKVersion")
    std::string lastError();             // GetGlobalOption("LastError")

    // device lifecycle
    int  deviceCount();                  // GetDevices() enumeration
    bool openFirst();                    // GetDevices + OpenDevice on [0]
    bool isOpen() const;
    bool isFunctional();                 // GetConnectionStatus == 3
    void close();

    // options (key/value, EyeOne convention)
    bool        setOption(const char* key, const char* value);
    std::string getOption(const char* key);
    std::string serialNumber() { return getOption("SerialNumber"); }

    // measurement
    bool calibrate();                    // white-tile calibration
    bool triggerMeasurement();
    int  availableSamples();
    bool spectrum(std::array<float, 36>& out, uint32_t index = 0);
    bool triStimulus(std::array<float, 3>& out, uint32_t index = 0);

private:
    I1Pro3Driver();
    ~I1Pro3Driver();
    I1Pro3Driver(const I1Pro3Driver&) = delete;
    I1Pro3Driver& operator=(const I1Pro3Driver&) = delete;
    struct Impl;
    Impl* d_;
};

}  // namespace i1sdk
}  // namespace jcolour
