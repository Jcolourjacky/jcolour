#include "halftone.h"
#include <algorithm>
#include <cmath>

namespace jcolour {
namespace halftone {

ImageB floyd_steinberg(const ImageF& input) {
    int w = input.width;
    int h = input.height;

    // Work on a mutable copy with error accumulation
    std::vector<double> buf(input.data.begin(), input.data.end());
    ImageB out(w, h);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double old_val = std::max(0.0, std::min(1.0, buf[y * w + x]));
            uint8_t new_val = (old_val >= 0.5) ? 1 : 0;
            out.at(x, y) = new_val;

            double err = old_val - static_cast<double>(new_val);

            // Distribute error: Floyd-Steinberg kernel
            //        [*]  7/16
            //  3/16  5/16  1/16
            if (x + 1 < w)
                buf[y * w + (x + 1)]       += err * 7.0 / 16.0;
            if (y + 1 < h) {
                if (x - 1 >= 0)
                    buf[(y + 1) * w + (x - 1)] += err * 3.0 / 16.0;
                buf[(y + 1) * w + x]           += err * 5.0 / 16.0;
                if (x + 1 < w)
                    buf[(y + 1) * w + (x + 1)] += err * 1.0 / 16.0;
            }
        }
    }
    return out;
}

double dot_density(const ImageB& img) {
    int total = img.width * img.height;
    if (total == 0) return 0.0;
    int ones = 0;
    for (uint8_t v : img.data) ones += v;
    return static_cast<double>(ones) / static_cast<double>(total);
}

// Vector error diffusion: N channels with shared error buffer.
// Each pixel is processed channel by channel. The threshold for each
// channel is 0.5. Error is propagated per-channel using the standard
// Floyd-Steinberg kernel. Channels share the same scan order so that
// dot placement is spatially correlated across channels.
ImageMB vector_error_diffusion(const ImageMF& input) {
    int w = input.width;
    int h = input.height;
    int c = input.channels;

    // Per-channel error buffers
    std::vector<std::vector<double>> buf(c, std::vector<double>(w * h));
    for (int ch = 0; ch < c; ++ch)
        for (int i = 0; i < w * h; ++i)
            buf[ch][i] = input.data[i * c + ch];

    ImageMB out(w, h, c);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            for (int ch = 0; ch < c; ++ch) {
                double old_val = std::max(0.0, std::min(1.0, buf[ch][y * w + x]));
                uint8_t new_val = (old_val >= 0.5) ? 1 : 0;
                out.at(x, y, ch) = new_val;

                double err = old_val - static_cast<double>(new_val);

                if (x + 1 < w)
                    buf[ch][y * w + (x + 1)]       += err * 7.0 / 16.0;
                if (y + 1 < h) {
                    if (x - 1 >= 0)
                        buf[ch][(y + 1) * w + (x - 1)] += err * 3.0 / 16.0;
                    buf[ch][(y + 1) * w + x]           += err * 5.0 / 16.0;
                    if (x + 1 < w)
                        buf[ch][(y + 1) * w + (x + 1)] += err * 1.0 / 16.0;
                }
            }
        }
    }
    return out;
}

double dot_density(const ImageMB& img, int channel) {
    int total = img.width * img.height;
    if (total == 0) return 0.0;
    int ones = 0;
    for (int i = 0; i < total; ++i)
        ones += img.data[i * img.channels + channel];
    return static_cast<double>(ones) / static_cast<double>(total);
}

}  // namespace halftone
}  // namespace jcolour
