#pragma once

#include <iosfwd>
#include <string>

// ===========================================================================
// jcolour-stp-backend — process-isolated Gutenprint bridge.
//
// WHY THIS IS A SEPARATE EXECUTABLE (GPL ISOLATION):
//   libgutenprint is GPL. The jcolour engine/app is closed-source. To keep the
//   GPL boundary clean, ONLY this backend executable links libgutenprint. The
//   engine talks to it across a process boundary: it writes a manifest (+ a raw
//   RGB input file), runs the backend, and reads back the ESC/P2 byte stream.
//   No engine/app object ever links GPL code.
//
//   Stage 0 uses the SC-P9000 driver (escp2-p9000) as the base model for the
//   real P9580 — same PrecisionCore TFP head / ESC/P2 core. The P9580-only
//   deltas (V channel, media tokens, weave) are pinned later (Stage 1, .prn).
// ===========================================================================

namespace jcolour {
namespace stp {

// Wire contract between the closed engine and this backend. Serialized as a
// simple `key=value` text manifest (one per line; `#` comments allowed).
struct StpJobRequest {
    // Gutenprint driver id. Stage-0 base model for the P9580 is the P9000.
    std::string model     = "escp2-p9000";

    // Raw 8-bit interleaved RGB input, width_px * height_px * 3 bytes.
    std::string input;

    // Destination for the rendered ESC/P2 byte stream ("-" = stdout).
    std::string output    = "-";

    int         width_px  = 0;
    int         height_px = 0;
    int         dpi       = 1440;

    // Gutenprint parameter names (PageSize / MediaType / Resolution / Quality).
    // These must be values the chosen driver accepts (e.g. for escp2-p9000:
    // MediaType "Luster"/"Matte"/"Photo"/..., PageSize "A4"/"Letter"/...).
    std::string media     = "Luster";
    std::string pagesize  = "A4";
    std::string resolution;            // empty = driver default for dpi
    int         quality   = 4;

    double      ink_limit = 1.0;       // Density multiplier (1.0 = nominal).
};

// Parse a key=value manifest from `in`. Returns false and fills `err` on a
// malformed line or an out-of-range numeric value. Unknown keys are ignored
// with no error (forward-compat with newer engine builds).
bool parse_manifest(std::istream& in, StpJobRequest& out, std::string& err);

// Serialize `req` as a key=value manifest (engine side / round-trip tests).
void write_manifest(std::ostream& out, const StpJobRequest& req);

// Drive Gutenprint to render `req` into ESC/P2. Returns a process exit code
// (0 = success, non-zero = failure). Diagnostics are written to `err`.
//
// When the backend is built WITHOUT libgutenprint (JCOLOUR_HAVE_GUTENPRINT
// undefined), this returns a non-zero "not configured" error so the skeleton
// still compiles and links on machines that do not yet have Gutenprint.
int run_stp_backend(const StpJobRequest& req, std::ostream& err);

}  // namespace stp
}  // namespace jcolour
