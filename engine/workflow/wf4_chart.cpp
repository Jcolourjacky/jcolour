#include "wf4_chart.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace jcolour {
namespace workflow {

static std::string channel_name(InkChannel ch, BlackKind kind) {
    switch (ch) {
        case InkChannel::MAIN_K: return kind == BlackKind::PK ? "PK" : "MK";
        case InkChannel::LC:     return "LC";
        case InkChannel::C:      return "C";
        case InkChannel::VLM:    return "VLM";
        case InkChannel::VM:     return "VM";
        case InkChannel::Y:      return "Y";
        case InkChannel::Or:     return "Or";
        case InkChannel::GY:     return "GY";
        case InkChannel::LGY:    return "LGY";
        case InkChannel::V:      return "V";
        case InkChannel::Gr:     return "Gr";
        default:                 return "UK";
    }
}

WF4ChartLayout generate_wf4_ink_ramp(BlackKind black_kind) {
    WF4ChartLayout layout;
    layout.black_kind = black_kind;
    layout.columns = 12;
    layout.dpi = 1440;
    layout.patch_size_mm = 10.0f;

    for (int ch = 0; ch < NUM_INK_CHANNELS; ++ch) {
        for (int step = 0; step <= 20; ++step) {
            InkPatch patch;
            patch.channel = static_cast<InkChannel>(ch);
            patch.ink_density = step * 5.0f / 100.0f;
            patch.label = channel_name(patch.channel, black_kind) + "_" +
                          std::to_string(static_cast<int>(patch.ink_density * 100));
            layout.single_ink_patches.push_back(patch);
        }
    }
    return layout;
}

WF4ChartLayout generate_wf4_til_patches(BlackKind black_kind) {
    WF4ChartLayout layout;
    layout.black_kind = black_kind;
    layout.columns = 6;
    layout.dpi = 1440;
    layout.patch_size_mm = 10.0f;

    // TIL combinations exercise the worst-case ink loadings the printer is
    // expected to encounter. MAIN_K stands in for whichever black is loaded.
    std::vector<std::vector<std::pair<InkChannel, float>>> til_combinations = {
        {{InkChannel::C, 0.5f},  {InkChannel::VM, 0.5f},  {InkChannel::Y, 0.5f}},
        {{InkChannel::C, 0.75f}, {InkChannel::VM, 0.75f}, {InkChannel::Y, 0.75f}},
        {{InkChannel::C, 1.0f},  {InkChannel::VM, 1.0f},  {InkChannel::Y, 1.0f}},
        {{InkChannel::Or, 1.0f}},
        {{InkChannel::Gr, 1.0f}},
        {{InkChannel::V,  1.0f}},
        {{InkChannel::Or, 0.8f}, {InkChannel::Gr, 0.8f}},
        {{InkChannel::C,  0.5f}, {InkChannel::VM, 0.5f}, {InkChannel::Y, 0.5f},
         {InkChannel::MAIN_K, 0.5f}},
        {{InkChannel::LC, 1.0f}, {InkChannel::VLM, 1.0f}, {InkChannel::Y, 0.5f}},
    };

    for (size_t i = 0; i < til_combinations.size(); ++i) {
        TILPatch patch;
        patch.label = "TIL_" + std::to_string(static_cast<int>(i) + 1);
        patch.channels = til_combinations[i];
        layout.til_patches.push_back(patch);
    }
    return layout;
}

WF4ChartLayout generate_wf4_calibration_chart(BlackKind black_kind) {
    auto ramp = generate_wf4_ink_ramp(black_kind);
    auto til  = generate_wf4_til_patches(black_kind);

    WF4ChartLayout combined;
    combined.black_kind = black_kind;
    combined.columns = 12;
    combined.dpi = 1440;
    combined.patch_size_mm = 10.0f;
    combined.single_ink_patches = std::move(ramp.single_ink_patches);
    combined.til_patches = std::move(til.til_patches);
    return combined;
}

WF4ChartLayout generate_wf4_verification_chart(BlackKind black_kind) {
    WF4ChartLayout layout;
    layout.black_kind = black_kind;
    layout.columns = 8;
    layout.dpi = 1440;
    layout.patch_size_mm = 10.0f;

    std::vector<std::vector<std::pair<InkChannel, float>>> verification = {
        {{InkChannel::MAIN_K, 0.1f}},
        {{InkChannel::MAIN_K, 0.25f}},
        {{InkChannel::MAIN_K, 0.5f}},
        {{InkChannel::MAIN_K, 0.75f}},
        {{InkChannel::MAIN_K, 1.0f}},
        {{InkChannel::MAIN_K, 0.3f}, {InkChannel::VLM, 0.1f}, {InkChannel::Y, 0.15f}},
        {{InkChannel::C, 0.4f}, {InkChannel::LC, 0.2f}},
        {{InkChannel::C, 0.5f}, {InkChannel::Y, 0.4f}},
        {{InkChannel::VM, 0.6f}, {InkChannel::Y, 0.6f}},
        {{InkChannel::C, 0.6f}, {InkChannel::VM, 0.4f}},
        {{InkChannel::Or, 0.8f}},
    };

    for (size_t i = 0; i < verification.size(); ++i) {
        TILPatch patch;
        patch.label = "V_" + std::to_string(static_cast<int>(i) + 1);
        patch.channels = verification[i];
        layout.til_patches.push_back(patch);
    }
    return layout;
}

WF4ChartLayout generate_wf4_extended_target(int target_patches,
                                             BlackKind black_kind) {
    WF4ChartLayout layout;
    layout.black_kind = black_kind;
    layout.columns = 16;
    layout.dpi = 1440;
    layout.patch_size_mm = 6.0f;

    // Neutral grays via MAIN_K
    for (int step = 0; step <= 20; ++step) {
        TILPatch patch;
        patch.label = "G" + std::to_string(step * 5);
        patch.channels = {{InkChannel::MAIN_K, step * 5.0f / 100.0f}};
        layout.til_patches.push_back(patch);
    }

    // Saturated colors at each gamut boundary corner
    std::vector<std::vector<std::pair<InkChannel, float>>> saturated = {
        {{InkChannel::C,  1.0f}},
        {{InkChannel::VM, 1.0f}},
        {{InkChannel::Y,  1.0f}},
        {{InkChannel::C,  1.0f}, {InkChannel::VM, 1.0f}},
        {{InkChannel::VM, 1.0f}, {InkChannel::Y,  1.0f}},
        {{InkChannel::C,  1.0f}, {InkChannel::Y,  1.0f}},
        {{InkChannel::Or, 1.0f}},
        {{InkChannel::V,  1.0f}},
        {{InkChannel::Gr, 1.0f}},
    };

    for (size_t i = 0; i < saturated.size(); ++i) {
        TILPatch patch;
        patch.label = "S" + std::to_string(static_cast<int>(i));
        patch.channels = saturated[i];
        layout.til_patches.push_back(patch);
    }

    // LUT grid (9x9x9 = 729 points) — RGB-to-CMYK GCR then mapped via
    // MAIN_K + C/VM/Y. The other 7 channels (LC, VLM, Or, GY, LGY, V, Gr)
    // are exercised by the saturated set above and the TIL combinations,
    // so they're not in this RGB grid.
    int lut_grid = 9;
    double step_size = 1.0 / (lut_grid - 1);

    for (int r_step = 0; r_step < lut_grid; ++r_step) {
        for (int g_step = 0; g_step < lut_grid; ++g_step) {
            for (int b_step = 0; b_step < lut_grid; ++b_step) {
                double r = r_step * step_size;
                double g = g_step * step_size;
                double b = b_step * step_size;

                double c = 1.0 - r;
                double m = 1.0 - g;
                double y = 1.0 - b;
                double k = std::min({c, m, y});
                c = (c - k) / (1.0 - k + 0.001);
                m = (m - k) / (1.0 - k + 0.001);
                y = (y - k) / (1.0 - k + 0.001);

                TILPatch patch;
                patch.label = "L" + std::to_string(r_step) + "_" +
                              std::to_string(g_step) + "_" +
                              std::to_string(b_step);
                if (k > 0.1)  patch.channels.push_back({InkChannel::MAIN_K, static_cast<float>(k)});
                if (c > 0.05) patch.channels.push_back({InkChannel::C,      static_cast<float>(c)});
                if (m > 0.05) patch.channels.push_back({InkChannel::VM,     static_cast<float>(m)});
                if (y > 0.05) patch.channels.push_back({InkChannel::Y,      static_cast<float>(y)});

                if (!patch.channels.empty()) {
                    layout.til_patches.push_back(patch);
                }
            }
        }
    }

    // Pad if needed — cycle through 11 active channels.
    while (static_cast<int>(layout.til_patches.size()) < target_patches) {
        TILPatch patch;
        patch.label = "E" + std::to_string(static_cast<int>(layout.til_patches.size()));
        int idx = static_cast<int>(layout.til_patches.size());
        patch.channels = {{static_cast<InkChannel>((idx * 7) % NUM_INK_CHANNELS),
                           static_cast<float>((idx * 13) % 100) / 100.0f}};
        layout.til_patches.push_back(patch);
    }

    return layout;
}

void export_wf4_chart_svg(const WF4ChartLayout& chart, const std::string& path) {
    std::ofstream out(path);
    if (!out) return;

    int total_patches = static_cast<int>(chart.single_ink_patches.size()) +
                        static_cast<int>(chart.til_patches.size());
    int rows = (total_patches + chart.columns - 1) / chart.columns;
    double margin_mm = 10.0;
    double cell_w = (210.0 - 2 * margin_mm) / chart.columns;
    double cell_h = (297.0 - 2 * margin_mm) / rows;

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"210mm\" height=\"297mm\" viewBox=\"0 0 210 297\">\n";
    out << "  <style>.patch-label { font-size: 2mm; fill: black; }</style>\n";

    int idx = 0;
    for (const auto& patch : chart.single_ink_patches) {
        int col = idx % chart.columns;
        int row = idx / chart.columns;
        double gray = 1.0 - patch.ink_density;
        int gv = static_cast<int>(gray * 255);
        char hex[8];
        snprintf(hex, sizeof(hex), "#%02x%02x%02x", gv, gv, gv);
        out << "  <rect x=\"" << margin_mm + col * cell_w << "mm\" y=\"" << margin_mm + row * cell_h << "mm\" "
            << "width=\"" << cell_w - 1 << "mm\" height=\"" << cell_h - 1 << "mm\" "
            << "fill=\"" << hex << "\" stroke=\"black\" stroke-width=\"0.1mm\"/>\n";
        out << "  <text x=\"" << margin_mm + col * cell_w + 0.5 << "mm\" y=\"" << margin_mm + row * cell_h + cell_h - 0.5 << "mm\" "
            << "class=\"patch-label\">" << patch.label << "</text>\n";
        ++idx;
    }
    out << "</svg>\n";
}

void export_wf4_chart_json(const WF4ChartLayout& chart, const std::string& path) {
    std::ofstream out(path);
    if (!out) return;

    out << "{\n";
    out << "  \"version\": \"1.0\",\n";
    out << "  \"workflow\": \"wf4\",\n";
    out << "  \"channels\": " << NUM_INK_CHANNELS << ",\n";
    out << "  \"black_kind\": \""
        << (chart.black_kind == BlackKind::PK ? "PK" : "MK") << "\",\n";
    out << "  \"dpi\": " << chart.dpi << ",\n";
    out << "  \"patch_size_mm\": " << chart.patch_size_mm << ",\n";
    out << "  \"single_ink_patches\": [\n";

    for (size_t i = 0; i < chart.single_ink_patches.size(); ++i) {
        const auto& p = chart.single_ink_patches[i];
        out << "    {\"label\": \"" << p.label << "\", \"channel\": " << static_cast<int>(p.channel)
            << ", \"density\": " << p.ink_density << "}";
        if (i < chart.single_ink_patches.size() - 1) out << ",";
        out << "\n";
    }
    out << "  ],\n  \"til_patches\": [\n";

    for (size_t i = 0; i < chart.til_patches.size(); ++i) {
        const auto& p = chart.til_patches[i];
        out << "    {\"label\": \"" << p.label << "\", \"channels\": [";
        for (size_t j = 0; j < p.channels.size(); ++j) {
            out << "{\"channel\": " << static_cast<int>(p.channels[j].first)
                << ", \"density\": " << p.channels[j].second << "}";
            if (j < p.channels.size() - 1) out << ", ";
        }
        out << "]}";
        if (i < chart.til_patches.size() - 1) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
}

}  // namespace workflow
}  // namespace jcolour
