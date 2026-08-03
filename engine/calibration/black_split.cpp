#include "black_split.h"

#include <algorithm>

namespace jcolour {
namespace calibration {

namespace {

// Hermite smoothstep: 0 below a, 1 above b, C1-continuous in between.
double smoothstep(double a, double b, double t) {
    if (b <= a) return t >= b ? 1.0 : 0.0;
    double x = std::clamp((t - a) / (b - a), 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

}  // namespace

InkSplit split_neutral(double t, const BlackSplitConfig& c) {
    t = std::clamp(t, 0.0, 1.0);
    InkSplit s;
    // Light Gray: up then down (highlights → fades out by midtones).
    s.lgy = smoothstep(0.0, c.lgy_up, t) * (1.0 - smoothstep(c.lgy_dn0, c.lgy_dn1, t));
    // Gray: up in midtones, down as black takes the shadows.
    s.gy  = smoothstep(c.gy_up0, c.gy_up1, t) * (1.0 - smoothstep(c.gy_dn0, c.gy_dn1, t));
    // Main black: rises through the shadows.
    s.k   = smoothstep(c.k_up0, 1.0, t);

    s.lgy = std::min(s.lgy, c.lgy_max);
    s.gy  = std::min(s.gy,  c.gy_max);
    s.k   = std::min(s.k,   c.k_max);
    return s;
}

std::vector<InkSplit> neutral_ramp(int steps, const BlackSplitConfig& c) {
    std::vector<InkSplit> out;
    if (steps < 2) steps = 2;
    out.reserve(steps);
    for (int i = 0; i < steps; ++i)
        out.push_back(split_neutral(double(i) / (steps - 1), c));
    return out;
}

}  // namespace calibration
}  // namespace jcolour
