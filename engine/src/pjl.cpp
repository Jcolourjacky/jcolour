#include "pjl.h"
#include <cups/cups.h>
#include <cups/ppd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace jcolour {
namespace pjl {

// PJL command prefixes and suffixes
constexpr uint8_t ESC = 0x1B;
constexpr uint8_t CR = 0x0D;
constexpr uint8_t LF = 0x0A;

constexpr const char* PJL_UEL = "\x1B%-12345X";
constexpr const char* PJL_PREFIX = "@PJL ";

PJLCommand PJLCommand::set_ink_mode(InkMode mode) {
    PJLCommand cmd;
    const char* mode_str;
    switch (mode) {
        case InkMode::PhotoBlack: mode_str = "PHOTOBLACK"; break;
        case InkMode::MatteBlack: mode_str = "MATTE"; break;
        case InkMode::PhotoBlackMatte: mode_str = "PHOTOBLACKMATTE"; break;
        case InkMode::LightBlackPhotoBlack: mode_str = "LKBK"; break;
        default: mode_str = "PHOTOBLACK"; break;
    }
    std::string pjl = PJL_PREFIX + std::string("SET INKMODE=") + mode_str + "\r\n";
    cmd.data.insert(cmd.data.end(), pjl.begin(), pjl.end());
    return cmd;
}

PJLCommand PJLCommand::set_color_mode(ColorMode mode) {
    PJLCommand cmd;
    const char* mode_str;
    switch (mode) {
        case ColorMode::Color: mode_str = "COLOR"; break;
        case ColorMode::AdvancedBW: mode_str = "ADVBW"; break;
        case ColorMode::Off: mode_str = "OFF"; break;
        default: mode_str = "OFF"; break;
    }
    std::string pjl = PJL_PREFIX + std::string("SET COLORMODE=") + mode_str + "\r\n";
    cmd.data.insert(cmd.data.end(), pjl.begin(), pjl.end());
    return cmd;
}

PJLCommand PJLCommand::set_print_quality(int quality) {
    std::ostringstream pjl;
    pjl << PJL_PREFIX << "SET QUALITY=PRINT" << quality << "\r\n";
    PJLCommand cmd;
    std::string str = pjl.str();
    cmd.data.insert(cmd.data.end(), str.begin(), str.end());
    return cmd;
}

PJLCommand PJLCommand::set_paper_size(const char* size) {
    std::ostringstream pjl;
    pjl << PJL_PREFIX << "SET PAPERSIZE=" << size << "\r\n";
    PJLCommand cmd;
    std::string str = pjl.str();
    cmd.data.insert(cmd.data.end(), str.begin(), str.end());
    return cmd;
}

PJLCommand PJLCommand::set_paper_type(const char* type) {
    std::ostringstream pjl;
    pjl << PJL_PREFIX << "SET MEDIATYPE=" << type << "\r\n";
    PJLCommand cmd;
    std::string str = pjl.str();
    cmd.data.insert(cmd.data.end(), str.begin(), str.end());
    return cmd;
}

PJLCommand PJLCommand::set_resolution(int dpi) {
    std::ostringstream pjl;
    pjl << PJL_PREFIX << "SET RESOLUTION=" << dpi << "\r\n";
    PJLCommand cmd;
    std::string str = pjl.str();
    cmd.data.insert(cmd.data.end(), str.begin(), str.end());
    return cmd;
}

PJLCommand PJLCommand::enter_language(const char* lang) {
    std::ostringstream pjl;
    pjl << PJL_PREFIX << "ENTER LANGUAGE=" << lang << "\r\n";
    PJLCommand cmd;
    std::string str = pjl.str();
    cmd.data.insert(cmd.data.end(), str.begin(), str.end());
    return cmd;
}

PJLCommand PJLCommand::query_status() {
    std::ostringstream pjl;
    pjl << PJL_PREFIX << "INFO STATUS\r\n";
    PJLCommand cmd;
    std::string str = pjl.str();
    cmd.data.insert(cmd.data.end(), str.begin(), str.end());
    return cmd;
}

PJLCommand PJLCommand::uel() {
    PJLCommand cmd;
    std::string str(PJL_UEL);
    cmd.data.insert(cmd.data.end(), str.begin(), str.end());
    return cmd;
}

struct EpsonPrinter::Impl {
    std::string printer_uri;
    int sockfd = -1;
    bool connected = false;

    Impl(const std::string& uri) : printer_uri(uri) {}

    ~Impl() {
        if (sockfd >= 0) {
            ::close(sockfd);
            sockfd = -1;
        }
    }

    // Parse a printer URI into host + port. Supported forms:
    //   socket://host             → port 9100 (raw JetDirect, Epson default)
    //   socket://host:NNNN        → explicit port
    //   ipp://host                → port 631
    //   ipp://host:NNNN/queue     → explicit port (queue path is ignored)
    //   host                      → bare host, port 9100
    //   host:NNNN                 → bare host + explicit port
    static bool parse_uri(const std::string& uri,
                          std::string& host, int& port) {
        // Default port = 9100 (JetDirect raw). Audit F18: was 80 (HTTP),
        // which Epson printers do not listen on for print jobs.
        constexpr int DEFAULT_RAW_PORT = 9100;
        constexpr int DEFAULT_IPP_PORT = 631;
        port = DEFAULT_RAW_PORT;

        std::string remainder = uri;
        if (uri.substr(0, 9) == "socket://") {
            remainder = uri.substr(9);
            port = DEFAULT_RAW_PORT;
        } else if (uri.substr(0, 6) == "ipp://") {
            remainder = uri.substr(6);
            port = DEFAULT_IPP_PORT;
        } else if (uri.substr(0, 7) == "ipps://") {
            remainder = uri.substr(7);
            port = DEFAULT_IPP_PORT;
        }

        // Strip any path component (".../queue" → host is everything before /)
        std::string host_port = remainder.substr(0, remainder.find('/'));
        if (host_port.empty()) return false;

        // Bracketed IPv6 literal: [::1]:631
        if (host_port.front() == '[') {
            auto end = host_port.find(']');
            if (end == std::string::npos) return false;
            host = host_port.substr(1, end - 1);
            if (end + 1 < host_port.size() && host_port[end + 1] == ':') {
                try { port = std::stoi(host_port.substr(end + 2)); }
                catch (...) { return false; }
            }
            return !host.empty();
        }

        // host:port — last colon (so IPv6 without brackets is rejected; if
        // the user wants IPv6 they should bracket it).
        auto colon = host_port.rfind(':');
        if (colon != std::string::npos) {
            host = host_port.substr(0, colon);
            try { port = std::stoi(host_port.substr(colon + 1)); }
            catch (...) { return false; }
        } else {
            host = host_port;
        }
        return !host.empty();
    }

    bool connect_socket() {
        if (connected) return true;
        if (sockfd >= 0) {
            ::close(sockfd);
            sockfd = -1;
        }

        std::string host;
        int port = 0;
        if (!parse_uri(printer_uri, host, port)) {
            return false;
        }

        // getaddrinfo: IPv4/IPv6 capable, thread-safe, replaces deprecated
        // gethostbyname (audit F19/F27).
        struct addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo* res = nullptr;
        std::string port_str = std::to_string(port);
        if (::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) {
            return false;
        }

        bool ok = false;
        for (auto* p = res; p; p = p->ai_next) {
            int fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (fd < 0) continue;
            if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
                sockfd = fd;
                ok = true;
                break;
            }
            ::close(fd);
        }
        ::freeaddrinfo(res);

        if (!ok) return false;

        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
        connected = true;
        return true;
    }

    // Robust send-all. connect_socket() puts the socket in O_NONBLOCK (so
    // recv() in query_status can't hang forever), which means a single
    // ::send() of a large raster returns a short write — or EAGAIN — and the
    // rest of the job is silently dropped. Loop until the whole buffer is
    // flushed, waiting for writability on EAGAIN. Returns total bytes sent
    // (== data.size() on success) or -1 on a hard error / timeout.
    ssize_t send_data(const std::vector<uint8_t>& data) {
        if (!connected) {
            if (!connect_socket()) return -1;
        }
        size_t total = 0;
        while (total < data.size()) {
            ssize_t n = ::send(sockfd, data.data() + total,
                               data.size() - total, 0);
            if (n > 0) {
                total += static_cast<size_t>(n);
                continue;
            }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                struct pollfd pfd{};
                pfd.fd = sockfd;
                pfd.events = POLLOUT;
                if (::poll(&pfd, 1, 30000) <= 0) return -1; // timeout / error
                continue;
            }
            if (n < 0 && errno == EINTR) continue;
            return -1; // hard error (peer reset, etc.)
        }
        return static_cast<ssize_t>(total);
    }

    std::vector<uint8_t> receive_data(size_t max_len = 4096) {
        std::vector<uint8_t> buf(max_len);
        ssize_t n = recv(sockfd, buf.data(), max_len, 0);
        if (n > 0) {
            buf.resize(n);
        } else {
            buf.clear();
        }
        return buf;
    }
};

EpsonPrinter::EpsonPrinter(const std::string& printer_uri)
    : pImpl_(new Impl(printer_uri)) {}

EpsonPrinter::~EpsonPrinter() = default;

bool EpsonPrinter::connect() {
    return pImpl_->connect_socket();
}

void EpsonPrinter::disconnect() {
    if (pImpl_->sockfd >= 0) {
        ::close(pImpl_->sockfd);
        pImpl_->sockfd = -1;
    }
    pImpl_->connected = false;
}

bool EpsonPrinter::is_connected() const {
    return pImpl_->connected;
}

bool EpsonPrinter::send_pjl_command(const PJLCommand& cmd) {
    return pImpl_->send_data(cmd.data) >= 0;
}

EpsonPrinter::Status EpsonPrinter::query_status() {
    Status status = {};
    status.is_ready = false;

    if (!pImpl_->connected) {
        if (!pImpl_->connect_socket()) {
            status.error_message = "Cannot connect to printer";
            return status;
        }
    }

    auto cmd = PJLCommand::query_status();
    pImpl_->send_data(cmd.data);

    auto response = pImpl_->receive_data();
    if (!response.empty()) {
        status.is_ready = true;
    }

    return status;
}

bool EpsonPrinter::print(const PrintJob& job) {
    if (!pImpl_->connected) {
        if (!pImpl_->connect_socket()) {
            return false;
        }
    }

    std::vector<uint8_t> full_job;
    full_job.insert(full_job.end(), job.pjl_header.begin(), job.pjl_header.end());
    full_job.insert(full_job.end(), job.raster_data.begin(), job.raster_data.end());
    full_job.insert(full_job.end(), job.pjl_footer.begin(), job.pjl_footer.end());

    ssize_t sent = pImpl_->send_data(full_job);
    return sent == static_cast<ssize_t>(full_job.size());
}

bool EpsonPrinter::test_print_16bit() {
    constexpr int WIDTH = 256;
    constexpr int HEIGHT = 64;

    std::vector<uint16_t> test_data(WIDTH * HEIGHT * 3);
    for (int i = 0; i < WIDTH * HEIGHT; ++i) {
        double value = (i % WIDTH) / 255.0;
        test_data[i * 3 + 0] = static_cast<uint16_t>(value * 65535);
        test_data[i * 3 + 1] = static_cast<uint16_t>(value * 65535);
        test_data[i * 3 + 2] = static_cast<uint16_t>(value * 65535);
    }

    std::vector<uint8_t> raster = RasterGenerator::convert_to_raster(
        test_data, WIDTH, HEIGHT, {});

    PrintJob job;
    job.pjl_header = PJLCommand::set_color_mode(ColorMode::Off).data;
    job.pjl_header.insert(job.pjl_header.end(),
        PJLCommand::enter_language("PCL").data.begin(),
        PJLCommand::enter_language("PCL").data.end());
    job.raster_data = raster;
    job.pjl_footer = PJLCommand::uel().data;
    job.is_16bit = true;
    job.width_pixels = WIDTH;
    job.height_pixels = HEIGHT;

    return print(job);
}

bool EpsonPrinter::test_gray_scale_ink_density() {
    constexpr int STEPS = 16;
    constexpr int STRIP_HEIGHT = 32;
    constexpr int STRIP_WIDTH = 128;

    std::vector<uint16_t> gray_data(STRIP_WIDTH * STRIP_HEIGHT * STEPS * 3);
    for (int step = 0; step < STEPS; ++step) {
        double value = step / static_cast<double>(STEPS - 1);
        for (int y = 0; y < STRIP_HEIGHT; ++y) {
            for (int x = 0; x < STRIP_WIDTH; ++x) {
                int idx = (step * STRIP_HEIGHT + y) * STRIP_WIDTH + x;
                gray_data[idx * 3 + 0] = static_cast<uint16_t>(value * 65535);
                gray_data[idx * 3 + 1] = static_cast<uint16_t>(value * 65535);
                gray_data[idx * 3 + 2] = static_cast<uint16_t>(value * 65535);
            }
        }
    }

    std::vector<double> ink_limits(12, 1.0);
    auto raster = RasterGenerator::convert_to_raster(
        gray_data, STRIP_WIDTH, STRIP_HEIGHT * STEPS, ink_limits);

    PrintJob job;
    job.pjl_header = PJLCommand::set_color_mode(ColorMode::Off).data;
    job.raster_data = raster;
    job.pjl_footer = PJLCommand::uel().data;
    job.is_16bit = true;

    return print(job);
}

bool EpsonPrinter::test_single_black_mode(InkMode black_type) {
    constexpr int WIDTH = 256;
    constexpr int HEIGHT = 256;

    std::vector<uint16_t> bw_data(WIDTH * HEIGHT * 3);
    for (int i = 0; i < WIDTH * HEIGHT; ++i) {
        double value = (i % WIDTH) / 255.0;
        bw_data[i * 3 + 0] = static_cast<uint16_t>(value * 65535);
        bw_data[i * 3 + 1] = static_cast<uint16_t>(value * 65535);
        bw_data[i * 3 + 2] = static_cast<uint16_t>(value * 65535);
    }

    std::vector<double> single_black_limits(12, 0.0);
    if (black_type == InkMode::PhotoBlack) {
        single_black_limits[0] = 1.0;
    } else if (black_type == InkMode::MatteBlack) {
        single_black_limits[1] = 1.0;
    }

    auto raster = RasterGenerator::convert_to_raster(
        bw_data, WIDTH, HEIGHT, single_black_limits);

    PrintJob job;
    job.pjl_header = PJLCommand::set_ink_mode(black_type).data;
    job.pjl_header.insert(job.pjl_header.end(),
        PJLCommand::set_color_mode(ColorMode::Off).data.begin(),
        PJLCommand::set_color_mode(ColorMode::Off).data.end());
    job.raster_data = raster;
    job.pjl_footer = PJLCommand::uel().data;
    job.is_16bit = true;
    job.width_pixels = WIDTH;
    job.height_pixels = HEIGHT;

    return print(job);
}

std::vector<std::string> EpsonPrinter::discover_printers() {
    std::vector<std::string> printers;

    cups_dest_t* dests = nullptr;
    int num_dests = cupsGetDests(&dests);

    for (int i = 0; i < num_dests; ++i) {
        if (dests[i].name && strstr(dests[i].name, "Epson") != nullptr) {
            printers.push_back(dests[i].name);
        }
    }

    cupsFreeDests(num_dests, dests);
    return printers;
}

bool EpsonPrinter::is_sandbox_safe() {
    return false;
}

std::vector<uint8_t> RasterGenerator::generate_raster_header(int width, int height, bool is_16bit) {
    std::vector<uint8_t> header;

    // ESC ( G - Enter raster graphics mode
    header.push_back(ESC);
    header.push_back('(');
    header.push_back('G');

    // Raster data length (placeholder, fill in actual)
    header.push_back(0x00);
    header.push_back(0x00);

    return header;
}

std::vector<uint8_t> RasterGenerator::convert_to_raster(
    const std::vector<uint16_t>& image_data,
    int width, int height,
    const std::vector<double>& ink_limits,
    BlackKind black_kind) {

    std::vector<uint8_t> raster;

    auto limit_of = [&](int slot) -> double {
        if (ink_limits.empty()) return 1.0;
        return (slot < static_cast<int>(ink_limits.size())) ? ink_limits[slot] : 1.0;
    };

    for (int y = 0; y < height; ++y) {
        std::array<std::vector<uint8_t>, NUM_PHYSICAL_CHANNELS> plane_data;
        for (auto& plane : plane_data) {
            plane.resize((width + 7) / 8);
            std::fill(plane.begin(), plane.end(), 0);
        }

        for (int x = 0; x < width; ++x) {
            int pixel_idx = y * width + x;
            double r = image_data[pixel_idx * 3 + 0] / 65535.0;
            double g = image_data[pixel_idx * 3 + 1] / 65535.0;
            double b = image_data[pixel_idx * 3 + 2] / 65535.0;

            // Standard CMYK separation. This is the only color separation
            // we can do without an ICC profile — gamut-expanding inks
            // (LC, VLM, Or, GY, LGY, V, Gr) stay at zero. wf4/wf6 do
            // proper 11-channel separation through ICC; this raster
            // generator is the no-ICC fallback path.
            double cyan = std::max(0.0, 1.0 - r);
            double magenta = std::max(0.0, 1.0 - g);
            double yellow = std::max(0.0, 1.0 - b);
            double black = std::min({cyan, magenta, yellow});
            cyan    -= black;
            magenta -= black;
            yellow  -= black;

            // Build canonical ActiveInks (11 entries). MAIN_K stands in
            // for whichever black is loaded; PK/MK mutual exclusivity is
            // structurally guaranteed here.
            ActiveInks active{};
            active[idx(ActiveChannel::MAIN_K)] = static_cast<float>(black);
            active[idx(ActiveChannel::C)]      = static_cast<float>(cyan);
            active[idx(ActiveChannel::VM)]     = static_cast<float>(magenta);
            active[idx(ActiveChannel::Y)]      = static_cast<float>(yellow);
            // Remaining 7 active channels intentionally stay at 0.

            // Expand to 12 physical slots — MAIN_K lands in slot 0 (PK)
            // or slot 1 (MK), the other black slot is guaranteed 0.
            PhysicalInks physical = expand_to_physical(active, black_kind);

            // Apply per-slot ink_limits in the physical 12-slot layout.
            // This preserves the existing single-black test mode that
            // silences specific slots via limit = 0.
            int byte_idx = x / 8;
            int bit_idx = 7 - (x % 8);
            for (int slot = 0; slot < NUM_PHYSICAL_CHANNELS; ++slot) {
                double value = physical[slot] * limit_of(slot);
                if (value > 0.5) {
                    plane_data[slot][byte_idx] |= (1 << bit_idx);
                }
            }
        }

        for (auto& plane : plane_data) {
            raster.insert(raster.end(), plane.begin(), plane.end());
        }
    }

    return raster;
}

std::vector<uint8_t> RasterGenerator::generate_raster_footer() {
    return {ESC, '(', 'G', 0x00, 0x00};
}

}
}