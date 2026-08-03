#include "wf3_chart.h"
#include <fstream>

namespace jcolour {
namespace workflow {

static std::string channel_name(Wf3Channel ch, BlackKind black_kind) {
    switch (ch) {
        case Wf3Channel::MAIN_K:
            return black_kind == BlackKind::PK ? "PK" : "MK";
        case Wf3Channel::GY:  return "GY";
        case Wf3Channel::LGY: return "LGY";
        case Wf3Channel::C:   return "C";
        case Wf3Channel::VM:  return "VM";
        case Wf3Channel::Y:   return "Y";
        default:              return "UK";
    }
}

Wf3ChartLayout generate_wf3_ink_ramp(BlackKind black_kind) {
    Wf3ChartLayout layout;
    layout.black_kind = black_kind;
    layout.columns = 8;
    layout.dpi = 1440;
    layout.patch_size_mm = 10.0f;

    // 6 channels × 21 steps = 126 patches
    for (int ch = 0; ch < WF3_NUM_CHANNELS; ++ch) {
        for (int step = 0; step <= 20; ++step) {
            Wf3Patch patch;
            patch.channel = static_cast<Wf3Channel>(ch);
            patch.ink_density = step * 5.0f / 100.0f;
            patch.label = channel_name(patch.channel, black_kind) + "_" +
                          std::to_string(static_cast<int>(patch.ink_density * 100));
            layout.patches.push_back(patch);
        }
    }
    return layout;
}

Wf3ChartLayout generate_wf3_cellular_patches(BlackKind black_kind) {
    Wf3ChartLayout layout;
    layout.black_kind = black_kind;
    layout.columns = 6;
    layout.dpi = 1440;
    layout.patch_size_mm = 10.0f;

    // F22 audit fix: real multi-channel cellular patches via Wf3MultiPatch.
    // Each entry represents an actual multi-ink mix printed as a single
    // patch — what the CYNSN model needs for cross-channel interaction
    // training. Previously these were split into per-channel patches that
    // disguised the multi-ink intent.
    auto add_single = [&](const std::string& label, Wf3Channel ch, float dens) {
        Wf3MultiPatch p;
        p.label = label;
        p.channels.push_back({ch, dens});
        layout.multi_patches.push_back(p);
    };
    auto add_multi = [&](const std::string& label,
                          std::vector<std::pair<Wf3Channel, float>> combo) {
        Wf3MultiPatch p;
        p.label = label;
        p.channels = std::move(combo);
        layout.multi_patches.push_back(p);
    };

    // Single-ink density references for each channel.
    int n = 0;
    add_single("CELL_" + std::to_string(n++), Wf3Channel::MAIN_K, 0.3f);
    add_single("CELL_" + std::to_string(n++), Wf3Channel::MAIN_K, 0.5f);
    add_single("CELL_" + std::to_string(n++), Wf3Channel::MAIN_K, 0.7f);
    add_single("CELL_" + std::to_string(n++), Wf3Channel::C,  0.2f);
    add_single("CELL_" + std::to_string(n++), Wf3Channel::C,  0.4f);
    add_single("CELL_" + std::to_string(n++), Wf3Channel::VM, 0.2f);
    add_single("CELL_" + std::to_string(n++), Wf3Channel::VM, 0.4f);
    add_single("CELL_" + std::to_string(n++), Wf3Channel::Y,  0.2f);
    add_single("CELL_" + std::to_string(n++), Wf3Channel::Y,  0.4f);

    // True multi-ink patches — black + CMY co-printed.
    add_multi("CELL_" + std::to_string(n++),
              {{Wf3Channel::MAIN_K, 0.3f},
               {Wf3Channel::C,      0.2f},
               {Wf3Channel::VM,     0.2f}});
    add_multi("CELL_" + std::to_string(n++),
              {{Wf3Channel::MAIN_K, 0.5f},
               {Wf3Channel::C,      0.3f},
               {Wf3Channel::VM,     0.3f}});

    return layout;
}

Wf3ChartLayout generate_wf3_verification(BlackKind black_kind) {
    Wf3ChartLayout layout;
    layout.black_kind = black_kind;
    layout.columns = 5;
    layout.dpi = 1440;
    layout.patch_size_mm = 10.0f;

    // 21 neutral steps via MAIN_K
    for (int step = 0; step <= 20; ++step) {
        Wf3Patch patch;
        patch.channel = Wf3Channel::MAIN_K;
        patch.ink_density = step * 5.0f / 100.0f;
        patch.label = "N" + std::to_string(static_cast<int>(patch.ink_density * 100));
        layout.patches.push_back(patch);
    }

    return layout;
}

Wf3ChartLayout generate_wf3_calibration_chart(BlackKind black_kind) {
    auto ramp = generate_wf3_ink_ramp(black_kind);
    auto cellular = generate_wf3_cellular_patches(black_kind);

    Wf3ChartLayout combined;
    combined.black_kind = black_kind;
    combined.columns = 8;
    combined.dpi = 1440;
    combined.patch_size_mm = 10.0f;
    combined.patches = std::move(ramp.patches);
    combined.patches.insert(combined.patches.end(),
                            cellular.patches.begin(), cellular.patches.end());
    combined.multi_patches = std::move(cellular.multi_patches);

    return combined;
}

}  // namespace workflow
}  // namespace jcolour
