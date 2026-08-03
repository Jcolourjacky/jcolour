#include "tiff_io.h"
#include "color_pipeline.h"
#include "delta_e.h"
#include <tiffio.h>
#include <tiff.h>
#include <csetjmp>
#include <iostream>
#include <algorithm>
#include <numeric>

namespace {

void tiff_error_handler(const char* module, const char* fmt, char* ap) {
    std::fprintf(stderr, "[TIFF Error %s] ", module ? module : "");
    std::vfprintf(stderr, fmt, ap);
    std::fprintf(stderr, "\n");
}

void tiff_warning_handler(const char* module, const char* fmt, char* ap) {
    std::fprintf(stderr, "[TIFF Warning %s] ", module ? module : "");
    std::vfprintf(stderr, fmt, ap);
    std::fprintf(stderr, "\n");
}

} // anonymous namespace

namespace jcolour {
namespace io {

struct TIFFReader::Impl {
    TIFF* tif = nullptr;

    ~Impl() {
        if (tif) TIFFClose(tif);
    }
};

TIFFReader::TIFFReader(const std::string& path) : pImpl_(new Impl) {
    TIFFSetErrorHandler(tiff_error_handler);
    TIFFSetWarningHandler(tiff_warning_handler);

    pImpl_->tif = TIFFOpen(path.c_str(), "r");
    if (pImpl_->tif) {
        uint32_t w, h;
        TIFFGetField(pImpl_->tif, TIFFTAG_IMAGEWIDTH, &w);
        TIFFGetField(pImpl_->tif, TIFFTAG_IMAGELENGTH, &h);
        uint16_t bits, samples, config;
        TIFFGetField(pImpl_->tif, TIFFTAG_BITSPERSAMPLE, &bits);
        TIFFGetField(pImpl_->tif, TIFFTAG_SAMPLESPERPIXEL, &samples);
        TIFFGetField(pImpl_->tif, TIFFTAG_PLANARCONFIG, &config);

        image_.width = w;
        image_.height = h;
        image_.bits_per_sample = bits;
        image_.samples_per_pixel = samples;
        image_.data.resize(w * h * samples);

        if (config == PLANARCONFIG_SEPARATE) {
            for (int s = 0; s < samples; ++s) {
                uint16_t scanline_size = TIFFScanlineSize(pImpl_->tif);
                std::vector<uint8_t> scanline(scanline_size);
                for (uint32_t row = 0; row < h; ++row) {
                    TIFFReadScanline(pImpl_->tif, scanline.data(), row, s);
                    int offset = (row * w + 0) * samples + s;
                    std::memcpy(&image_.data[offset], scanline.data(),
                               std::min((size_t)scanline_size, w * sizeof(uint16_t)));
                }
            }
        } else {
            for (uint32_t row = 0; row < h; ++row) {
                tdata_t buf = _TIFFmalloc(TIFFScanlineSize(pImpl_->tif));
                TIFFReadScanline(pImpl_->tif, buf, row, 0);
                std::memcpy(&image_.data[row * w * samples], buf,
                           w * samples * sizeof(uint16_t));
                _TIFFfree(buf);
            }
        }
    }
}

TIFFReader::~TIFFReader() = default;

bool TIFFReader::is_valid() const {
    return !image_.data.empty();
}

TIFFImage TIFFReader::read(const std::string& path) {
    TIFFReader reader(path);
    return reader.image_;
}

void TIFFReader::write(const std::string& path, const TIFFImage& image,
                       bool planar_config_contig) {

    TIFF* out = TIFFOpen(path.c_str(), "w");
    if (!out) return;

    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, image.width);
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, image.height);
    TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, image.bits_per_sample);
    TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, image.samples_per_pixel);
    TIFFSetField(out, TIFFTAG_PLANARCONFIG,
                 planar_config_contig ? PLANARCONFIG_CONTIG : PLANARCONFIG_SEPARATE);
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    TIFFSetField(out, TIFFTAG_XRESOLUTION, 300.0);
    TIFFSetField(out, TIFFTAG_YRESOLUTION, 300.0);
    TIFFSetField(out, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);

    int line_size = image.width * image.samples_per_pixel;
    for (uint32_t row = 0; row < image.height; ++row) {
        TIFFWriteScanline(out, const_cast<uint16_t*>(&image.data[row * line_size]), row, 0);
    }

    TIFFClose(out);
}

VerificationReport verify_against_reference(
    const TIFFImage& result,
    const std::string& reference_path,
    const char* profile_path) {

    VerificationReport report{};
    report.total_pixels = result.width * result.height;

    TIFFReader ref_reader(reference_path);
    if (!ref_reader.is_valid()) {
        return report;
    }

    const auto& ref = ref_reader.image();

    size_t gray_count = 0, skin_count = 0, saturated_count = 0;
    double gray_de_sum = 0, skin_de_sum = 0, saturated_de_sum = 0;

    for (int i = 0; i < result.width * result.height; ++i) {
        double r = result.data[i * 3 + 0] / 65535.0;
        double g = result.data[i * 3 + 1] / 65535.0;
        double b = result.data[i * 3 + 2] / 65535.0;

        auto spec1 = spectral::rgb_to_spectrum({r, g, b});
        auto lab1 = spectral::xyz_to_lab(spectral::spectrum_to_xyz(spec1));

        int ref_idx = std::min(i, ref.width * ref.height - 1);
        double rr = ref.data[ref_idx * 3 + 0] / 65535.0;
        double gr = ref.data[ref_idx * 3 + 1] / 65535.0;
        double br = ref.data[ref_idx * 3 + 2] / 65535.0;

        auto spec2 = spectral::rgb_to_spectrum({rr, gr, br});
        auto lab2 = spectral::xyz_to_lab(spectral::spectrum_to_xyz(spec2));

        double de = delta_e::delta_e_2000(lab1, lab2);

        if (de > report.max_de00) {
            report.max_de00 = de;
            report.max_de00_location[0] = i % result.width;
            report.max_de00_location[1] = i / result.width;
        }
        report.avg_de00 += de;

        double luminance = (r + g + b) / 3.0;
        bool is_gray = std::abs(r - g) < 0.02 && std::abs(g - b) < 0.02;
        bool is_skin = luminance > 0.3 && luminance < 0.7 &&
                       r > g && g > b * 0.8 && r < 0.9;
        bool is_saturated = (std::max({r, g, b}) - std::min({r, g, b})) > 0.7;

        if (is_gray) {
            gray_count++;
            gray_de_sum += de;
        } else if (is_skin) {
            skin_count++;
            skin_de_sum += de;
        } else if (is_saturated) {
            saturated_count++;
            saturated_de_sum += de;
        }

        if (de <= 1.0) report.pixels_within_de00_1_0++;
        if (de <= 0.5) report.pixels_within_de00_0_5++;
        if (de <= 1.5) report.pixels_within_de00_1_5++;
    }

    report.avg_de00 /= report.total_pixels;

    if (gray_count > 0) {
        report.gray_avg_de00 = gray_de_sum / gray_count;
    }
    if (skin_count > 0) {
        report.skin_avg_de00 = skin_de_sum / skin_count;
    }
    if (saturated_count > 0) {
        report.saturated_avg_de00 = saturated_de_sum / saturated_count;
    }

    return report;
}

}
}