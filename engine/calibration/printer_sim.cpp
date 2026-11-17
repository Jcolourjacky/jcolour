#include "printer_sim.h"

#include <algorithm>
#include <cmath>

namespace jcolour {
namespace printbase {

namespace {

constexpr double kXn = 0.96422, kYn = 1.0, kZn = 0.82521;   // D50

spectral::XYZ lab_to_xyz(const Lab& c) {
    const double fy = (c.L + 16.0) / 116.0;
    const double fx = fy + c.a / 500.0;
    const double fz = fy - c.b / 200.0;
    auto inv = [](double f) {
        const double d = 6.0 / 29.0;
        return f > d ? f * f * f : 3.0 * d * d * (f - 4.0 / 29.0);
    };
    return {kXn * inv(fx), kYn * inv(fy), kZn * inv(fz)};
}

Lab xyz_to_lab(const spectral::XYZ& x) {
    auto f = [](double t) {
        const double d = 6.0 / 29.0;
        return t > d * d * d ? std::cbrt(t) : t / (3 * d * d) + 4.0 / 29.0;
    };
    const double fx = f(x.X / kXn), fy = f(x.Y / kYn), fz = f(x.Z / kZn);
    return {116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)};
}

}  // namespace

SimulatedPrinter SimulatedPrinter::epson_p_nominal() {
    SimulatedPrinter p;
    p.setNominalPaper({95.5, 0.5, -2.5});
    // 近似 UltraChrome PRO12 在亮光相纸上的实地色（D50）
    p.addChannel({"K",   {4.0, 0.3, -0.8},  3.2});
    p.addChannel({"LC",  {68.0, -27.0, -24.0}, 2.6});
    p.addChannel({"C",   {54.0, -33.0, -50.0}, 3.0});
    p.addChannel({"VLM", {67.0, 42.0, -14.0}, 2.6});
    p.addChannel({"VM",  {47.0, 76.0, -6.0},  3.0});
    p.addChannel({"Y",   {88.0, -3.0, 95.0},  2.8});
    p.addChannel({"Or",  {64.0, 52.0, 74.0},  3.0});
    p.addChannel({"GY",  {46.0, 0.2, -1.5},  2.7});
    p.addChannel({"LGY", {70.0, 0.0, -1.8},  2.5});
    p.addChannel({"V",   {30.0, 38.0, -62.0}, 3.1});
    p.addChannel({"Gr",  {55.0, -66.0, 22.0}, 3.0});
    return p;
}

Lab SimulatedPrinter::print(const std::vector<std::pair<std::string, double>>& inks) const {
    const spectral::XYZ pn = lab_to_xyz(nominalPaper_);
    const spectral::XYZ pc = lab_to_xyz(paper_);
    double tx = 1.0, ty = 1.0, tz = 1.0;
    for (const auto& [name, u0] : inks) {
        const SimChannel* ch = nullptr;
        for (const auto& c : channels_)
            if (c.name == name) { ch = &c; break; }
        if (!ch) continue;
        const double u = std::clamp(u0, 0.0, 1.0);
        const double x = std::clamp(u + ch->dotGain * 4.0 * u * (1.0 - u), 0.0, 1.0);
        const double sat = (1.0 - std::exp(-ch->shape * x)) / (1.0 - std::exp(-ch->shape));
        const double q = ch->strength * sat;
        const spectral::XYZ s = lab_to_xyz(ch->solid);
        auto T = [q](double solid, double paper) {
            const double r = std::clamp(solid / paper, 1e-4, 1.0);
            return std::pow(r, q);
        };
        tx *= T(s.X, pn.X);
        ty *= T(s.Y, pn.Y);
        tz *= T(s.Z, pn.Z);
    }
    return xyz_to_lab({pc.X * tx, pc.Y * ty, pc.Z * tz});
}

ChannelRamp SimulatedPrinter::measureRamp(const std::string& ch,
                                          const std::vector<double>& steps) const {
    ChannelRamp r;
    r.channel = ch;
    for (double s : steps) r.patches.push_back({s, printSingle(ch, s)});
    return r;
}

void SimulatedPrinter::driftInk(const std::string& ch, double strengthFactor, double dotGainDelta) {
    for (auto& c : channels_)
        if (c.name == ch) { c.strength *= strengthFactor; c.dotGain += dotGainDelta; }
}

const std::vector<double>& default_ramp_steps() {
    static const std::vector<double> k = {0.0, 0.03, 0.06, 0.10, 0.20, 0.30, 0.40,
                                          0.50, 0.60, 0.70, 0.80, 0.90, 1.0};
    return k;
}

}  // namespace printbase
}  // namespace jcolour
