#include "charts.h"
#include <stdexcept>

namespace jcolour {
namespace calibration {

// ---------------------------------------------------------------------------
// Patch helpers
// ---------------------------------------------------------------------------

static Patch make_patch(const char* label, float c, float m, float y, float k) {
    Patch p;
    p.label   = label;
    p.cmyk[0] = c;
    p.cmyk[1] = m;
    p.cmyk[2] = y;
    p.cmyk[3] = k;
    return p;
}

// ---------------------------------------------------------------------------
// Workflow patch sets
// ---------------------------------------------------------------------------

// Workflow 1 — 6 patches: white + 4 primaries + black
static std::vector<Patch> workflow1_patches() {
    return {
        make_patch("W",    0.0f, 0.0f, 0.0f, 0.0f),
        make_patch("C100", 1.0f, 0.0f, 0.0f, 0.0f),
        make_patch("M100", 0.0f, 1.0f, 0.0f, 0.0f),
        make_patch("Y100", 0.0f, 0.0f, 1.0f, 0.0f),
        make_patch("K100", 0.0f, 0.0f, 0.0f, 1.0f),
        make_patch("CMY",  1.0f, 1.0f, 1.0f, 0.0f),
    };
}

// Workflow 2 — 19 patches: workflow1 + 13 tone-scale steps
static std::vector<Patch> workflow2_patches() {
    auto patches = workflow1_patches();
    // Neutral ramp K 10%–90% in 10% steps (9 patches)
    for (int i = 1; i <= 9; ++i) {
        float k = i * 0.1f;
        char label[16];
        snprintf(label, sizeof(label), "K%d", i * 10);
        patches.push_back(make_patch(label, 0.0f, 0.0f, 0.0f, k));
    }
    // CMY secondaries at 50%
    patches.push_back(make_patch("C50M50", 0.5f, 0.5f, 0.0f, 0.0f));
    patches.push_back(make_patch("C50Y50", 0.5f, 0.0f, 0.5f, 0.0f));
    patches.push_back(make_patch("M50Y50", 0.0f, 0.5f, 0.5f, 0.0f));
    patches.push_back(make_patch("C50",    0.5f, 0.0f, 0.0f, 0.0f));
    // Total: 6 + 9 + 4 = 19
    return patches;
}

// Workflow 3 — 22 patches: workflow2 + 3 overprint patches
static std::vector<Patch> workflow3_patches() {
    auto patches = workflow2_patches();
    patches.push_back(make_patch("CMYK50",  0.5f, 0.5f, 0.5f, 0.5f));
    patches.push_back(make_patch("C100K50", 1.0f, 0.0f, 0.0f, 0.5f));
    patches.push_back(make_patch("M100K50", 0.0f, 1.0f, 0.0f, 0.5f));
    // Total: 19 + 3 = 22
    return patches;
}

// Workflow 4 — 29 patches: workflow3 + 7 highlight/shadow patches
static std::vector<Patch> workflow4_patches() {
    auto patches = workflow3_patches();
    // Highlight steps C 5%, 10%, 15%
    patches.push_back(make_patch("C5",  0.05f, 0.0f, 0.0f, 0.0f));
    patches.push_back(make_patch("C10", 0.10f, 0.0f, 0.0f, 0.0f));
    patches.push_back(make_patch("C15", 0.15f, 0.0f, 0.0f, 0.0f));
    // Shadow steps K 85%, 90%, 95%
    patches.push_back(make_patch("K85", 0.0f, 0.0f, 0.0f, 0.85f));
    patches.push_back(make_patch("K90", 0.0f, 0.0f, 0.0f, 0.90f));
    patches.push_back(make_patch("K95", 0.0f, 0.0f, 0.0f, 0.95f));
    // Rich black
    patches.push_back(make_patch("RICH_K", 0.4f, 0.3f, 0.3f, 1.0f));
    // Total: 22 + 7 = 29
    return patches;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

ChartLayout build_self_test_chart(int workflow) {
    ChartLayout layout;
    layout.workflow = workflow;
    switch (workflow) {
        case 1: layout.patches = workflow1_patches(); break;
        case 2: layout.patches = workflow2_patches(); break;
        case 3: layout.patches = workflow3_patches(); break;
        case 4: layout.patches = workflow4_patches(); break;
        default:
            throw std::invalid_argument("unknown workflow id: " +
                                        std::to_string(workflow));
    }
    return layout;
}

}  // namespace calibration
}  // namespace jcolour
