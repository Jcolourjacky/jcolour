#include "stp_backend.h"

#include <algorithm>
#include <cstdio>
#include <ostream>
#include <string>
#include <vector>

// ===========================================================================
// run_stp_backend(): the ONLY translation unit that touches libgutenprint.
//
// Built in two flavours:
//   * JCOLOUR_HAVE_GUTENPRINT defined  -> real Gutenprint render path (GPL).
//   * otherwise                        -> stub that returns a "not configured"
//                                         error, so the skeleton still builds.
//
// The Gutenprint path below is structurally complete but MUST be verified
// against the installed <gutenprint/gutenprint.h> once Gutenprint is present
// (parameter names and a few signatures vary slightly across 5.3.x point
// releases). It is intentionally isolated here so the GPL dependency never
// leaks into the engine/app.
// ===========================================================================

#ifdef JCOLOUR_HAVE_GUTENPRINT
#include <gutenprint/gutenprint.h>
#endif

namespace jcolour {
namespace stp {

namespace {

bool validate(const StpJobRequest& req, std::ostream& err) {
    if (req.width_px <= 0 || req.height_px <= 0) {
        err << "stp-backend: invalid dimensions "
            << req.width_px << "x" << req.height_px << "\n";
        return false;
    }
    if (req.input.empty()) {
        err << "stp-backend: empty input path\n";
        return false;
    }
    return true;
}

// Load raw 8-bit interleaved RGB (width*height*3) from `path`.
// (Unused in the stub build that lacks libgutenprint.)
[[maybe_unused]] bool load_rgb(const std::string& path, size_t expected,
                               std::vector<unsigned char>& buf, std::ostream& err) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        err << "stp-backend: cannot open input \"" << path << "\"\n";
        return false;
    }
    buf.resize(expected);
    const size_t got = std::fread(buf.data(), 1, expected, f);
    std::fclose(f);
    if (got != expected) {
        err << "stp-backend: short read on \"" << path << "\": got " << got
            << " of " << expected << " bytes\n";
        return false;
    }
    return true;
}

#ifdef JCOLOUR_HAVE_GUTENPRINT

// stp_image_t backing store over an in-memory raw RGB buffer.
struct RawRgbImage {
    const unsigned char* data = nullptr;
    int width  = 0;
    int height = 0;
};

void img_init(stp_image_t*) {}
void img_reset(stp_image_t*) {}
int  img_width(stp_image_t* im)  { return static_cast<RawRgbImage*>(im->rep)->width; }
int  img_height(stp_image_t* im) { return static_cast<RawRgbImage*>(im->rep)->height; }

stp_image_status_t img_get_row(stp_image_t* im, unsigned char* out,
                               size_t byte_limit, int row) {
    auto* r = static_cast<RawRgbImage*>(im->rep);
    const size_t row_bytes = static_cast<size_t>(r->width) * 3;
    if (row < 0 || row >= r->height || row_bytes > byte_limit) {
        return STP_IMAGE_STATUS_ABORT;
    }
    std::copy(r->data + static_cast<size_t>(row) * row_bytes,
              r->data + static_cast<size_t>(row + 1) * row_bytes, out);
    return STP_IMAGE_STATUS_OK;
}

const char* img_appname(stp_image_t*) { return "jcolour-stp-backend"; }
void        img_conclude(stp_image_t*) {}

void out_cb(void* data, const char* buf, size_t bytes) {
    std::fwrite(buf, 1, bytes, static_cast<FILE*>(data));
}

void err_cb(void* data, const char* buf, size_t bytes) {
    auto* es = static_cast<std::ostream*>(data);
    es->write(buf, static_cast<std::streamsize>(bytes));
}

#endif  // JCOLOUR_HAVE_GUTENPRINT

}  // namespace

#ifdef JCOLOUR_HAVE_GUTENPRINT

int run_stp_backend(const StpJobRequest& req, std::ostream& err) {
    if (!validate(req, err)) return 2;

    const size_t expected =
        static_cast<size_t>(req.width_px) * req.height_px * 3;
    std::vector<unsigned char> rgb;
    if (!load_rgb(req.input, expected, rgb, err)) return 3;

    FILE* out = (req.output == "-") ? stdout
                                    : std::fopen(req.output.c_str(), "wb");
    if (!out) {
        err << "stp-backend: cannot open output \"" << req.output << "\"\n";
        return 4;
    }

    stp_init();

    const stp_printer_t* printer = stp_get_printer_by_driver(req.model.c_str());
    if (!printer) {
        err << "stp-backend: unknown Gutenprint driver \"" << req.model << "\"\n";
        if (out != stdout) std::fclose(out);
        return 5;
    }

    stp_vars_t* v = stp_vars_create();
    stp_set_driver(v, req.model.c_str());
    stp_set_printer_defaults(v, printer);

    stp_set_outfunc(v, out_cb);
    stp_set_outdata(v, out);
    stp_set_errfunc(v, err_cb);
    stp_set_errdata(v, &err);

    stp_set_string_parameter(v, "PageSize", req.pagesize.c_str());
    stp_set_string_parameter(v, "MediaType", req.media.c_str());
    stp_set_string_parameter(v, "InputImageType", "RGB");
    if (!req.resolution.empty()) {
        stp_set_string_parameter(v, "Resolution", req.resolution.c_str());
    }
    stp_set_float_parameter(v, "Density", req.ink_limit);

    // Place inside the printer's imageable area. Origin must respect the
    // hardware minimum margins (stp_verify rejects left/top below them); size
    // is the requested px/dpi extent, clamped to what fits. Units are 1/72".
    stp_dimension_t ia_left = 0, ia_right = 0, ia_bottom = 0, ia_top = 0;
    stp_get_imageable_area(v, &ia_left, &ia_right, &ia_bottom, &ia_top);
    const stp_dimension_t avail_w = ia_right - ia_left;
    const stp_dimension_t avail_h = ia_bottom - ia_top;
    stp_dimension_t want_w = 72.0 * req.width_px  / req.dpi;
    stp_dimension_t want_h = 72.0 * req.height_px / req.dpi;
    if (avail_w > 0 && want_w > avail_w) want_w = avail_w;
    if (avail_h > 0 && want_h > avail_h) want_h = avail_h;
    stp_set_left(v, ia_left);
    stp_set_top(v, ia_top);
    stp_set_width(v, want_w);
    stp_set_height(v, want_h);

    RawRgbImage rep{rgb.data(), req.width_px, req.height_px};
    stp_image_t image;
    image.init        = img_init;
    image.reset       = img_reset;
    image.width       = img_width;
    image.height      = img_height;
    image.get_row     = img_get_row;
    image.get_appname = img_appname;
    image.conclude    = img_conclude;
    image.rep         = &rep;

    int rc = 0;
    if (!stp_verify(v)) {
        err << "stp-backend: stp_verify() failed for model \"" << req.model << "\"\n";
        rc = 6;
    } else if (!stp_print(v, &image)) {
        err << "stp-backend: stp_print() failed\n";
        rc = 7;
    }

    stp_vars_destroy(v);
    if (out != stdout) std::fclose(out);
    return rc;
}

#else  // !JCOLOUR_HAVE_GUTENPRINT

int run_stp_backend(const StpJobRequest& req, std::ostream& err) {
    if (!validate(req, err)) return 2;
    err << "stp-backend: built WITHOUT libgutenprint (JCOLOUR_HAVE_GUTENPRINT "
           "undefined). Install Gutenprint and reconfigure CMake so the "
           "backend can drive model \""
        << req.model << "\".\n";
    return 64;  // configuration error.
}

#endif  // JCOLOUR_HAVE_GUTENPRINT

}  // namespace stp
}  // namespace jcolour
