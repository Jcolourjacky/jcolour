// (2) i1 measure→match pipeline, hardware-free: generate CMYKOGV target →
// write device sidecar → fabricate a CGATS .ti3 via KM forward (synthetic
// inks) → parse + join + gamut-summarise. Proves the ingestion the real
// i1Pro3 readings will plug into.

#include "cmykogv_target.h"
#include "target_measure.h"
#include "km.h"
#include "spectral.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <fstream>
#include <vector>

using namespace jcolour;
using namespace jcolour::calibration;
using jcolour::forward::KMForwardModel;

namespace {
spectral::Spectrum band(double c, double w, double depth = 0.92) {
    spectral::Spectrum s;
    for (int i = 0; i < spectral::NUM_CHANNELS; ++i) {
        double l = spectral::WAVELENGTHS[i];
        s[i] = std::max(0.02, 0.96 - depth * std::exp(-0.5 * std::pow((l - c) / w, 2.0)));
    }
    return s;
}
spectral::Spectrum band2(double c1,double w1,double c2,double w2){
    spectral::Spectrum s;
    for(int i=0;i<spectral::NUM_CHANNELS;++i){double l=spectral::WAVELENGTHS[i];
        double a=0.92*std::exp(-0.5*std::pow((l-c1)/w1,2.))+0.92*std::exp(-0.5*std::pow((l-c2)/w2,2.));
        s[i]=std::max(0.02,0.96-std::min(0.93,a));}
    return s;
}
spectral::Spectrum flat(double r){ spectral::Spectrum s; s.fill(r); return s; }

// KM model over the 7 CMYKOGV colorants (synthetic, OGV purer than CMY).
KMForwardModel cmykogv_model() {
    KMForwardModel m(flat(0.93));
    m.add_ink(band(620,70,0.82)); m.add_ink(band(545,65,0.82)); m.add_ink(band(445,50,0.85));
    m.add_ink(flat(0.03)); m.add_ink(band(500,30,0.96)); m.add_ink(band2(425,26,660,36)); m.add_ink(band(565,55,0.95));
    return m;
}
}

TEST_CASE("measure→match→gamut pipeline (synthetic .ti3)", "[measure]") {
    auto patches = generate_cmykogv_patches();
    TargetChart chart;
    render_target_job(patches, BlackKind::PK, 360, 8, 1.5, 10, 210, 297, 0, &chart);
    REQUIRE(chart.placed > 100);

    const std::string tsv = "/tmp/tm_patches.tsv";
    const std::string ti3 = "/tmp/tm_meas.ti3";
    REQUIRE(write_patch_list(chart, tsv));

    // fabricate the instrument reading: KM forward over each placed patch.
    auto model = cmykogv_model();
    std::ofstream f(ti3);
    f << "CGATS.17\nBEGIN_DATA_FORMAT\nSAMPLE_ID LAB_L LAB_A LAB_B\nEND_DATA_FORMAT\n";
    f << "NUMBER_OF_SETS " << chart.placed << "\nBEGIN_DATA\n";
    for (int i = 0; i < (int)chart.patches.size(); ++i) {
        const auto& p = chart.patches[i];
        std::vector<double> inks(p.cmykogv.begin(), p.cmykogv.end());
        auto R = model.predict(inks);
        auto lab = spectral::xyz_to_lab(spectral::spectrum_to_xyz(R));
        f << (i + 1) << ' ' << lab.L << ' ' << lab.a << ' ' << lab.b << '\n';
    }
    f << "END_DATA\n";
    f.close();

    auto joined = join_measurement(tsv, ti3);
    REQUIRE(joined.size() == chart.patches.size());
    // device↔Lab alignment: white patch ≈ paper white (high L, low chroma)
    REQUIRE(joined.front().label == "W");
    REQUIRE(joined.front().L > 90.0);
    REQUIRE(joined.front().chroma() < 5.0);

    auto g = summarize_gamut(joined);
    std::printf("\n[measure] joined %zu patches\n", joined.size());
    std::printf("[measure] corner→nearest-CMY ΔEab:  Or %.1f (C*%.0f) · Gr %.1f (C*%.0f) · V %.1f (C*%.0f)\n",
                g.or_dist, g.or_corner_C, g.gr_dist, g.gr_corner_C, g.v_dist, g.v_corner_C);

    // each OGV corner sits clearly beyond the CMY-only point cloud (extension)
    REQUIRE(g.or_dist > 4.0);
    REQUIRE(g.gr_dist > 4.0);
    REQUIRE(g.v_dist  > 4.0);
}

TEST_CASE("join rejects mismatched counts", "[measure]") {
    std::ofstream("/tmp/tm_p.tsv") << "LABEL\tC\tM\tY\tK\tOr\tGr\tV\tCOL\tROW\nW\t0\t0\t0\t0\t0\t0\t0\t0\t0\n";
    std::ofstream("/tmp/tm_m.ti3") << "BEGIN_DATA_FORMAT\nSAMPLE_ID LAB_L LAB_A LAB_B\nEND_DATA_FORMAT\n"
                                       "BEGIN_DATA\n1 95 0 0\n2 50 1 1\nEND_DATA\n";
    REQUIRE(join_measurement("/tmp/tm_p.tsv", "/tmp/tm_m.ti3").empty());
}
