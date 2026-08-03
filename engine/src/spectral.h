#pragma once

#include <array>
#include <vector>
#include <cstdint>

namespace jcolour {
namespace spectral {

constexpr int NUM_CHANNELS = 36;
constexpr double WAVELENGTH_MIN = 380.0;
constexpr double WAVELENGTH_MAX = 730.0;
constexpr double WAVELENGTH_STEP = 10.0;

using Spectrum = std::array<double, NUM_CHANNELS>;

constexpr std::array<double, NUM_CHANNELS> WAVELENGTHS = [] {
    std::array<double, NUM_CHANNELS> wavelengths = {};
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        wavelengths[i] = WAVELENGTH_MIN + i * WAVELENGTH_STEP;
    }
    return wavelengths;
}();

class Illuminant {
public:
    static Spectrum D50();
    static Spectrum D65();
    static Spectrum A();
    static Spectrum F2();
    static Spectrum F11();
    static Spectrum LED3000K();
    static Spectrum LED5000K();
};

class CMFIlluminant {
public:
    // CIE 1931 2° observer, 380-730nm @ 10nm: [wavelength, x̄, ȳ, z̄]
    static constexpr double CIE_1931_2DEG[36][4] = {
        {380, 0.001368, 0.000039, 0.006450},
        {390, 0.004243, 0.000120, 0.020050},
        {400, 0.014310, 0.000396, 0.067850},
        {410, 0.043510, 0.001210, 0.207400},
        {420, 0.134380, 0.004000, 0.645600},
        {430, 0.283900, 0.011600, 1.385600},
        {440, 0.348280, 0.023000, 1.747060},
        {450, 0.336200, 0.038000, 1.772110},
        {460, 0.290800, 0.060000, 1.669200},
        {470, 0.195360, 0.090980, 1.287640},
        {480, 0.095640, 0.139020, 0.812950},
        {490, 0.032010, 0.208020, 0.465180},
        {500, 0.004900, 0.323000, 0.272000},
        {510, 0.009300, 0.503000, 0.158200},
        {520, 0.063270, 0.710000, 0.078250},
        {530, 0.165500, 0.862000, 0.042160},
        {540, 0.290400, 0.954000, 0.020300},
        {550, 0.433450, 0.994950, 0.008750},
        {560, 0.594500, 0.995000, 0.003900},
        {570, 0.762100, 0.952000, 0.002100},
        {580, 0.916300, 0.870000, 0.001650},
        {590, 1.026300, 0.757000, 0.001100},
        {600, 1.062200, 0.631000, 0.000800},
        {610, 1.002600, 0.503000, 0.000340},
        {620, 0.854450, 0.381000, 0.000190},
        {630, 0.642400, 0.265000, 0.000050},
        {640, 0.447900, 0.175000, 0.000020},
        {650, 0.283500, 0.107000, 0.000000},
        {660, 0.164900, 0.061000, 0.000000},
        {670, 0.087400, 0.032000, 0.000000},
        {680, 0.046770, 0.017000, 0.000000},
        {690, 0.022700, 0.008210, 0.000000},
        {700, 0.011359, 0.004102, 0.000000},
        {710, 0.005790, 0.002091, 0.000000},
        {720, 0.002899, 0.001047, 0.000000},
        {730, 0.001440, 0.000520, 0.000000},
    };
};

struct XYZ {
    double X, Y, Z;
};

struct Lab {
    double L, a, b;
};

Spectrum rgb_to_spectrum(const std::array<double, 3>& rgb);
XYZ spectrum_to_xyz(const Spectrum& spectrum, const Spectrum& illuminant);
XYZ spectrum_to_xyz(const Spectrum& spectrum);
Lab xyz_to_lab(const XYZ& xyz);
double* spectrum_to_array(const Spectrum& spectrum);

}
}