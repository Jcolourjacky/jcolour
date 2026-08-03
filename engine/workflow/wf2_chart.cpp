#include "wf2_chart.h"
#include <fstream>
#include <sstream>

namespace jcolour {
namespace workflow {

static std::string channel_name(Wf2Channel ch, BlackKind black_kind) {
    switch (ch) {
        case Wf2Channel::MAIN_K:
            return black_kind == BlackKind::PK ? "PK" : "MK";
        case Wf2Channel::GY:  return "GY";
        case Wf2Channel::LGY: return "LGY";
        default:              return "UK";
    }
}

Wf2ChartLayout generate_wf2_ink_ramp(BlackKind black_kind) {
    Wf2ChartLayout layout;
    layout.black_kind = black_kind;
    layout.columns = 8;
    layout.dpi = 1440;
    layout.patch_size_mm = 10.0f;

    for (int ch = 0; ch < WF2_NUM_CHANNELS; ++ch) {
        for (int step = 0; step <= 20; ++step) {
            Wf2Patch patch;
            patch.channel = static_cast<Wf2Channel>(ch);
            patch.ink_density = step * 5.0f / 100.0f;
            patch.label = channel_name(patch.channel, black_kind) + "_" +
                          std::to_string(static_cast<int>(patch.ink_density * 100));
            layout.patches.push_back(patch);
        }
    }
    return layout;
}

Wf2NeugebauerPatches generate_wf2_neugebauer_primaries() {
    Wf2NeugebauerPatches primaries;

    // 3 single-ink primaries
    primaries.primaries.push_back({Wf2Channel::MAIN_K});
    primaries.primaries.push_back({Wf2Channel::GY});
    primaries.primaries.push_back({Wf2Channel::LGY});

    // 3 double combinations (MAIN_K+GY, MAIN_K+LGY, GY+LGY)
    primaries.primaries.push_back({Wf2Channel::MAIN_K, Wf2Channel::GY});
    primaries.primaries.push_back({Wf2Channel::MAIN_K, Wf2Channel::LGY});
    primaries.primaries.push_back({Wf2Channel::GY, Wf2Channel::LGY});

    // 1 triple combination (all three)
    primaries.primaries.push_back({Wf2Channel::MAIN_K, Wf2Channel::GY, Wf2Channel::LGY});

    return primaries;
}

Wf2ChartLayout generate_wf2_verification(BlackKind black_kind) {
    Wf2ChartLayout layout;
    layout.black_kind = black_kind;
    layout.columns = 5;
    layout.dpi = 1440;
    layout.patch_size_mm = 10.0f;

    // 21 neutral steps via MAIN_K
    for (int step = 0; step <= 20; step += 1) {
        Wf2Patch patch;
        patch.channel = Wf2Channel::MAIN_K;
        patch.ink_density = step * 5.0f / 100.0f;
        patch.label = "N" + std::to_string(step * 5);
        layout.patches.push_back(patch);
    }

    // Mixed tone samples — exercises the 3 channels at intermediate densities
    std::vector<std::pair<Wf2Channel, float>> mixed_tones = {
        {Wf2Channel::MAIN_K, 0.3f},
        {Wf2Channel::MAIN_K, 0.2f},
        {Wf2Channel::GY, 0.3f},
        {Wf2Channel::GY, 0.5f},
        {Wf2Channel::LGY, 0.6f},
    };

    for (size_t i = 0; i < mixed_tones.size(); ++i) {
        Wf2Patch patch;
        patch.channel = mixed_tones[i].first;
        patch.ink_density = mixed_tones[i].second;
        patch.label = "M" + std::to_string(static_cast<int>(i));
        layout.patches.push_back(patch);
    }

    return layout;
}

Wf2ChartLayout generate_wf2_calibration_chart(BlackKind black_kind) {
    auto ramp = generate_wf2_ink_ramp(black_kind);
    auto verification = generate_wf2_verification(black_kind);

    Wf2ChartLayout combined;
    combined.black_kind = black_kind;
    combined.columns = 8;
    combined.dpi = 1440;
    combined.patch_size_mm = 10.0f;

    combined.patches = ramp.patches;
    for (const auto& p : verification.patches) {
        combined.patches.push_back(p);
    }

    return combined;
}

void export_wf2_chart_json(const Wf2ChartLayout& chart, const std::string& path) {
    std::ofstream out(path);
    if (!out) return;

    out << "{\n";
    out << "  \"version\": \"1.0\",\n";
    out << "  \"workflow\": \"wf2\",\n";
    out << "  \"channels\": " << WF2_NUM_CHANNELS << ",\n";
    out << "  \"black_kind\": \""
        << (chart.black_kind == BlackKind::PK ? "PK" : "MK") << "\",\n";
    out << "  \"dpi\": " << chart.dpi << ",\n";
    out << "  \"patch_size_mm\": " << chart.patch_size_mm << ",\n";
    out << "  \"patches\": [\n";

    for (size_t i = 0; i < chart.patches.size(); ++i) {
        const auto& p = chart.patches[i];
        out << "    {\"label\": \"" << p.label
            << "\", \"channel\": " << static_cast<int>(p.channel)
            << ", \"density\": " << p.ink_density << "}";
        if (i < chart.patches.size() - 1) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

}  // namespace workflow
}  // namespace jcolour
