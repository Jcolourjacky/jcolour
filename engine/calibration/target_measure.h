#pragma once

// target_measure — ingest the MEASURED CMYKOGV target and join readings back to
// device values, readying the i1Pro3 path (PIPELINE §2/§9). Flow:
//   print target (cmykogv_target) → i1 measures → CGATS .ti3
//   + write_patch_list() sidecar (device values, in print order)
//   → join_measurement() → device↔Lab dataset → gamut analysis / profile feed.
//
// Hardware-free testable: a synthetic .ti3 (KM forward over the device values)
// exercises the whole parse→join→analyse loop minus the instrument.

#include <array>
#include <string>
#include <vector>

namespace jcolour {
namespace calibration {

// One joined record: device drive (C M Y K Or Gr V) ↔ measured Lab.
struct MeasuredPatch {
    std::string         label;
    std::array<float, 7> cmykogv{};
    double L = 0, a = 0, b = 0;
    double chroma() const;          // hypot(a, b)
    double hue() const;             // atan2(b, a) degrees, 0..360
};

// Parse the device-value sidecar written by write_patch_list().
std::vector<MeasuredPatch> parse_patch_list(const std::string& path);

// Parse a CGATS / Argyll .ti3 with SAMPLE_ID + LAB_L/LAB_A/LAB_B columns.
struct CgatsRow { std::string id; double L, a, b; };
std::vector<CgatsRow> parse_cgats_lab(const std::string& path);

// Join sidecar (device) with CGATS (Lab) by print order (row i ↔ row i).
// Returns empty if counts differ.
std::vector<MeasuredPatch> join_measurement(const std::string& patch_list_tsv,
                                            const std::string& cgats_ti3);

// OGV gamut headroom from the measured set: how far each extended-ink corner
// (pure Or/Gr/V full tone) sits BEYOND the CMY-only point cloud — the nearest
// CMYK ΔEab to that corner (large ⇒ unreachable by CMYK ⇒ real extension).
// Consistent with the (B) inversion spike, but Lab-only (no forward model).
struct GamutSummary {
    double or_corner_C = 0, gr_corner_C = 0, v_corner_C = 0;  // corner chroma
    double or_dist = 0, gr_dist = 0, v_dist = 0;              // nearest CMY ΔEab
};
GamutSummary summarize_gamut(const std::vector<MeasuredPatch>& m);

}  // namespace calibration
}  // namespace jcolour
