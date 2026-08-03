#pragma once

// cmykogv_target — generate and render a CMYK+Orange+Green+Violet profiling
// target for the PRO12 printer.
//
// Why this exists: Argyll colprof cannot build N-color (>4) output profiles,
// and jcolour prints PJL-raw (no driver to own separation). So jcolour drives
// the 7 saturated colorants **directly** to characterise the full OGV-extended
// gamut. The light inks (LC/LM/GY/LGY) are dilutions handled by per-ink
// linearization, NOT gamut extenders — they stay at 0 on this target so each
// patch measures the raw colorant response (no colour management, no split).
//
// Pipeline:  generate_cmykogv_patches()  →  render_target_job()
//            →  epson::build_job()  →  Printer::send_raw_data()  (or .prn file)
//
// The chart sidecar (write_patch_list) records label + 7 device values + grid
// position so the measurement step can match i1 readings back to device input.

#include "../epson/job.h"
#include "../src/channel_layout.h"   // BlackKind
#include <array>
#include <string>
#include <vector>

namespace jcolour {
namespace calibration {

// One target patch: device drive values for C M Y K O G V, each 0..1.
struct TargetPatch {
    std::array<float, 7> cmykogv{};   // [C, M, Y, K, Or, Gr, V]
    std::string          label;
};

// Grid layout produced by render_target_job (for the sidecar + preview).
struct TargetChart {
    std::vector<TargetPatch> patches;     // only the patches actually placed
    int       cols = 0, rows = 0;
    int       placed = 0;                 // patches placed on this page
    int       pages = 1;                  // pages the full set needs
    double    patch_mm = 0, gap_mm = 0, margin_mm = 0;
    double    page_w_mm = 0, page_h_mm = 0;
    int       dpi = 0;
    BlackKind black = BlackKind::PK;
};

// Built-in deterministic CMYKOGV profiling set (white + 7 single-ink ramps +
// process/OGV secondaries + neutral ramps + a coarse CMY body cube). Count is
// approximate; the real spread is fixed and reproducible.
std::vector<TargetPatch> generate_cmykogv_patches(int approx_count = 300);

// Lay patches into a grid on one page and render to a 12-channel Epson JobSpec
// via an ordered (Bayer 8×8) dither. Only the 7 saturated channels are driven
// (lights stay empty). Returns the JobSpec for page `page` (0-based); fills
// `layout_out` with the grid actually used.
epson::JobSpec render_target_job(const std::vector<TargetPatch>& patches,
                                 BlackKind black = BlackKind::PK,
                                 int    dpi       = 360,
                                 double patch_mm  = 8.0,
                                 double gap_mm    = 1.5,
                                 double margin_mm = 10.0,
                                 double page_w_mm = 210.0,   // A4 portrait
                                 double page_h_mm = 297.0,
                                 int    page      = 0,
                                 TargetChart* layout_out = nullptr);

// Write the placed-patch list as a tab-separated sidecar (label, C..V, col,row).
bool write_patch_list(const TargetChart& chart, const std::string& path);

// Map a CMYKOGV patch to the 12-slot **epson::Channel** layout (the order
// JobSpec/PJL uses), lights = 0, respecting PK/MK exclusivity. Exposed for
// tests. NOTE: epson::Channel order differs from channel_layout.h's
// PhysicalInks order — JobSpec is authoritative here, so we map to it directly.
std::array<float, epson::CH_COUNT> patch_to_physical(const TargetPatch& p,
                                                     BlackKind black);

}  // namespace calibration
}  // namespace jcolour
