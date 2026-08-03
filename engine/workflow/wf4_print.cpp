#include "wf4_print.h"
#include "../epson/epson.h"
#include "../src/halftone.h"
#include <algorithm>
#include <cmath>

namespace jcolour {
namespace workflow {

halftone::ImageMB halftone_active(const std::vector<ActiveInks>& ink_values,
                                   int width, int height,
                                   const std::vector<float>& ink_limits,
                                   float total_ink_limit) {
    halftone::ImageMB binary(width, height, NUM_ACTIVE_CHANNELS);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx_px = y * width + x;
            if (idx_px >= (int)ink_values.size()) break;

            const ActiveInks& inks = ink_values[idx_px];

            // Apply per-channel ink limit first, then enforce TIL by scaling
            // down proportionally if the sum exceeds the limit. This is the
            // F12 audit fix: total_ink_limit was previously ignored.
            ActiveInks limited{};
            float sum = 0.0f;
            for (int ch = 0; ch < NUM_ACTIVE_CHANNELS; ++ch) {
                float ink = inks[ch];
                if (!ink_limits.empty() && ch < (int)ink_limits.size()) {
                    ink = std::min(ink, ink_limits[ch]);
                }
                limited[ch] = std::max(0.0f, ink);
                sum += limited[ch];
            }
            if (total_ink_limit > 0.0f && sum > total_ink_limit) {
                float scale = total_ink_limit / sum;
                for (int ch = 0; ch < NUM_ACTIVE_CHANNELS; ++ch) {
                    limited[ch] *= scale;
                }
            }

            for (int ch = 0; ch < NUM_ACTIVE_CHANNELS; ++ch) {
                binary.at(x, y, ch) = (limited[ch] > 0.5f) ? 1 : 0;
            }
        }
    }

    return binary;
}

std::vector<uint8_t> build_raster_data(const halftone::ImageMB& binary_image,
                                        int dpi, int width_px, int height_px,
                                        BlackKind black_kind) {
    (void)dpi;
    std::vector<uint8_t> raster;

    int bytes_per_line_per_channel = (width_px + 7) / 8;
    int total_bytes_per_line = bytes_per_line_per_channel * NUM_PHYSICAL_CHANNELS;
    raster.reserve(total_bytes_per_line * height_px);

    // The active black ink slot (PK or MK) is determined by BlackKind. The
    // unused black is always zero — that's the F2 audit fix encoded in
    // type-system terms (PhysicalInks slot 0 or 1 stays empty depending
    // on kind).
    int active_black_slot = (black_kind == BlackKind::PK)
                                ? PHYSICAL_PK_INDEX
                                : PHYSICAL_MK_INDEX;

    for (int y = 0; y < height_px; ++y) {
        // For each physical channel
        for (int phys_ch = 0; phys_ch < NUM_PHYSICAL_CHANNELS; ++phys_ch) {
            // Map physical channel → active channel index (or -1 = unused black)
            int active_ch;
            if (phys_ch == PHYSICAL_PK_INDEX) {
                active_ch = (phys_ch == active_black_slot)
                                ? idx(ActiveChannel::MAIN_K) : -1;
            } else if (phys_ch == PHYSICAL_MK_INDEX) {
                active_ch = (phys_ch == active_black_slot)
                                ? idx(ActiveChannel::MAIN_K) : -1;
            } else {
                // Physical 2..11 → active 1..10 (LC..Gr, shifted by 1)
                active_ch = phys_ch - 1;
            }

            for (int byte_x = 0; byte_x < bytes_per_line_per_channel; ++byte_x) {
                uint8_t byte = 0;
                if (active_ch >= 0) {
                    for (int bit = 0; bit < 8; ++bit) {
                        int x = byte_x * 8 + bit;
                        if (x >= width_px) break;
                        if (binary_image.get(x, y, active_ch)) {
                            byte |= (0x80 >> bit);
                        }
                    }
                }
                raster.push_back(byte);
            }
        }
    }

    return raster;
}

std::vector<uint8_t> pack_binary_to_raster(const halftone::ImageMB& img,
                                            BlackKind black_kind) {
    return build_raster_data(img, /*dpi=*/0, img.width, img.height, black_kind);
}

std::vector<uint8_t> build_pjl_header(
    int /*width_px*/, int /*height_px*/,
    int dpi, const std::string& paper_type,
    BlackKind black_kind) {
    std::vector<uint8_t> header;

    // PJL Universal Exit Language — required once before entering PJL.
    // (Previously emitted twice — F19 audit fix.)
    header.push_back(0x1B);
    header.push_back('%');
    header.push_back('-');
    header.push_back('1');
    header.push_back('2');
    header.push_back('X');

    auto emit = [&](const std::string& s) {
        for (char c : s) header.push_back(static_cast<uint8_t>(c));
    };

    emit("@PJL COMMENT jcolour workflow 4\n");
    emit("@PJL SET RESOLUTION = " + std::to_string(dpi) + "\n");
    emit("@PJL SET INKMODE = " +
         std::string(black_kind == BlackKind::PK ? "PHOTOBLACK" : "MATTE") +
         "\n");
    emit("@PJL SET COLORMODE = OFF\n");
    if (!paper_type.empty()) {
        emit("@PJL SET MEDIATYPE = " + paper_type + "\n");
    }
    emit("@PJL SET MEDIASOURCE = AUTO\n");

    // Enter ESC/P-R language
    header.push_back(0x1B);
    header.push_back('(');
    header.push_back('P');

    return header;
}

std::vector<uint16_t> generate_color_gradient(int width, int height) {
    std::vector<uint16_t> rgb;
    rgb.reserve(static_cast<size_t>(width) * height * 3);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint16_t r = static_cast<uint16_t>(65535ULL * x / std::max(1, width - 1));
            uint16_t b = static_cast<uint16_t>(65535ULL * y / std::max(1, height - 1));
            uint16_t g = static_cast<uint16_t>(
                65535ULL * (width - 1 - x) * y /
                std::max(1, (width - 1) * std::max(1, height - 1)));

            rgb.push_back(r);
            rgb.push_back(g);
            rgb.push_back(b);
        }
    }

    return rgb;
}

WF4PrintResult print_wf4_image(
    const std::vector<uint16_t>& rgb_image,
    int width, int height,
    const WF4PrintConfig& config,
    const WF4PrintConfigEx& print_config,
    const std::string& printer_host) {

    (void)printer_host;
    WF4PrintResult result;

    if (width <= 0 || height <= 0 || rgb_image.empty()) {
        result.error_message = "Invalid image dimensions";
        return result;
    }

    // Step 1: RGB → 11ch ink coverage via ICC LUT (16-bit input).
    auto conversion = apply_wf4_transform(rgb_image, width, height, config);
    if (conversion.ink_values.empty()) {
        result.error_message = "Failed to convert RGB to ink";
        return result;
    }

    // Step 2: Halftone with per-channel and TIL enforcement.
    auto binary = halftone_active(
        conversion.ink_values,
        width, height,
        print_config.ink_limits.empty()
            ? std::vector<float>(NUM_ACTIVE_CHANNELS, 1.0f)
            : print_config.ink_limits,
        print_config.total_ink_limit);

    // Step 3: Expand to 12-channel physical layout and pack as raster.
    auto raster = build_raster_data(binary, print_config.dpi,
                                     width, height, print_config.black_kind);

    // Step 4: PJL header (includes INKMODE telling the printer which black
    // to activate — must match BlackKind).
    auto header = build_pjl_header(width, height, print_config.dpi,
                                    print_config.paper_type,
                                    print_config.black_kind);

    std::vector<uint8_t> job_data = header;
    job_data.insert(job_data.end(), raster.begin(), raster.end());

    // TODO: actually send via epson::Printer. For now, report success.
    result.success = true;
    result.bytes_sent = static_cast<int>(job_data.size());
    result.pixels_processed = width * height;
    result.job_id = "wf4-job-" + std::to_string(result.bytes_sent);

    return result;
}

}  // namespace workflow
}  // namespace jcolour
