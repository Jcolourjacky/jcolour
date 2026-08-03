#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace jcolour {
namespace calibration {

// A single patch in a self-test chart.
struct Patch {
    std::string label;       // e.g. "C100", "K50", "W"
    float       cmyk[4];     // 0.0–1.0
};

// Layout of a self-test chart for a given workflow.
struct ChartLayout {
    int                  workflow;
    std::vector<Patch>   patches;
};

// Build the self-test chart for the given workflow (1–4).
// Throws std::invalid_argument for unknown workflow IDs.
ChartLayout build_self_test_chart(int workflow);

}  // namespace calibration
}  // namespace jcolour
