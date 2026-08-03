#include "wf1_chart.h"

namespace jcolour {
namespace workflow {

ChartLayout generate_wf1_ink_ramp() {
    ChartLayout layout;
    layout.columns = 7;        // 7 patches per row: 0%, 5%, 10%, ... 100%
    layout.dpi = 1440;
    layout.patch_size_mm = 10.0f;

    // 21 steps: 0, 5, 10, ... 100
    for (int i = 0; i <= 20; ++i) {
        Patch patch;
        patch.label = "L" + std::to_string(i * 5);
        patch.ink_density = i * 5.0f / 100.0f;
        patch.channel = 0;  // PK for workflow 1
        layout.patches.push_back(patch);
    }
    return layout;
}

ChartLayout generate_wf1_verification() {
    ChartLayout layout;
    layout.columns = 4;        // 4 patches per row
    layout.dpi = 1440;
    layout.patch_size_mm = 10.0f;

    // 11 steps for verification: 0%, 10%, 20%, ... 100%
    for (int i = 0; i <= 10; ++i) {
        Patch patch;
        patch.label = "V" + std::to_string(i * 10);
        patch.ink_density = i * 10.0f / 100.0f;
        patch.channel = 0;
        layout.patches.push_back(patch);
    }
    return layout;
}

}  // namespace workflow
}  // namespace jcolour