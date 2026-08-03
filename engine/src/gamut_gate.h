#pragma once

#include <lcms2.h>
#include <string>
#include <cstdint>

namespace jcolour {
namespace gamut {

enum class GateResult { Pass, Warn, Reject };
enum class WarnLevel { None, Yellow, Amber, Red };

struct GateStatus {
    GateResult result = GateResult::Pass;
    WarnLevel warn_level = WarnLevel::None;
    std::string message;
};

struct GateReport {
    GateStatus g1_source_profile;
    GateStatus g2_bit_depth;
    GateStatus g3_alpha_clip;
    GateStatus g4_display_gamut;
    GateStatus g5_dest_icc;

    bool all_passed() const {
        return g1_source_profile.result != GateResult::Reject &&
               g2_bit_depth.result != GateResult::Reject &&
               g3_alpha_clip.result != GateResult::Reject &&
               g4_display_gamut.result != GateResult::Reject &&
               g5_dest_icc.result != GateResult::Reject;
    }
};

class GamutGate {
public:
    // G1: source profile must be ProPhoto RGB
    static GateStatus check_source_profile(cmsHPROFILE profile);

    // G2: bit depth must be 16
    static GateStatus check_bit_depth(int bits_per_sample);

    // G3: no alpha premultiplication, no hard clip
    static GateStatus check_alpha_clip(const uint16_t* data, int width, int height,
                                       int channels, int extra_samples_type);

    // G4: display gamut >= Display P3 (softproofing only)
    static GateStatus check_display_gamut(cmsHPROFILE display_profile);

    // G5: destination ICC completeness
    static GateStatus check_dest_icc(cmsHPROFILE dest_profile);

    // Run all gates
    static GateReport run_all(cmsHPROFILE src_profile, int bits_per_sample,
                              const uint16_t* data, int width, int height,
                              int channels, int extra_samples_type,
                              cmsHPROFILE display_profile,
                              cmsHPROFILE dest_profile);

    // Profile detection helpers
    static bool matches_prophoto_primaries(cmsHPROFILE profile, double epsilon = 0.001);
    static bool matches_adobergb_primaries(cmsHPROFILE profile, double epsilon = 0.001);
    static bool matches_srgb_primaries(cmsHPROFILE profile, double epsilon = 0.001);
};

}
}
