#pragma once

// black_split — sub-problem (A) of jcolour's self-managed separation: split a
// neutral tone across the three same-hue black/grey inks of the PRO12 head
// (MAIN_K = PK or MK · GY = Gray · LGY = Light Gray).
//
// Approach is the QuadToneRIP-style "partition curve" method (a documented,
// non-proprietary technique — see docs/separation-strategy.md): each ink owns
// a tonal window; light ink leads the highlights, mid grey the midtones, full
// black the shadows, with smooth (smoothstep) crossovers so no single channel
// jumps. This is the layer that lives BELOW the device-RGB ICC (PIPELINE §6
// correction) — it is colour-management-free, pure device-side ink allocation.
//
// Pairs with §5 per-ink linearization: the crossover shapes here give a smooth
// monotonic neutral build; linearization then makes L* vs input linear.

#include <vector>

namespace jcolour {
namespace calibration {

// Coverage (0..1) of the three neutral inks for one input tone.
struct InkSplit {
    double k   = 0;   // MAIN_K (active PK/MK) — shadows
    double gy  = 0;   // Gray   — midtones
    double lgy = 0;   // Light Gray — highlights
    double total() const { return k + gy + lgy; }
};

// Crossover knots (input tone 0=white … 1=black). Defaults follow a typical
// 3-ink piezo neutral build; all tunable from the curve editor later.
struct BlackSplitConfig {
    // Light Gray: rises over [0, lgy_up], falls over [lgy_dn0, lgy_dn1].
    double lgy_up = 0.35, lgy_dn0 = 0.45, lgy_dn1 = 0.78;
    // Gray: rises over [gy_up0, gy_up1], falls over [gy_dn0, gy_dn1].
    double gy_up0 = 0.28, gy_up1 = 0.60, gy_dn0 = 0.72, gy_dn1 = 0.94;
    // Main black: rises over [k_up0, 1.0].
    double k_up0 = 0.55;
    // Per-ink coverage caps (head/paper limits).
    double lgy_max = 1.0, gy_max = 1.0, k_max = 1.0;
};

// Split one neutral input tone t∈[0,1] into the 3 inks.
InkSplit split_neutral(double t, const BlackSplitConfig& cfg = {});

// Sample the split across `steps` evenly-spaced input tones (incl. 0 and 1).
std::vector<InkSplit> neutral_ramp(int steps, const BlackSplitConfig& cfg = {});

}  // namespace calibration
}  // namespace jcolour
