#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "channel_layout.h"

namespace jcolour {
namespace pjl {

enum class InkMode : uint8_t {
    PhotoBlack = 0x00,
    MatteBlack = 0x01,
    PhotoBlackMatte = 0x02,
    LightBlackPhotoBlack = 0x03
};

enum class ColorMode : uint8_t {
    Color = 0x00,
    AdvancedBW = 0x01,
    Off = 0x02
};

struct PJLCommand {
    std::vector<uint8_t> data;

    static PJLCommand set_ink_mode(InkMode mode);
    static PJLCommand set_color_mode(ColorMode mode);
    static PJLCommand set_print_quality(int quality);
    static PJLCommand set_paper_size(const char* size);
    static PJLCommand set_paper_type(const char* type);
    static PJLCommand set_resolution(int dpi);
    static PJLCommand enter_language(const char* lang = "PCL");
    static PJLCommand query_status();
    static PJLCommand uel();
};

struct PrintJob {
    std::vector<uint8_t> pjl_header;
    std::vector<uint8_t> raster_data;
    std::vector<uint8_t> pjl_footer;
    bool is_16bit = false;
    int width_pixels = 0;
    int height_pixels = 0;
};

class EpsonPrinter {
public:
    struct Status {
        bool is_ready;
        std::string ink_status;
        int ink_levels[12];
        std::string error_message;
        int media_width;
        std::string media_type;
    };

    EpsonPrinter(const std::string& printer_uri);
    ~EpsonPrinter();

    bool connect();
    void disconnect();
    bool is_connected() const;

    bool send_pjl_command(const PJLCommand& cmd);
    Status query_status();
    bool print(const PrintJob& job);

    bool test_print_16bit();
    bool test_gray_scale_ink_density();
    bool test_single_black_mode(InkMode black_type);

    static std::vector<std::string> discover_printers();

    // is_sandbox_safe — returns true iff the PJL driver path can be used
    // from inside a macOS App Sandbox (i.e., for a Mac App Store build).
    //
    // **Currently returns false** because this implementation:
    //
    //   1. Uses raw TCP sockets (getaddrinfo + socket + connect) to talk to
    //      port 9100 on the printer. macOS sandboxed apps require the
    //      `com.apple.security.network.client` entitlement, which is
    //      permitted on MAS but must be declared.
    //
    //   2. Calls into CUPS (`cupsGetDests`) for discovery. CUPS access
    //      from a sandbox requires the printing entitlement
    //      `com.apple.security.print`, which IS allowed on MAS but again
    //      must be declared and triggers Apple's enhanced review.
    //
    //   3. Sends raw PJL/ESC-P-R bytes (not via CGPrintingTicket / IPP
    //      Everywhere), which is *not formally supported* in sandboxed
    //      apps. Apple has accepted apps doing this in the past, but it
    //      is the single biggest MAS-acceptance risk for jcolour — this
    //      is the Spike 2 red line in ROADMAP.md.
    //
    // **Spike 2 acceptance criteria** (until met, this function returns
    // false and the PJL path should not be enabled in MAS builds):
    //   - Build with App Sandbox entitlement = YES, network.client = YES,
    //     print = YES
    //   - Connect to a real P9580 / P9570 at port 9100 and send a small
    //     PJL test job
    //   - Verify on Sonoma 14.x and Sequoia 15.x
    //   - Submit to TestFlight via App Store Connect and confirm review
    //     does not reject "for accessing system networking outside of the
    //     supported APIs" or similar
    //
    // Until those steps are green, MAS builds should use the macOS
    // Print Dialog fallback path (not yet implemented) and leave this
    // helper returning false so callers can branch correctly.
    static bool is_sandbox_safe();

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

class RasterGenerator {
public:
    static std::vector<uint8_t> generate_raster_header(int width, int height, bool is_16bit);

    // Convert a 16-bit RGB image to a 12-channel PJL raster.
    //
    // Performs a simple CMYK separation (the only color separation we can
    // do without an ICC), feeds it into the canonical 11-active channel
    // layout (MAIN_K + C + VM + Y; the gamut-expanding inks LC/VLM/Or/GY/
    // LGY/V/Gr stay zero), and expands to the 12 physical PJL slots via
    // jcolour::expand_to_physical(). The unused black slot always carries
    // zero ink — the PK/MK mutual exclusivity rule is enforced by the type
    // system, not by best-effort masking.
    //
    // ink_limits is interpreted in the 12 physical slots:
    //   0:PK 1:MK 2:LC 3:C 4:VLM 5:VM 6:Y 7:Or 8:GY 9:LGY 10:V 11:Gr
    // Set the slot of the unused black (and any channel you want silenced)
    // to 0 to force it off — the existing single-black test mode relies on
    // this. If ink_limits is empty, all channels default to 1.0.
    static std::vector<uint8_t> convert_to_raster(const std::vector<uint16_t>& image_data,
                                                  int width, int height,
                                                  const std::vector<double>& ink_limits,
                                                  BlackKind black_kind = BlackKind::PK);

    static std::vector<uint8_t> generate_raster_footer();
};

}
}
