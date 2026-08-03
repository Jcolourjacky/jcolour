#pragma once

// Halftone interface and scalar error diffusion.
//
// Halftoning converts continuous-tone coverage values [0,1] to binary
// dot patterns. This module provides:
//   - A common Image type (single-channel float and binary)
//   - Floyd-Steinberg error diffusion for a single channel
//   - Vector error diffusion for N channels with shared error buffer

#include <vector>
#include <cstdint>
#include <functional>

namespace jcolour {
namespace halftone {

// Single-channel image: row-major, values in [0,1].
struct ImageF {
    int                 width;
    int                 height;
    std::vector<double> data;  // data[y*width + x]

    ImageF(int w, int h, double fill = 0.0)
        : width(w), height(h), data(w * h, fill) {}

    double  get(int x, int y) const { return data[y * width + x]; }
    double& at (int x, int y)       { return data[y * width + x]; }
};

// Single-channel binary image: 0 or 1.
struct ImageB {
    int                    width;
    int                    height;
    std::vector<uint8_t>   data;  // 0 or 1

    ImageB(int w, int h) : width(w), height(h), data(w * h, 0) {}

    uint8_t  get(int x, int y) const { return data[y * width + x]; }
    uint8_t& at (int x, int y)       { return data[y * width + x]; }
};

// Multi-channel image: N channels, row-major.
// data[y*width*channels + x*channels + c]
struct ImageMF {
    int                 width;
    int                 height;
    int                 channels;
    std::vector<double> data;

    ImageMF(int w, int h, int c, double fill = 0.0)
        : width(w), height(h), channels(c), data(w * h * c, fill) {}

    double  get(int x, int y, int c) const { return data[(y*width + x)*channels + c]; }
    double& at (int x, int y, int c)       { return data[(y*width + x)*channels + c]; }
};

// Multi-channel binary image: N channels, row-major.
struct ImageMB {
    int                  width;
    int                  height;
    int                  channels;
    std::vector<uint8_t> data;  // 0 or 1

    ImageMB(int w, int h, int c)
        : width(w), height(h), channels(c), data(w * h * c, 0) {}

    uint8_t  get(int x, int y, int c) const { return data[(y*width + x)*channels + c]; }
    uint8_t& at (int x, int y, int c)       { return data[(y*width + x)*channels + c]; }
};

// Floyd-Steinberg error diffusion (single channel).
ImageB floyd_steinberg(const ImageF& input);

// Vector error diffusion (N channels, shared error buffer).
// Each channel is halftoned independently using Floyd-Steinberg, but
// the error buffer is shared across channels so that total ink load
// is distributed evenly across the dot pattern.
ImageMB vector_error_diffusion(const ImageMF& input);

// Measure dot density of a binary image: fraction of pixels that are 1.
double dot_density(const ImageB& img);

// Measure dot density of one channel in a multi-channel binary image.
double dot_density(const ImageMB& img, int channel);

}  // namespace halftone
}  // namespace jcolour

