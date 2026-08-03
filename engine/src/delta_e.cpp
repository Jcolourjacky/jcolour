#include "delta_e.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace jcolour {
namespace delta_e {

double delta_e_76(const spectral::Lab& lab1, const spectral::Lab& lab2) {
    double dL = lab2.L - lab1.L;
    double da = lab2.a - lab1.a;
    double db = lab2.b - lab1.b;
    return std::sqrt(dL * dL + da * da + db * db);
}

double delta_e_94(const spectral::Lab& lab1, const spectral::Lab& lab2) {
    double dL = lab2.L - lab1.L;
    double da = lab2.a - lab1.a;
    double db = lab2.b - lab1.b;

    double Sl = 1.0;
    double Sc = 1.0 + K_1 * std::sqrt(lab1.a * lab1.a + lab1.b * lab1.b);
    double Sh = 1.0 + K_2 * std::sqrt(lab1.a * lab1.a + lab1.b * lab1.b);

    double dC = std::sqrt(da * da + db * db);
    double dH_sq = da * da + db * db - dC * dC;
    double dH = dH_sq > 0 ? std::sqrt(dH_sq) : 0;

    double term1 = dL / (K_L * Sl);
    double term2 = dC / (K_C * Sc);
    double term3 = dH / (K_H * Sh);

    return std::sqrt(term1 * term1 + term2 * term2 + term3 * term3);
}

double delta_e_2000(const spectral::Lab& lab1, const spectral::Lab& lab2) {
    double C_1 = std::sqrt(lab1.a * lab1.a + lab1.b * lab1.b);
    double C_2 = std::sqrt(lab2.a * lab2.a + lab2.b * lab2.b);
    double C_bar = (C_1 + C_2) / 2.0;
    double C_bar_7 = C_bar * C_bar * C_bar * C_bar * C_bar * C_bar * C_bar;
    double G = 0.5 * (1.0 - std::sqrt(C_bar_7 / (C_bar_7 + 6103515625.0)));

    double a1_prime = lab1.a * (1.0 + G);
    double a2_prime = lab2.a * (1.0 + G);

    double C1_prime = std::sqrt(a1_prime * a1_prime + lab1.b * lab1.b);
    double C2_prime = std::sqrt(a2_prime * a2_prime + lab2.b * lab2.b);
    double C_bar_prime = (C1_prime + C2_prime) / 2.0;

    double h1_prime = std::atan2(lab1.b, a1_prime) * 180.0 / M_PI;
    if (h1_prime < 0) h1_prime += 360.0;

    double h2_prime = std::atan2(lab2.b, a2_prime) * 180.0 / M_PI;
    if (h2_prime < 0) h2_prime += 360.0;

    double dL_prime = lab2.L - lab1.L;
    double dC_prime = C2_prime - C1_prime;

    double dh_prime;
    if (C1_prime * C2_prime == 0) {
        dh_prime = 0;
    } else if (std::abs(h2_prime - h1_prime) <= 180.0) {
        dh_prime = h2_prime - h1_prime;
    } else if (h2_prime - h1_prime > 180.0) {
        dh_prime = h2_prime - h1_prime - 360.0;
    } else {
        dh_prime = h2_prime - h1_prime + 360.0;
    }
    double dH_prime = 2.0 * std::sqrt(C1_prime * C2_prime) *
                       std::sin((dh_prime * M_PI / 180.0) / 2.0);

    double L_bar_prime = (lab1.L + lab2.L) / 2.0;
    double H_bar_prime = h1_prime + h2_prime;
    if (C1_prime * C2_prime != 0) {
        if (std::abs(h1_prime - h2_prime) > 180.0) {
            H_bar_prime += (H_bar_prime < 360.0) ? 360.0 : -360.0;
        }
        H_bar_prime /= 2.0;
    }

    double T = 1.0
             - 0.17 * std::cos((H_bar_prime - 30.0) * M_PI / 180.0)
             + 0.24 * std::cos(2.0 * H_bar_prime * M_PI / 180.0)
             + 0.32 * std::cos((3.0 * H_bar_prime + 6.0) * M_PI / 180.0)
             - 0.20 * std::cos((4.0 * H_bar_prime - 63.0) * M_PI / 180.0);

    double C_bar_7_prime = C_bar_prime * C_bar_prime * C_bar_prime *
                          C_bar_prime * C_bar_prime * C_bar_prime * C_bar_prime;
    double S_L = 1.0 + (0.015 * (L_bar_prime - 50.0) * (L_bar_prime - 50.0)) /
                       std::sqrt(20.0 + (L_bar_prime - 50.0) * (L_bar_prime - 50.0));
    double S_C = 1.0 + 0.045 * C_bar_prime;
    double S_H = 1.0 + 0.015 * C_bar_prime * T;
    double dTheta = 30.0 * std::exp(-((H_bar_prime - 275.0) / 25.0) * ((H_bar_prime - 275.0) / 25.0));
    double R_C = 2.0 * std::sqrt(C_bar_7_prime / (C_bar_7_prime + 6103515625.0));
    double R_T = -R_C * std::sin(2.0 * dTheta * M_PI / 180.0);

    double kL = 1.0, kC = 1.0, kH = 1.0;

    double term_L = dL_prime / (kL * S_L);
    double term_C = dC_prime / (kC * S_C);
    double term_H = dH_prime / (kH * S_H);

    return std::sqrt(term_L * term_L + term_C * term_C + term_H * term_H +
                     R_T * term_C * term_H);
}

spectral::Lab interpolate_lab(const spectral::Spectrum& spec) {
    auto xyz = spectral::spectrum_to_xyz(spec);
    return spectral::xyz_to_lab(xyz);
}

double max_delta_e_under(
    const spectral::Spectrum& spec1,
    const spectral::Spectrum& spec2,
    const std::vector<spectral::Spectrum>& illuminants)
{
    double max_de = 0.0;
    for (const auto& illum : illuminants) {
        auto lab1 = spectral::xyz_to_lab(spectral::spectrum_to_xyz(spec1, illum));
        auto lab2 = spectral::xyz_to_lab(spectral::spectrum_to_xyz(spec2, illum));
        double de = delta_e_2000(lab1, lab2);
        if (de > max_de) max_de = de;
    }
    return max_de;
}

double max_delta_e_under_standard(
    const spectral::Spectrum& spec1,
    const spectral::Spectrum& spec2)
{
    return max_delta_e_under(spec1, spec2, {
        spectral::Illuminant::D50(),
        spectral::Illuminant::D65(),
        spectral::Illuminant::A(),
        spectral::Illuminant::F2(),
        spectral::Illuminant::F11()
    });
}

}
}