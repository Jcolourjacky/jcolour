#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace jcolour {
namespace epson {

// 12-channel order for UltraChrome Pro12 (P9580 / P20580 / P7580).
// Index used to address JobSpec::channel_planes.
enum Channel : int {
    CH_PK  = 0,   // Photo Black
    CH_MK  = 1,   // Matte Black
    CH_C   = 2,
    CH_M   = 3,
    CH_Y   = 4,
    CH_LC  = 5,   // Light Cyan
    CH_LM  = 6,   // Light Magenta
    CH_GY  = 7,   // Gray
    CH_LGY = 8,   // Light Gray
    CH_OR  = 9,   // Orange
    CH_GR  = 10,  // Green
    CH_V   = 11,  // Violet
    CH_COUNT = 12
};

struct JobSpec {
    int  width_px       = 0;
    int  height_px      = 0;
    int  dpi            = 1440;          // both axes
    bool bidirectional  = false;
    bool black_overcoat = false;         // BEO (engaged via PJL SET, command TBD per B7)
    std::string media_type = "PHOTORAG"; // PJL MEDIATYPE token
    std::string paper_size = "A4";       // PJL PAPERSIZE token

    // 1-bit packed raster planes, one per channel, MSB-first within each byte,
    // row-major, stride = ceil(width_px/8) bytes.  Empty plane = "channel off".
    std::array<std::vector<uint8_t>, CH_COUNT> channel_planes;
};

// ---------------------------------------------------------------------------
// STAGE-3 IN-HOUSE ESC/P2 EMITTER — NOT THE PRODUCTION PRINT PATH.
//
// The builders below emit an in-house, per-channel raster stream: one 1-bit
// plane per ink + `ESC ( $` colour-select. Despite the historical "escpr"
// naming, this is ESC/P2 raster, NOT consumer ESC/P-R (which only accepts RGB
// continuous tone and does halftone/weave in firmware). The
// `@PJL ENTER LANGUAGE=ESCPR` token emitted by build_pjl_header() is a
// PLACEHOLDER pending the P9580 language/weave/drop parameters, which must be
// pinned from real-machine .prn capture (Stage 1).
//
// Until those parameters are pinned, this emitter must NOT drive hardware:
//   - production printing goes through the process-isolated Gutenprint backend
//     (jcolour-stp-backend), keeping GPL code out of the closed engine;
//   - these builders remain available for unit tests and offline buffer / .prn
//     generation only.
//
// Hardware transmission is gated by JCOLOUR_ENABLE_INHOUSE_ESCP2 (default OFF);
// callers that send to a real printer must go through build_hardware_job().
// ---------------------------------------------------------------------------

// Build the leading PJL header (UEL + SET commands + ENTER LANGUAGE token).
std::vector<uint8_t> build_pjl_header(const JobSpec& s);

// Build the ESC/P2 raster body: init, page-format commands, per-ink planes,
// form-feed. (Stage-3 in-house emitter — see banner above.)
std::vector<uint8_t> build_escpr_block(const JobSpec& s);

// Build the trailing PJL footer (terminating UEL).
std::vector<uint8_t> build_pjl_footer();

// Convenience: header + body + footer in one buffer. For tests / offline
// buffer generation. To send to hardware use build_hardware_job() instead.
std::vector<uint8_t> build_job(const JobSpec& s);

// True if the in-house ESC/P2 emitter is permitted to drive hardware.
// Compile-time gated by JCOLOUR_ENABLE_INHOUSE_ESCP2 (default OFF).
bool inhouse_escp2_enabled();

// Hardware-bound wrapper around build_job(). Throws std::logic_error unless
// JCOLOUR_ENABLE_INHOUSE_ESCP2 is defined, so the unpinned Stage-3 emitter
// cannot reach a real printer by accident. Offline/test code calls build_job()
// directly.
std::vector<uint8_t> build_hardware_job(const JobSpec& s);

// Bytes per row for a 1-bit packed channel plane.
inline int packed_stride(int width_px) {
    return (width_px + 7) / 8;
}

// Convert millimeters to pixels at `dpi` (1 in = 25.4 mm).
inline int mm_to_px(double mm, int dpi) {
    return static_cast<int>((mm / 25.4) * dpi + 0.5);
}

// Build a JobSpec for a solid-fill block of `channel`, sized in millimeters.
// Used by the B6 acceptance test and by the calibration self-test pipeline.
JobSpec make_solid_block(Channel channel,
                         double width_mm,
                         double height_mm,
                         int dpi = 1440);

}  // namespace epson
}  // namespace jcolour
