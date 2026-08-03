#include "job.h"

#include <sstream>
#include <stdexcept>

namespace jcolour {
namespace epson {

namespace {

constexpr uint8_t  ESC = 0x1B;
constexpr uint8_t  FF  = 0x0C;
constexpr const char* UEL = "\x1B%-12345X";

void append_str(std::vector<uint8_t>& out, const std::string& s) {
    out.insert(out.end(), s.begin(), s.end());
}

void append_bytes(std::vector<uint8_t>& out, std::initializer_list<uint8_t> bs) {
    out.insert(out.end(), bs.begin(), bs.end());
}

// ESC/P2 "ESC ( <cmd> <nL> <nH> <data...>" extended-command envelope.
void append_remote(std::vector<uint8_t>& out, char cmd,
                   std::initializer_list<uint8_t> data) {
    out.push_back(ESC);
    out.push_back('(');
    out.push_back(static_cast<uint8_t>(cmd));
    uint16_t n = static_cast<uint16_t>(data.size());
    out.push_back(static_cast<uint8_t>(n & 0xFF));
    out.push_back(static_cast<uint8_t>((n >> 8) & 0xFF));
    out.insert(out.end(), data.begin(), data.end());
}

// Map our 12-channel index to the ESC/P2 colour id used in `ESC ( $`.
// Reference: Epson SC-P9000/P20000-series ESC/P2 "Set printing color"
// extended command. Values that are not yet confirmed for P9580 are flagged
// in B7 (BEO + 12-channel exploration) and will be re-pinned there.
uint8_t channel_color_code(Channel ch) {
    switch (ch) {
        case CH_PK:  return 0x00;
        case CH_MK:  return 0x01;
        case CH_C:   return 0x02;
        case CH_M:   return 0x03;
        case CH_Y:   return 0x04;
        case CH_LC:  return 0x05;
        case CH_LM:  return 0x06;
        case CH_GY:  return 0x07;
        case CH_LGY: return 0x08;
        case CH_OR:  return 0x09;
        case CH_GR:  return 0x0A;
        case CH_V:   return 0x0B;
        default:     return 0x00;
    }
}

}  // namespace

std::vector<uint8_t> build_pjl_header(const JobSpec& s) {
    std::vector<uint8_t> out;

    // Universal Exit Language → enter PJL.
    append_str(out, UEL);
    append_str(out, "@PJL\r\n");

    // Job context.
    std::ostringstream ss;
    ss << "@PJL SET PAPERSIZE=" << s.paper_size << "\r\n"
       << "@PJL SET MEDIATYPE=" << s.media_type << "\r\n"
       << "@PJL SET RESOLUTION=" << s.dpi << "\r\n"
       << "@PJL SET DIRECTION=" << (s.bidirectional ? "BIDIR" : "UNIDIR") << "\r\n"
       << "@PJL SET BLACKOVERCOAT=" << (s.black_overcoat ? "ON" : "OFF") << "\r\n"
       // STAGE-3 PLACEHOLDER: the body below is ESC/P2 raster (per-ink planes),
       // not consumer ESC/P-R. The correct LANGUAGE token for the P9580 native
       // mode must be pinned from real-machine .prn capture (Stage 1). Kept as
       // ESCPR for now only so structural tests have a stable string to anchor.
       << "@PJL ENTER LANGUAGE=ESCPR\r\n";
    append_str(out, ss.str());

    return out;
}

std::vector<uint8_t> build_pjl_footer() {
    std::vector<uint8_t> out;
    append_str(out, UEL);
    append_str(out, "@PJL EOJ\r\n");
    append_str(out, UEL);
    return out;
}

std::vector<uint8_t> build_escpr_block(const JobSpec& s) {
    if (s.width_px <= 0 || s.height_px <= 0) {
        throw std::invalid_argument("build_escpr_block: empty page dimensions");
    }
    const int stride = packed_stride(s.width_px);
    const int dpi    = s.dpi;

    std::vector<uint8_t> out;

    // ESC @ : reset the printer to defaults.
    append_bytes(out, {ESC, '@'});

    // ESC ( G 1 0 1 : enter graphics mode.
    append_remote(out, 'G', {0x01});

    // ESC ( U 5 0 <p> <p> <p> <m_lo> <m_hi> :
    //   page unit, vertical unit, horizontal unit (all 1/dpi inch),
    //   base unit = dpi.
    {
        const uint8_t p_unit = static_cast<uint8_t>(3600 / dpi);     // 1/dpi
        const uint16_t base  = static_cast<uint16_t>(dpi);
        out.push_back(ESC); out.push_back('('); out.push_back('U');
        out.push_back(0x05); out.push_back(0x00);
        out.push_back(p_unit);                                       // page unit
        out.push_back(p_unit);                                       // vertical unit
        out.push_back(p_unit);                                       // horizontal unit
        out.push_back(static_cast<uint8_t>(base & 0xFF));
        out.push_back(static_cast<uint8_t>((base >> 8) & 0xFF));
    }

    // ESC ( C 4 0 <h_lo .. h_hi> : page length in vertical units.
    {
        const uint32_t h = static_cast<uint32_t>(s.height_px);
        out.push_back(ESC); out.push_back('('); out.push_back('C');
        out.push_back(0x04); out.push_back(0x00);
        out.push_back(static_cast<uint8_t>(h & 0xFF));
        out.push_back(static_cast<uint8_t>((h >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((h >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((h >> 24) & 0xFF));
    }

    // ESC ( c 8 0 <top_lo..> <bottom_lo..> : margins (top=0, bottom=height).
    {
        const uint32_t top = 0;
        const uint32_t bot = static_cast<uint32_t>(s.height_px);
        out.push_back(ESC); out.push_back('('); out.push_back('c');
        out.push_back(0x08); out.push_back(0x00);
        for (int i = 0; i < 4; ++i)
            out.push_back(static_cast<uint8_t>((top >> (8 * i)) & 0xFF));
        for (int i = 0; i < 4; ++i)
            out.push_back(static_cast<uint8_t>((bot >> (8 * i)) & 0xFF));
    }

    // ESC U <0|1> : set print direction (0=bidir, 1=unidir).
    out.push_back(ESC); out.push_back('U');
    out.push_back(s.bidirectional ? 0x00 : 0x01);

    // For each channel that has a non-empty plane, emit ESC ( $ to pick the
    // color, then a single ESC . 1 raster block carrying all rows.
    for (int c = 0; c < CH_COUNT; ++c) {
        const auto& plane = s.channel_planes[c];
        if (plane.empty()) continue;
        if (static_cast<int>(plane.size()) != stride * s.height_px) {
            throw std::invalid_argument(
                "build_escpr_block: channel plane size mismatch");
        }

        // ESC ( $ 2 0 <color_code> <reserved=0>
        append_remote(out, '$',
                      {channel_color_code(static_cast<Channel>(c)), 0x00});

        // ESC . 1 <vert_dpi/3600 unit> <horz_dpi/3600 unit> <rows> <bytes_lo> <bytes_hi> data
        // Compression mode 1 = no compression (kept simple for B6 acceptance).
        const uint8_t v_unit = static_cast<uint8_t>(3600 / dpi);
        const uint8_t h_unit = static_cast<uint8_t>(3600 / dpi);
        if (s.height_px > 0xFFFF) {
            throw std::invalid_argument(
                "build_escpr_block: height exceeds single-block limit (split required)");
        }
        out.push_back(ESC); out.push_back('.');
        out.push_back(0x01);                                  // compression off
        out.push_back(v_unit);
        out.push_back(h_unit);
        out.push_back(static_cast<uint8_t>(s.height_px));     // rows in block
        out.push_back(static_cast<uint8_t>(stride & 0xFF));
        out.push_back(static_cast<uint8_t>((stride >> 8) & 0xFF));
        out.insert(out.end(), plane.begin(), plane.end());
    }

    // Form-feed → eject the page.
    out.push_back(FF);

    // ESC @ : reset.
    append_bytes(out, {ESC, '@'});

    return out;
}

std::vector<uint8_t> build_job(const JobSpec& s) {
    std::vector<uint8_t> out = build_pjl_header(s);
    auto body = build_escpr_block(s);
    out.insert(out.end(), body.begin(), body.end());
    auto foot = build_pjl_footer();
    out.insert(out.end(), foot.begin(), foot.end());
    return out;
}

bool inhouse_escp2_enabled() {
#ifdef JCOLOUR_ENABLE_INHOUSE_ESCP2
    return true;
#else
    return false;
#endif
}

std::vector<uint8_t> build_hardware_job(const JobSpec& s) {
    if (!inhouse_escp2_enabled()) {
        throw std::logic_error(
            "build_hardware_job: the in-house ESC/P2 emitter is gated off "
            "(JCOLOUR_ENABLE_INHOUSE_ESCP2 undefined). Production printing must "
            "go through jcolour-stp-backend (Gutenprint). This Stage-3 emitter "
            "stays disabled until P9580 weave/drop/language params are pinned "
            "from real-machine .prn capture.");
    }
    return build_job(s);
}

JobSpec make_solid_block(Channel channel, double width_mm, double height_mm, int dpi) {
    JobSpec s;
    s.dpi       = dpi;
    s.width_px  = mm_to_px(width_mm, dpi);
    s.height_px = mm_to_px(height_mm, dpi);

    const int stride = packed_stride(s.width_px);
    std::vector<uint8_t> plane(stride * s.height_px, 0xFF);

    // Mask off any padding bits past width_px in the last byte of each row,
    // so the printer never receives bits beyond the requested column count.
    const int extra_bits = stride * 8 - s.width_px;
    if (extra_bits > 0) {
        const uint8_t last_mask = static_cast<uint8_t>(0xFF << extra_bits);
        for (int y = 0; y < s.height_px; ++y) {
            plane[y * stride + (stride - 1)] &= last_mask;
        }
    }

    s.channel_planes[channel] = std::move(plane);
    return s;
}

}  // namespace epson
}  // namespace jcolour
