#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

#include "epson.h"
#include "job.h"

using namespace jcolour::epson;

namespace {

bool contains(const std::vector<uint8_t>& hay, const std::string& needle) {
    if (needle.empty() || hay.size() < needle.size()) return false;
    return std::search(hay.begin(), hay.end(),
                       needle.begin(), needle.end()) != hay.end();
}

bool find_seq(const std::vector<uint8_t>& hay,
              std::initializer_list<uint8_t> needle) {
    return std::search(hay.begin(), hay.end(),
                       needle.begin(), needle.end()) != hay.end();
}

}  // namespace

TEST_CASE("packed_stride: byte-aligned and non-aligned widths", "[job]") {
    REQUIRE(packed_stride(8)   == 1);
    REQUIRE(packed_stride(9)   == 2);
    REQUIRE(packed_stride(567) == 71);   // 1 cm @ 1440 dpi
    REQUIRE(packed_stride(1)   == 1);
}

TEST_CASE("mm_to_px: 1cm @ 1440dpi rounds to 567 px", "[job]") {
    REQUIRE(mm_to_px(10.0, 1440) == 567);
    REQUIRE(mm_to_px(25.4, 720)  == 720);  // exactly 1 inch
}

TEST_CASE("build_pjl_header: starts with UEL and declares ESC/P-R", "[job]") {
    JobSpec s;
    s.width_px = 567; s.height_px = 567; s.dpi = 1440;
    s.media_type = "PHOTORAG";
    s.paper_size = "A4";

    auto h = build_pjl_header(s);

    // Must begin with the Universal Exit Language sequence ESC%-12345X.
    REQUIRE(h.size() >= 9);
    REQUIRE(h[0] == 0x1B);
    REQUIRE(std::string(h.begin() + 1, h.begin() + 9) == "%-12345X");

    REQUIRE(contains(h, "@PJL ENTER LANGUAGE=ESCPR"));
    REQUIRE(contains(h, "@PJL SET RESOLUTION=1440"));
    REQUIRE(contains(h, "@PJL SET PAPERSIZE=A4"));
    REQUIRE(contains(h, "@PJL SET MEDIATYPE=PHOTORAG"));
    REQUIRE(contains(h, "@PJL SET DIRECTION=UNIDIR"));
    REQUIRE(contains(h, "@PJL SET BLACKOVERCOAT=OFF"));
}

TEST_CASE("build_pjl_header: bidirectional + BEO flags toggle correctly", "[job]") {
    JobSpec s;
    s.width_px = 100; s.height_px = 100;
    s.bidirectional  = true;
    s.black_overcoat = true;

    auto h = build_pjl_header(s);
    REQUIRE(contains(h, "@PJL SET DIRECTION=BIDIR"));
    REQUIRE(contains(h, "@PJL SET BLACKOVERCOAT=ON"));
}

TEST_CASE("build_pjl_footer: contains EOJ + UEL", "[job]") {
    auto f = build_pjl_footer();
    REQUIRE(contains(f, "@PJL EOJ"));
    REQUIRE(f.front() == 0x1B);   // leading UEL
    REQUIRE(f.back()  == 'X');    // trailing UEL ends with X
}

TEST_CASE("build_escpr_block: empty job throws", "[job]") {
    JobSpec s;
    REQUIRE_THROWS_AS(build_escpr_block(s), std::invalid_argument);
}

TEST_CASE("build_escpr_block: malformed plane throws", "[job]") {
    JobSpec s;
    s.width_px = 16; s.height_px = 4;
    s.channel_planes[CH_PK] = std::vector<uint8_t>(7, 0xFF);  // wrong size
    REQUIRE_THROWS_AS(build_escpr_block(s), std::invalid_argument);
}

TEST_CASE("build_escpr_block: ESC@ + ESC(G + form-feed framing", "[job]") {
    auto s = make_solid_block(CH_PK, /*mm*/ 10.0, /*mm*/ 10.0, /*dpi*/ 1440);
    auto body = build_escpr_block(s);

    // Leading ESC@ initialise.
    REQUIRE(body.size() >= 4);
    REQUIRE(body[0] == 0x1B);
    REQUIRE(body[1] == '@');

    // Enters graphics mode: ESC ( G 1 0 1
    REQUIRE(find_seq(body, {0x1B, '(', 'G', 0x01, 0x00, 0x01}));

    // Form-feed precedes trailing ESC@ reset.
    REQUIRE(body[body.size() - 3] == 0x0C);
    REQUIRE(body[body.size() - 2] == 0x1B);
    REQUIRE(body[body.size() - 1] == '@');
}

TEST_CASE("make_solid_block PK: 1cm x 1cm @ 1440 dpi has correct plane size",
          "[job][acceptance]") {
    auto s = make_solid_block(CH_PK, 10.0, 10.0, 1440);

    REQUIRE(s.width_px  == 567);
    REQUIRE(s.height_px == 567);
    REQUIRE(s.dpi       == 1440);

    const int stride = packed_stride(s.width_px);
    REQUIRE(stride == 71);

    // Only the PK plane is populated.
    REQUIRE(s.channel_planes[CH_PK].size() == static_cast<size_t>(stride * 567));
    for (int c = 0; c < CH_COUNT; ++c) {
        if (c == CH_PK) continue;
        REQUIRE(s.channel_planes[c].empty());
    }

    // Inner bytes are fully solid.
    for (int y = 0; y < s.height_px; ++y) {
        const auto* row = s.channel_planes[CH_PK].data() + y * stride;
        for (int b = 0; b < stride - 1; ++b) {
            REQUIRE(row[b] == 0xFF);
        }
        // Last byte: 567 % 8 = 7 valid bits → mask 0xFE.
        REQUIRE(row[stride - 1] == 0xFE);
    }
}

TEST_CASE("build_job: 1cm PK block — full job structure (B6 acceptance)",
          "[job][acceptance]") {
    auto s    = make_solid_block(CH_PK, 10.0, 10.0, 1440);
    auto bytes = build_job(s);

    // Starts with PJL UEL.
    REQUIRE(bytes.size() > 1024);
    REQUIRE(std::string(bytes.begin(), bytes.begin() + 9) ==
            std::string("\x1B" "%-12345X"));

    // Per-channel raster block selector ESC ( $ <code> for PK = 0x00.
    REQUIRE(find_seq(bytes, {0x1B, '(', '$', 0x02, 0x00, 0x00, 0x00}));

    // Carries a raster-data start ESC . 1 with stride = 71 bytes.
    // ESC . 1 v_unit h_unit rows lo hi ...; rows = 567 → lo=0x37 hi=0x02.
    REQUIRE(find_seq(bytes, {0x1B, '.', 0x01}));

    // Footer present.
    REQUIRE(contains(bytes, "@PJL EOJ"));

    // Sanity: payload size dominated by the single 71×567 = 40257-byte plane,
    // plus overhead ≪ 2 KB.
    const size_t plane = 71u * 567u;
    REQUIRE(bytes.size() >= plane);
    REQUIRE(bytes.size() <  plane + 2048u);
}

TEST_CASE("build_job: multi-channel job emits one selector per non-empty plane",
          "[job]") {
    JobSpec s;
    s.width_px = 16; s.height_px = 2; s.dpi = 720;
    const int stride = packed_stride(s.width_px);
    s.channel_planes[CH_PK] = std::vector<uint8_t>(stride * s.height_px, 0xAA);
    s.channel_planes[CH_C ] = std::vector<uint8_t>(stride * s.height_px, 0x55);
    s.channel_planes[CH_Y ] = std::vector<uint8_t>(stride * s.height_px, 0xF0);

    auto body = build_escpr_block(s);

    // Color-select envelopes: ESC ( $ <2><0> <code> <0> for codes 0x00, 0x02, 0x04.
    REQUIRE(find_seq(body, {0x1B, '(', '$', 0x02, 0x00, 0x00, 0x00}));
    REQUIRE(find_seq(body, {0x1B, '(', '$', 0x02, 0x00, 0x02, 0x00}));
    REQUIRE(find_seq(body, {0x1B, '(', '$', 0x02, 0x00, 0x04, 0x00}));
    // Channels not populated must not appear.
    REQUIRE_FALSE(find_seq(body, {0x1B, '(', '$', 0x02, 0x00, 0x03, 0x00}));  // M
    REQUIRE_FALSE(find_seq(body, {0x1B, '(', '$', 0x02, 0x00, 0x0A, 0x00}));  // GR
}

TEST_CASE("build_job → send_raw_data round-trip on unreachable host fails fast",
          "[job][epson]") {
    auto bytes = build_job(make_solid_block(CH_PK, 5.0, 5.0, 720));
    Printer p("127.0.0.1", 1);
    REQUIRE(p.send_raw_data(bytes) == false);
    REQUIRE(p.is_connected() == false);
}
