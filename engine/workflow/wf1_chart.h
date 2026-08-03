#pragma once

#include <vector>
#include <string>

namespace jcolour {
namespace workflow {

// Single patch in a calibration chart
struct Patch {
    std::string label;
    float       ink_density = 0.0f;  // 0.0..1.0 for single-channel
    int         channel = 0;         // 0=PK, 1=MK, 2=GY, 3=LGY for Wf2; 0=PK for Wf1
};

// Layout describing how patches should be printed on a page
struct ChartLayout {
    std::vector<Patch> patches;
    int                 columns = 6;   // patches per row
    int                 dpi = 1440;   // print resolution
    float               patch_size_mm = 10.0f;  // width/height of each patch
};

// Generate ink density ramp for single-black workflow calibration.
// Returns 21 patches from 0% to 100% ink coverage.
ChartLayout generate_wf1_ink_ramp();

// Generate verification chart (fewer patches, for validation).
// Returns ~11 patches spanning the density range.
ChartLayout generate_wf1_verification();

// Export chart as PNG for printing (requires libtiff or Qt for rendering).
// void export_chart_as_png(const ChartLayout& chart, const std::string& path);

}  // namespace workflow
}  // namespace jcolour