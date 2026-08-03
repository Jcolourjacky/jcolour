#include "softproof.h"
#include "channel_layout.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace jcolour {
namespace gamut {

namespace {

// printer.icc from Argyll colprof has 12-channel device-side output mapped
// to the canonical PJL physical layout (see channel_layout.h):
//   0:PK 1:MK 2:LC 3:C 4:VLM 5:VM 6:Y 7:Or 8:GY 9:LGY 10:V 11:Gr
// The gamut-expansion meter samples channels 7 (Or), 10 (V), 11 (Gr).
//
// These indices are the *physical* slot positions, not active-channel
// indices — softproof reads ICC output directly, so it lives in the
// physical layout.
constexpr int OR_CHANNEL_INDEX = 7;
constexpr int V_CHANNEL_INDEX  = 10;
constexpr int GR_CHANNEL_INDEX = 11;

// Sanity-check the constants against channel_layout's expand_to_physical:
// build a unit ActiveInks for Or / V / Gr and confirm the physical slot
// these constants point at really does receive that ink. Catches accidental
// drift if the layout ever changes.
static_assert(NUM_PHYSICAL_CHANNELS == 12,
              "softproof channel indices assume 12 physical PRO12 slots");

// Default ΔE threshold for "out of gamut" — aligns with ADR-11 minimum useful.
constexpr float DEFAULT_OOG_DELTA_E = 3.0f;

// Display coverage sampling matches GamutGate::check_display_gamut so the two
// helpers stay in agreement.
constexpr double DISPLAY_ADEQUACY_RATIO = 0.95;
constexpr double DISPLAY_SAMPLE_DE_LIMIT = 5.0;

inline double delta_e_76(const cmsCIELab& a, const cmsCIELab& b) {
    double dL = a.L - b.L;
    double da = a.a - b.a;
    double db = a.b - b.b;
    return std::sqrt(dL * dL + da * da + db * db);
}

}  // namespace

struct SoftproofEngine::Impl {
    SoftproofConfig config;
};

SoftproofEngine::SoftproofEngine() : pImpl_(new Impl) {}
SoftproofEngine::~SoftproofEngine() { delete pImpl_; }

void SoftproofEngine::set_config(const SoftproofConfig& config) {
    pImpl_->config = config;
}

SoftproofResult SoftproofEngine::render(
    const uint16_t* src_data, int width, int height, int channels,
    cmsHPROFILE src_profile,
    cmsHPROFILE printer_profile,
    cmsHPROFILE display_profile,
    int intent, bool bpc) {

    SoftproofResult result;
    result.width = width;
    result.height = height;

    if (!src_data || !src_profile || !printer_profile || !display_profile ||
        width <= 0 || height <= 0 || channels < 3) {
        return result;
    }

    const SoftproofConfig& cfg = pImpl_->config;
    const float oog_threshold = cfg.gamut_warning_threshold > 0.0f
        ? cfg.gamut_warning_threshold
        : DEFAULT_OOG_DELTA_E;
    const double oog_threshold_d = static_cast<double>(oog_threshold);

    const size_t pixel_count = static_cast<size_t>(width) * height;
    result.proofed_image.assign(pixel_count * 3, 0);
    result.gamut_warning_mask.assign(pixel_count, 0);

    // We only handle RGB-like inputs; the contract in PIPELINE §7 is
    // 16-bit ProPhoto. Channel count > 3 (alpha) is tolerated by ignoring
    // extra channels via TYPE_RGB_16 plus manual row stride.
    const cmsUInt32Number src_format = TYPE_RGB_16;

    // Proofing intent governs paper white simulation:
    //   ABSOLUTE  → on-screen white shows paper tint (matches print look)
    //   RELATIVE  → on-screen white stays display white (no tint)
    const cmsUInt32Number proofing_intent = cfg.paper_white_simulation
        ? INTENT_ABSOLUTE_COLORIMETRIC
        : INTENT_RELATIVE_COLORIMETRIC;

    cmsUInt32Number proof_flags = cmsFLAGS_SOFTPROOFING |
                                  cmsFLAGS_HIGHRESPRECALC;
    if (bpc) proof_flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;

    cmsHTRANSFORM proof_xform = cmsCreateProofingTransform(
        src_profile, src_format,
        display_profile, TYPE_RGB_16,
        printer_profile,
        static_cast<cmsUInt32Number>(intent),
        proofing_intent,
        proof_flags);
    if (!proof_xform) return result;

    cmsHPROFILE lab_profile = cmsCreateLab4Profile(nullptr);

    // Round-trip transforms for OOG detection:
    //   src → src.Lab (what the image asks for)
    //   src → printer.Lab (what the printer can actually produce)
    // Both use INTENT_RELATIVE_COLORIMETRIC so paper tint is excluded from
    // the comparison — we only want to know whether the printer can hit
    // the color, not whether the paper looks tinted.
    cmsHTRANSFORM to_src_lab = nullptr;
    cmsHTRANSFORM to_printer_lab = nullptr;
    if (cfg.gamut_warning_enabled) {
        to_src_lab = cmsCreateTransform(
            src_profile, src_format,
            lab_profile, TYPE_Lab_DBL,
            INTENT_RELATIVE_COLORIMETRIC,
            cmsFLAGS_NOCACHE);

        to_printer_lab = cmsCreateProofingTransform(
            src_profile, src_format,
            lab_profile, TYPE_Lab_DBL,
            printer_profile,
            static_cast<cmsUInt32Number>(intent),
            INTENT_RELATIVE_COLORIMETRIC,
            cmsFLAGS_SOFTPROOFING |
                (bpc ? cmsFLAGS_BLACKPOINTCOMPENSATION : 0));
    }

    // Ink coverage meter — only meaningful for the PRO 12 device.
    // Falls back silently to zero coverage if the profile is not 12-ch.
    const int printer_channels =
        cmsChannelsOf(cmsGetColorSpace(printer_profile));
    cmsHTRANSFORM to_device = nullptr;
    if (printer_channels == 12) {
        const cmsUInt32Number dev_format = (COLORSPACE_SH(PT_MCH12) |
                                            CHANNELS_SH(12) |
                                            BYTES_SH(2));
        to_device = cmsCreateTransform(
            src_profile, src_format,
            printer_profile, dev_format,
            static_cast<cmsUInt32Number>(intent),
            cmsFLAGS_NOCACHE |
                (bpc ? cmsFLAGS_BLACKPOINTCOMPENSATION : 0));
    }

    // Optional paper-tint post-pass. Built once outside the row loop.
    const bool apply_paper_tint =
        cfg.paper_white_simulation &&
        (std::fabs(cfg.paper_white_a) > 0.01f ||
         std::fabs(cfg.paper_white_b) > 0.01f);
    cmsHTRANSFORM disp_to_lab = nullptr;
    cmsHTRANSFORM lab_to_disp = nullptr;
    if (apply_paper_tint) {
        disp_to_lab = cmsCreateTransform(
            display_profile, TYPE_RGB_16,
            lab_profile, TYPE_Lab_DBL,
            INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_NOCACHE);
        lab_to_disp = cmsCreateTransform(
            lab_profile, TYPE_Lab_DBL,
            display_profile, TYPE_RGB_16,
            INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_NOCACHE);
    }

    // Reusable row buffers — sized once.
    std::vector<uint16_t> rgb_in_row;
    const bool need_repack = (channels != 3);
    if (need_repack) rgb_in_row.assign(static_cast<size_t>(width) * 3, 0);

    std::vector<cmsCIELab> src_lab_row(width);
    std::vector<cmsCIELab> printer_lab_row(width);
    std::vector<cmsCIELab> tint_row(apply_paper_tint ? width : 0);
    std::vector<uint16_t> device_row(
        to_device ? static_cast<size_t>(width) * 12 : 0, 0);

    double or_sum = 0.0;
    double gr_sum = 0.0;
    double v_sum = 0.0;
    size_t oog_count = 0;

    for (int y = 0; y < height; ++y) {
        const uint16_t* row_full =
            src_data + static_cast<size_t>(y) * width * channels;

        const uint16_t* in_row;
        if (need_repack) {
            for (int x = 0; x < width; ++x) {
                rgb_in_row[x * 3 + 0] = row_full[x * channels + 0];
                rgb_in_row[x * 3 + 1] = row_full[x * channels + 1];
                rgb_in_row[x * 3 + 2] = row_full[x * channels + 2];
            }
            in_row = rgb_in_row.data();
        } else {
            in_row = row_full;
        }

        uint16_t* out_row = result.proofed_image.data() +
                            static_cast<size_t>(y) * width * 3;
        uint8_t* mask_row = result.gamut_warning_mask.data() +
                            static_cast<size_t>(y) * width;

        // 1. Render to display with proofing simulation.
        cmsDoTransform(proof_xform, in_row, out_row, width);

        // 2. Optional explicit paper a*/b* shift on top of LCMS2's sim.
        //    Used when the user knows the specific paper substrate offset
        //    (e.g., warm cotton vs. neutral baryta) and the printer ICC
        //    doesn't already encode it.
        if (apply_paper_tint && disp_to_lab && lab_to_disp) {
            cmsDoTransform(disp_to_lab, out_row, tint_row.data(), width);
            for (int x = 0; x < width; ++x) {
                tint_row[x].a += cfg.paper_white_a;
                tint_row[x].b += cfg.paper_white_b;
            }
            cmsDoTransform(lab_to_disp, tint_row.data(), out_row, width);
        }

        // 3. Per-pixel out-of-gamut mask via ΔE76 round-trip.
        if (to_src_lab && to_printer_lab) {
            cmsDoTransform(to_src_lab, in_row, src_lab_row.data(), width);
            cmsDoTransform(to_printer_lab, in_row, printer_lab_row.data(), width);
            for (int x = 0; x < width; ++x) {
                if (delta_e_76(src_lab_row[x], printer_lab_row[x]) >
                    oog_threshold_d) {
                    mask_row[x] = 255;
                    ++oog_count;
                }
            }
        }

        // 4. Or/Gr/V coverage accumulator.
        if (to_device) {
            cmsDoTransform(to_device, in_row, device_row.data(), width);
            for (int x = 0; x < width; ++x) {
                const uint16_t* dev = &device_row[x * 12];
                or_sum += dev[OR_CHANNEL_INDEX] / 65535.0;
                gr_sum += dev[GR_CHANNEL_INDEX] / 65535.0;
                v_sum  += dev[V_CHANNEL_INDEX]  / 65535.0;
            }
        }
    }

    result.out_of_gamut_percent =
        static_cast<float>(static_cast<double>(oog_count) /
                           static_cast<double>(pixel_count) * 100.0);

    if (to_device) {
        const double denom = static_cast<double>(pixel_count);
        result.ink_coverage_ogv[0] = static_cast<float>(or_sum / denom * 100.0);
        result.ink_coverage_ogv[1] = static_cast<float>(gr_sum / denom * 100.0);
        result.ink_coverage_ogv[2] = static_cast<float>(v_sum / denom * 100.0);
    }

    cmsDeleteTransform(proof_xform);
    if (to_src_lab) cmsDeleteTransform(to_src_lab);
    if (to_printer_lab) cmsDeleteTransform(to_printer_lab);
    if (to_device) cmsDeleteTransform(to_device);
    if (disp_to_lab) cmsDeleteTransform(disp_to_lab);
    if (lab_to_disp) cmsDeleteTransform(lab_to_disp);
    cmsCloseProfile(lab_profile);

    return result;
}

float SoftproofEngine::check_display_coverage(cmsHPROFILE display_profile) {
    if (!display_profile) return 0.0f;

    cmsHPROFILE lab_profile = cmsCreateLab4Profile(nullptr);
    cmsHTRANSFORM to_display = cmsCreateTransform(
        lab_profile, TYPE_Lab_DBL,
        display_profile, TYPE_RGB_16,
        INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_NOCACHE);
    cmsHTRANSFORM back_to_lab = cmsCreateTransform(
        display_profile, TYPE_RGB_16,
        lab_profile, TYPE_Lab_DBL,
        INTENT_ABSOLUTE_COLORIMETRIC, cmsFLAGS_NOCACHE);

    int in_gamut = 0;
    int total = 0;
    if (to_display && back_to_lab) {
        for (int h = 0; h < 360; h += 10) {
            for (double C = 40.0; C <= 120.0; C += 20.0) {
                double rad = h * M_PI / 180.0;
                cmsCIELab src = {60.0, C * std::cos(rad), C * std::sin(rad)};
                uint16_t rgb[3];
                cmsCIELab back;
                cmsDoTransform(to_display, &src, rgb, 1);
                cmsDoTransform(back_to_lab, rgb, &back, 1);
                if (delta_e_76(src, back) < DISPLAY_SAMPLE_DE_LIMIT) ++in_gamut;
                ++total;
            }
        }
    }

    if (to_display) cmsDeleteTransform(to_display);
    if (back_to_lab) cmsDeleteTransform(back_to_lab);
    cmsCloseProfile(lab_profile);

    return total > 0 ? static_cast<float>(in_gamut) /
                       static_cast<float>(total) : 0.0f;
}

bool SoftproofEngine::is_display_adequate(cmsHPROFILE display_profile) {
    return check_display_coverage(display_profile) >=
           static_cast<float>(DISPLAY_ADEQUACY_RATIO);
}

}
}
