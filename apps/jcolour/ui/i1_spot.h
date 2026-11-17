#pragma once

// i1Pro3 单点测量 → CIELAB（D50/2°）。与 TIL / G7 RGB 向导同一套驱动调用。
// 阻塞调用：请在工作线程里用。没有 SDK / 没有仪器时返回 false，由调用方决定
// 是否改用模拟（界面上必须标「模拟」）。

#include "../../../engine/src/i1pro3_driver.h"
#include "../../../engine/src/spectral.h"

#include <array>

namespace jcolour::ui {

inline bool measureSpotLab(jcolour::spectral::Lab& out) {
    auto& d = jcolour::i1sdk::I1Pro3Driver::instance();
    if (!d.available() || !(d.isOpen() || d.openFirst())) return false;
    d.setOption("MeasurementMode", "ReflectanceSpot");
    if (!(d.calibrate() && d.triggerMeasurement() && d.availableSamples() > 0)) return false;
    std::array<float, 36> spec{};
    if (!d.spectrum(spec, 0)) return false;
    jcolour::spectral::Spectrum s{};
    for (int i = 0; i < 36; ++i) s[size_t(i)] = double(spec[size_t(i)]);
    out = jcolour::spectral::xyz_to_lab(jcolour::spectral::spectrum_to_xyz(s));
    return true;
}

inline bool i1Available() {
    return jcolour::i1sdk::I1Pro3Driver::instance().available();
}

}  // namespace jcolour::ui
