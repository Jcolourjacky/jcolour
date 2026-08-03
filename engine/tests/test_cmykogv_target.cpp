// Spike test: prove jcolour can produce a printable CMYKOGV target.
//   generate → render (12-ch JobSpec) → epson::build_job() bytes
//   + write .prn job, patch-list sidecar, and a PPM preview to /tmp.

#include "cmykogv_target.h"
#include "job.h"               // epson::build_job
#include "channel_layout.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cstdio>
#include <fstream>
#include <vector>

using namespace jcolour;
using namespace jcolour::calibration;
using Catch::Approx;

TEST_CASE("CMYKOGV target generates the saturated colorant set", "[cmykogv]") {
    auto patches = generate_cmykogv_patches();
    REQUIRE(patches.size() > 150);
    REQUIRE(patches.front().label == "W");           // first patch is paper white

    // Each colorant must appear at full strength somewhere (gamut corners).
    bool seen[7] = {};
    for (auto& p : patches)
        for (int c = 0; c < 7; ++c)
            if (p.cmykogv[c] >= 0.999f) seen[c] = true;
    for (int c = 0; c < 7; ++c) REQUIRE(seen[c]);     // incl. Or/Gr/V at 100%
}

TEST_CASE("patch_to_physical drives OGV and leaves lights at zero", "[cmykogv]") {
    TargetPatch p;
    p.cmykogv = {0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};   // 50% C + 100% Or
    auto phys = patch_to_physical(p, BlackKind::PK);

    REQUIRE(phys[epson::CH_C]  == Approx(0.5f));
    REQUIRE(phys[epson::CH_OR] == Approx(1.0f));
    // light inks are dilutions — must stay 0 on the profiling target
    REQUIRE(phys[epson::CH_LC]  == 0.0f);
    REQUIRE(phys[epson::CH_LM]  == 0.0f);
    REQUIRE(phys[epson::CH_GY]  == 0.0f);
    REQUIRE(phys[epson::CH_LGY] == 0.0f);
    // PK active, MK silent (black exclusivity)
    REQUIRE(phys[epson::CH_MK] == 0.0f);
}

TEST_CASE("render → build_job yields a printable PJL/ESC-PR job", "[cmykogv]") {
    auto patches = generate_cmykogv_patches();
    TargetChart chart;
    epson::JobSpec job = render_target_job(patches, BlackKind::PK,
                                           /*dpi*/360, /*patch*/8.0, /*gap*/1.5,
                                           /*margin*/10.0, 210.0, 297.0, 0, &chart);

    REQUIRE(chart.placed > 0);
    REQUIRE(job.width_px > 0);
    REQUIRE(job.height_px > 0);

    // OGV channels must carry ink; light channels must be empty (off).
    REQUIRE_FALSE(job.channel_planes[epson::CH_OR].empty());
    REQUIRE_FALSE(job.channel_planes[epson::CH_GR].empty());
    REQUIRE_FALSE(job.channel_planes[epson::CH_V].empty());
    REQUIRE(job.channel_planes[epson::CH_LC].empty());
    REQUIRE(job.channel_planes[epson::CH_LGY].empty());
    REQUIRE(job.channel_planes[epson::CH_MK].empty());     // PK job

    // The printable artifact: full PJL + ESC/P-R byte stream.
    std::vector<uint8_t> bytes = epson::build_job(job);
    REQUIRE(bytes.size() > 1000);

    // Emit artifacts for manual inspection / sending to the printer.
    std::ofstream("/tmp/cmykogv_target.prn", std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    write_patch_list(chart, "/tmp/cmykogv_target.patches.tsv");

    // Crude sRGB preview (PPM): composite the active channels subtractively so
    // a human can eyeball the chart. NOT colour-managed — preview only.
    const int W = job.width_px, H = job.height_px, stride = (W + 7) / 8;
    auto bit = [&](int ch, int x, int y) -> int {
        const auto& pl = job.channel_planes[ch];
        if (pl.empty()) return 0;
        return (pl[y * stride + (x >> 3)] >> (7 - (x & 7))) & 1;
    };
    // approximate ink → sRGB absorption (R,G,B subtracted per dot)
    struct Ab { int ch; float r, g, b; };
    const Ab inks[] = {
        {epson::CH_C,  0.7f,0.1f,0.0f}, {epson::CH_M,  0.0f,0.7f,0.2f},
        {epson::CH_Y,  0.0f,0.0f,0.8f}, {epson::CH_PK, 0.9f,0.9f,0.9f},
        {epson::CH_OR, 0.0f,0.5f,0.8f}, {epson::CH_GR, 0.7f,0.0f,0.5f},
        {epson::CH_V,  0.4f,0.6f,0.0f}};
    // downsample to keep the preview small
    const int DS = 6, pw = W / DS, ph = H / DS;
    std::ofstream ppm("/tmp/cmykogv_target.ppm", std::ios::binary);
    ppm << "P6\n" << pw << " " << ph << "\n255\n";
    std::vector<unsigned char> row(pw * 3);
    for (int y = 0; y < ph; ++y) {
        for (int x = 0; x < pw; ++x) {
            float r = 1, g = 1, b = 1;
            for (auto& ik : inks)
                if (bit(ik.ch, x * DS, y * DS)) { r -= ik.r; g -= ik.g; b -= ik.b; }
            auto cl = [](float v) { return (unsigned char)(v < 0 ? 0 : v > 1 ? 255 : v * 255); };
            row[x * 3 + 0] = cl(r); row[x * 3 + 1] = cl(g); row[x * 3 + 2] = cl(b);
        }
        ppm.write(reinterpret_cast<char*>(row.data()), row.size());
    }

    std::printf("\n[cmykogv] placed=%d  grid=%dx%d  pages=%d  page=%dx%dpx  job=%zu bytes\n",
                chart.placed, chart.cols, chart.rows, chart.pages,
                job.width_px, job.height_px, bytes.size());
    std::printf("[cmykogv] wrote /tmp/cmykogv_target.prn  .patches.tsv  .ppm\n");
}
