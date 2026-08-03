#pragma once

#include "spectral.h"
#include <string>
#include <memory>

namespace jcolour {
namespace io {

struct TIFFImage {
    int width = 0;
    int height = 0;
    int bits_per_sample = 0;
    int samples_per_pixel = 0;
    std::vector<uint16_t> data;
};

class TIFFReader {
public:
    explicit TIFFReader(const std::string& path);
    ~TIFFReader();

    bool is_valid() const;
    const TIFFImage& image() const { return image_; }

    static TIFFImage read(const std::string& path);
    static void write(const std::string& path, const TIFFImage& image,
                      bool planar_config_contig = true);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
    TIFFImage image_;
};

struct VerificationReport {
    int total_pixels = 0;
    int pixels_within_de00_1_0 = 0;
    int pixels_within_de00_0_5 = 0;
    int pixels_within_de00_1_5 = 0;
    double max_de00 = 0.0;
    double avg_de00 = 0.0;
    double gray_avg_de00 = 0.0;
    double skin_avg_de00 = 0.0;
    double saturated_avg_de00 = 0.0;
    double max_de00_location[2] = {0.0, 0.0};
};

VerificationReport verify_against_reference(const TIFFImage& result,
                                           const std::string& reference_path,
                                           const char* profile_path);

}
}