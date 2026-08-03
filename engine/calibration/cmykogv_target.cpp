#include "cmykogv_target.h"

#include <cmath>
#include <cstdio>
#include <fstream>

namespace jcolour {
namespace calibration {

namespace {

constexpr int C = 0, M = 1, Y = 2, K = 3, O = 4, G = 5, V = 6;

TargetPatch mk(const char* label, std::array<float, 7> v) {
    TargetPatch p;
    p.cmykogv = v;
    p.label = label;
    return p;
}

void single_ramp(std::vector<TargetPatch>& out, int chan, const char* tag) {
    // 0 excluded (that's the white patch); 12.5% steps up to 100%.
    for (int s = 1; s <= 8; ++s) {
        std::array<float, 7> v{};
        v[chan] = s / 8.0f;
        char lbl[24];
        std::snprintf(lbl, sizeof(lbl), "%s%d", tag, s * 100 / 8);
        out.push_back(mk(lbl, v));
    }
}

void two_ch(std::vector<TargetPatch>& out, int a, int b, const char* tag) {
    const float levels[] = {0.25f, 0.5f, 0.75f, 1.0f};
    for (float la : levels)
        for (float lb : levels) {
            std::array<float, 7> v{};
            v[a] = la; v[b] = lb;
            char lbl[32];
            std::snprintf(lbl, sizeof(lbl), "%s%d_%d", tag,
                          (int)(la * 100), (int)(lb * 100));
            out.push_back(mk(lbl, v));
        }
}

// 8×8 Bayer ordered-dither threshold matrix, values 0..63.
constexpr int kBayer8[8][8] = {
    { 0,48,12,60, 3,51,15,63},{32,16,44,28,35,19,47,31},
    { 8,56, 4,52,11,59, 7,55},{40,24,36,20,43,27,39,23},
    { 2,50,14,62, 1,49,13,61},{34,18,46,30,33,17,45,29},
    {10,58, 6,54, 9,57, 5,53},{42,26,38,22,41,25,37,21}};

}  // namespace

std::vector<TargetPatch> generate_cmykogv_patches(int approx_count) {
    std::vector<TargetPatch> p;
    p.push_back(mk("W", {}));                       // paper white

    // 7 single-ink ramps (8 steps each = 56) — raw per-colorant response.
    single_ramp(p, C, "C");  single_ramp(p, M, "M");  single_ramp(p, Y, "Y");
    single_ramp(p, K, "K");  single_ramp(p, O, "Or"); single_ramp(p, G, "Gr");
    single_ramp(p, V, "V");

    // Process + OGV-neighbour secondaries (4×4 each) to map the gamut edges.
    two_ch(p, C, M, "CM");  two_ch(p, C, Y, "CY");  two_ch(p, M, Y, "MY");
    two_ch(p, O, Y, "OY");  // orange↔yellow edge
    two_ch(p, G, C, "GC");  // green↔cyan edge
    two_ch(p, V, M, "VM");  // violet↔magenta edge

    // Neutral ramps: CMY composite grey and K, for grey-balance anchoring.
    for (int s = 1; s <= 6; ++s) {
        float g = s / 6.0f;
        std::array<float, 7> v{}; v[C] = g; v[M] = g; v[Y] = g;
        char lbl[24]; std::snprintf(lbl, sizeof(lbl), "Gry%d", s * 100 / 6);
        p.push_back(mk(lbl, v));
    }

    // Coarse CMY body cube (4³ = 64) so the profiler sees the interior.
    const float lv[] = {0.0f, 0.33f, 0.66f, 1.0f};
    for (float c : lv) for (float m : lv) for (float y : lv) {
        if (c == 0 && m == 0 && y == 0) continue;   // skip duplicate white
        std::array<float, 7> v{}; v[C] = c; v[M] = m; v[Y] = y;
        char lbl[32];
        std::snprintf(lbl, sizeof(lbl), "Q%d%d%d", (int)(c*3),(int)(m*3),(int)(y*3));
        p.push_back(mk(lbl, v));
    }

    // If a larger set is asked for, densify single-ink ramps to 16 steps.
    if (approx_count > (int)p.size() + 100) {
        for (int chan : {C, M, Y, K, O, G, V})
            for (int s = 1; s <= 15; s += 2) {
                std::array<float, 7> v{}; v[chan] = s / 16.0f;
                char lbl[24]; std::snprintf(lbl, sizeof(lbl), "f%d_%d", chan, s);
                p.push_back(mk(lbl, v));
            }
    }
    return p;
}

std::array<float, epson::CH_COUNT> patch_to_physical(const TargetPatch& p,
                                                     BlackKind black) {
    std::array<float, epson::CH_COUNT> ph{};      // epson::Channel order
    ph[epson::CH_C]  = p.cmykogv[C];
    ph[epson::CH_M]  = p.cmykogv[M];
    ph[epson::CH_Y]  = p.cmykogv[Y];
    ph[epson::CH_OR] = p.cmykogv[O];
    ph[epson::CH_GR] = p.cmykogv[G];
    ph[epson::CH_V]  = p.cmykogv[V];
    ph[black == BlackKind::PK ? epson::CH_PK : epson::CH_MK] = p.cmykogv[K];
    // Lights CH_LC / CH_LM / CH_GY / CH_LGY stay 0: raw colorant measurement.
    return ph;
}

epson::JobSpec render_target_job(const std::vector<TargetPatch>& patches,
                                 BlackKind black, int dpi,
                                 double patch_mm, double gap_mm, double margin_mm,
                                 double page_w_mm, double page_h_mm,
                                 int page, TargetChart* layout_out) {
    using namespace epson;
    JobSpec s;
    s.dpi       = dpi;
    s.width_px  = mm_to_px(page_w_mm, dpi);
    s.height_px = mm_to_px(page_h_mm, dpi);

    const int patch_px = mm_to_px(patch_mm, dpi);
    const int gap_px   = mm_to_px(gap_mm, dpi);
    const int marg_px  = mm_to_px(margin_mm, dpi);
    const int cell     = patch_px + gap_px;
    const int cols     = std::max(1, (s.width_px  - 2 * marg_px + gap_px) / cell);
    const int rows     = std::max(1, (s.height_px - 2 * marg_px + gap_px) / cell);
    const int per_page = cols * rows;
    const int total    = (int)patches.size();
    const int pages    = (total + per_page - 1) / per_page;
    const int start    = page * per_page;
    const int end      = std::min(total, start + per_page);

    const int stride = packed_stride(s.width_px);
    for (auto& plane : s.channel_planes) plane.assign(stride * s.height_px, 0);

    TargetChart chart;
    chart.cols = cols; chart.rows = rows; chart.pages = pages;
    chart.patch_mm = patch_mm; chart.gap_mm = gap_mm; chart.margin_mm = margin_mm;
    chart.page_w_mm = page_w_mm; chart.page_h_mm = page_h_mm;
    chart.dpi = dpi; chart.black = black;

    auto set_bit = [&](std::vector<uint8_t>& plane, int x, int y) {
        plane[y * stride + (x >> 3)] |= (0x80 >> (x & 7));
    };

    for (int i = start; i < end; ++i) {
        const TargetPatch& p = patches[i];
        const int gi = i - start;
        const int cx = marg_px + (gi % cols) * cell;
        const int cy = marg_px + (gi / cols) * cell;
        const PhysicalInks phys = patch_to_physical(p, black);

        for (int ch = 0; ch < NUM_PHYSICAL_CHANNELS; ++ch) {
            const float v = phys[ch];
            if (v <= 0.0f) continue;
            auto& plane = s.channel_planes[ch];
            if (plane.empty()) plane.assign(stride * s.height_px, 0);
            const int thr = (int)std::lround(v * 64.0f);   // 0..64
            for (int dy = 0; dy < patch_px; ++dy) {
                const int yy = cy + dy;
                if (yy >= s.height_px) break;
                for (int dx = 0; dx < patch_px; ++dx) {
                    const int xx = cx + dx;
                    if (xx >= s.width_px) break;
                    if (kBayer8[dy & 7][dx & 7] < thr) set_bit(plane, xx, yy);
                }
            }
        }
        chart.patches.push_back(p);
    }
    // Drop any channel left all-zero so build_job treats it as "off".
    for (auto& plane : s.channel_planes) {
        bool any = false;
        for (uint8_t b : plane) { if (b) { any = true; break; } }
        if (!any) plane.clear();
    }

    chart.placed = end - start;
    if (layout_out) *layout_out = chart;
    return s;
}

bool write_patch_list(const TargetChart& chart, const std::string& path) {
    std::ofstream f(path);
    if (!f) return false;
    f << "# jcolour CMYKOGV target — " << chart.placed << " patches · "
      << chart.cols << "x" << chart.rows << " grid · " << chart.dpi << "dpi · black="
      << (chart.black == BlackKind::PK ? "PK" : "MK") << "\n";
    f << "LABEL\tC\tM\tY\tK\tOr\tGr\tV\tCOL\tROW\n";
    for (int i = 0; i < (int)chart.patches.size(); ++i) {
        const auto& p = chart.patches[i];
        f << p.label;
        for (float v : p.cmykogv) f << '\t' << v;
        f << '\t' << (i % chart.cols) << '\t' << (i / chart.cols) << '\n';
    }
    return true;
}

}  // namespace calibration
}  // namespace jcolour
