#pragma once

#include <lcms2.h>
#include <vector>
#include <array>
#include <string>
#include <cstdint>

namespace jcolour {
namespace gamut {

struct SoftproofConfig {
    bool paper_white_simulation = true;
    float paper_white_a = 0.0f;
    float paper_white_b = 0.0f;
    std::string paper_name;
    bool gamut_warning_enabled = true;
    float gamut_warning_threshold = 0.0f;
};

struct SoftproofResult {
    std::vector<uint16_t> proofed_image;
    std::vector<uint8_t> gamut_warning_mask;
    float out_of_gamut_percent = 0.0f;
    std::array<float, 3> ink_coverage_ogv = {0, 0, 0}; // Or, Gr, V
    int width = 0;
    int height = 0;
};

class SoftproofEngine {
public:
    SoftproofEngine();
    ~SoftproofEngine();

    void set_config(const SoftproofConfig& config);

    SoftproofResult render(
        const uint16_t* src_data, int width, int height, int channels,
        cmsHPROFILE src_profile,
        cmsHPROFILE printer_profile,
        cmsHPROFILE display_profile,
        int intent, bool bpc);

    static float check_display_coverage(cmsHPROFILE display_profile);
    static bool is_display_adequate(cmsHPROFILE display_profile);

private:
    struct Impl;
    Impl* pImpl_;
};

}
}
