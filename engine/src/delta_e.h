#pragma once

#include "spectral.h"
#include <cmath>
#include <vector>
#include <initializer_list>

namespace jcolour {
namespace delta_e {

constexpr double K_L = 1.0;
constexpr double K_C = 1.0;
constexpr double K_H = 1.0;
constexpr double K_1 = 0.045;
constexpr double K_2 = 0.015;

double delta_e_76(const spectral::Lab& lab1, const spectral::Lab& lab2);

double delta_e_94(const spectral::Lab& lab1, const spectral::Lab& lab2);

double delta_e_2000(const spectral::Lab& lab1, const spectral::Lab& lab2);

spectral::Lab interpolate_lab(const spectral::Spectrum& spec);

// Returns the maximum ΔE2000 between two spectra across all given illuminants.
// Used for metamerism detection: a high value means the two spectra look different
// under at least one of the test illuminants.
double max_delta_e_under(
    const spectral::Spectrum& spec1,
    const spectral::Spectrum& spec2,
    const std::vector<spectral::Spectrum>& illuminants);

// Convenience overload using the standard metamerism test illuminants:
// D50, D65, A, F2, F11 (CIE metamerism index set)
double max_delta_e_under_standard(
    const spectral::Spectrum& spec1,
    const spectral::Spectrum& spec2);

}
}