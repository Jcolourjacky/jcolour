#include "epson.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <sstream>

// For mDNS/Bonjour discovery
#include <dns_sd.h>

namespace jcolour {
namespace epson {

namespace {

// 1-second connect timeout (non-blocking + select). Keeps unit tests fast
// when the target host is unreachable while still tolerating a busy LAN.
constexpr int kConnectTimeoutSec = 1;
// 5-second send/recv timeout once a connection is up — tolerates a busy
// printer page-pump.
constexpr int kIoTimeoutSec = 5;

// Resolve hostname to sockaddr_in (both IPv4 literal and DNS name).
static bool resolve_host(const std::string& host, sockaddr_in& out) {
    std::memset(&out, 0, sizeof(out));
    out.sin_family = AF_INET;

    if (::inet_pton(AF_INET, host.c_str(), &out.sin_addr) == 1) {
        return true;
    }
    addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (::getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) {
        return false;
    }
    auto* a = reinterpret_cast<sockaddr_in*>(res->ai_addr);
    out.sin_addr = a->sin_addr;
    ::freeaddrinfo(res);
    return true;
}

}  // namespace

// ---- B2: mDNS/Bonjour printer discovery ------------------------------------

namespace {

// Callback for DNSServiceBrowse - collects resolved printer addresses
struct BrowseContext {
    std::vector<DiscoveredPrinter> printers;
    DNSServiceRef                 resolve_ref = nullptr;
};

// Resolve callback: convert service to IP address
void DNSSD_API resolve_callback(
    DNSServiceRef, DNSServiceFlags, uint32_t interfaceIndex, DNSServiceErrorType error,
    const char* fullname, const char* hosttarget, uint16_t port, uint16_t txtLen,
    const unsigned char* txtRecord, void* context) {
    auto* ctx = static_cast<BrowseContext*>(context);
    if (error != kDNSServiceErr_NoError) return;

    // Extract name from fullname (e.g., "EPSON P9580._pdl-datastream._tcp.local")
    std::string name = fullname ? fullname : "";
    size_t dot = name.find("._pdl-datastream");
    if (dot != std::string::npos) {
        name = name.substr(0, dot);
    }

    // Convert hosttarget (hostname) to IP address
    sockaddr_in addr{};
    if (resolve_host(hosttarget, addr)) {
        DiscoveredPrinter p;
        p.name = name;
        p.host = inet_ntoa(addr.sin_addr);
        p.port = ntohs(port);
        ctx->printers.push_back(p);
    }
    (void)interfaceIndex;
    (void)txtLen;
    (void)txtRecord;
}

// Browse callback: trigger resolve for each found service
void DNSSD_API browse_callback(
    DNSServiceRef, DNSServiceFlags flags, uint32_t interfaceIndex,
    DNSServiceErrorType error, const char* serviceName, const char* regtype,
    const char* domain, void* context) {
    auto* ctx = static_cast<BrowseContext*>(context);
    if (!(flags & kDNSServiceFlagsAdd) || error != kDNSServiceErr_NoError) return;

    DNSServiceRef ref = nullptr;
    DNSServiceErrorType err = DNSServiceResolve(
        &ref, 0, interfaceIndex, serviceName, regtype, domain, resolve_callback, ctx);
    if (err == kDNSServiceErr_NoError && ref) {
        // Keep ref alive during resolve
        DNSServiceRefDeallocate(ref);
    }
    (void)serviceName;
}

}  // anonymous namespace

std::vector<DiscoveredPrinter> discover_printers() {
    BrowseContext ctx;
    DNSServiceRef service_ref = nullptr;

    DNSServiceErrorType err = DNSServiceBrowse(
        &service_ref,
        0,                    // flags
        0,                    // interfaceIndex (0 = all)
        "_pdl-datastream._tcp",  // service type
        "",                   // domain (empty = default)
        browse_callback,
        &ctx);

    if (err != kDNSServiceErr_NoError || !service_ref) {
        return {};  // Bonjour not available
    }

    // Process browse results for 3 seconds to collect printers
    int sockfd = DNSServiceRefSockFD(service_ref);
    fd_set read_fds;
    timeval tv{3, 0};

    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        int n = select(sockfd + 1, &read_fds, nullptr, nullptr, &tv);
        if (n <= 0 || tv.tv_sec == 0) break;
        DNSServiceProcessResult(service_ref);
    }

    DNSServiceRefDeallocate(service_ref);
    return ctx.printers;
}

struct Printer::Impl {
    std::string host;
    int         port      = 9100;
    int         sockfd    = -1;
    bool        connected = false;

    Impl(const std::string& h, int p) : host(h), port(p) {}

    ~Impl() { close_socket(); }

    void close_socket() {
        if (sockfd >= 0) {
            ::close(sockfd);
            sockfd = -1;
        }
        connected = false;
    }

    bool open_socket() {
        close_socket();

        sockaddr_in addr{};
        if (!resolve_host(host, addr)) return false;
        addr.sin_port = htons(static_cast<uint16_t>(port));

        sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) return false;

        // Non-blocking connect so we can cap the wait with select().
        const int orig_flags = ::fcntl(sockfd, F_GETFL, 0);
        ::fcntl(sockfd, F_SETFL, orig_flags | O_NONBLOCK);

        int rc = ::connect(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (rc < 0 && errno != EINPROGRESS) {
            close_socket();
            return false;
        }

        if (rc < 0) {
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(sockfd, &wfds);
            timeval to{kConnectTimeoutSec, 0};
            int sel = ::select(sockfd + 1, nullptr, &wfds, nullptr, &to);
            if (sel <= 0) {
                close_socket();
                return false;
            }
            int err = 0;
            socklen_t errlen = sizeof(err);
            if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &errlen) < 0 || err != 0) {
                close_socket();
                return false;
            }
        }

        // Restore blocking mode + apply send/recv timeouts for the data phase.
        ::fcntl(sockfd, F_SETFL, orig_flags);

        timeval tv{kIoTimeoutSec, 0};
        ::setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        ::setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        int one = 1;
        ::setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

        connected = true;
        return true;
    }

    bool send_all(const uint8_t* data, size_t len) {
        if (!connected && !open_socket()) return false;
        size_t sent = 0;
        while (sent < len) {
            ssize_t n = ::send(sockfd, data + sent, len - sent, 0);
            if (n <= 0) {
                if (n < 0 && (errno == EINTR)) continue;
                close_socket();
                return false;
            }
            sent += static_cast<size_t>(n);
        }
        return true;
    }
};

Printer::Printer(const std::string& host, int port)
    : pImpl_(new Impl(host, port)) {}

Printer::~Printer() = default;

bool Printer::connect() {
    return pImpl_->open_socket();
}

void Printer::disconnect() {
    pImpl_->close_socket();
}

bool Printer::is_connected() const {
    return pImpl_->connected;
}

// ---- B3/B5: PJL commands --------------------------------------------------

std::string send_pjl_command(int sockfd, const std::string& cmd) {
    // PJL commands end with \r\n and a UEL (\x1B%-12345X)
    std::string pjl_cmd = cmd + "\r\n\x1B%-12345X";
    ssize_t sent = ::send(sockfd, pjl_cmd.data(), pjl_cmd.size(), 0);
    if (sent != static_cast<ssize_t>(pjl_cmd.size())) return "";

    // Read response (up to 4KB)
    std::string response;
    response.reserve(4096);
    char buf[1024];
    timeval tv{3, 0};
    fd_set read_fds;
    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        int n = select(sockfd + 1, &read_fds, nullptr, nullptr, &tv);
        if (n <= 0) break;
        ssize_t r = ::recv(sockfd, buf, sizeof(buf), 0);
        if (r <= 0) break;
        response.append(buf, static_cast<size_t>(r));
        if (response.size() > 4096) break;
    }
    return response;
}

// Parse PJL INFO IDINFO response to extract cartridge info.
// Response format is multi-line, each line: "INKx=NAME BATCH:xxx LEVEL:yy%"
// Returns vector of InkCartridge with batch_id and level_pct parsed.
std::vector<InkCartridge> parse_cartridge_info(const std::string& response) {
    std::vector<InkCartridge> cartridges;
    std::istringstream iss(response);
    std::string line;
    int slot = 0;
    while (std::getline(iss, line)) {
        // Look for INK lines like: "INK1=PHOTO BLACK BATCH:A47 LEVEL:85%"
        if (line.find("INK") != std::string::npos && line.find("BATCH:") != std::string::npos) {
            InkCartridge cart;
            cart.slot = slot++;

            // Extract color name (after =, before BATCH:)
            size_t eq = line.find('=');
            size_t batch = line.find("BATCH:");
            if (eq != std::string::npos && batch != std::string::npos) {
                cart.color_name = line.substr(eq + 1, batch - eq - 2);
            }

            // Extract batch ID
            if (batch != std::string::npos) {
                size_t level = line.find("LEVEL:");
                if (level != std::string::npos) {
                    cart.batch_id = line.substr(batch + 6, level - batch - 7);
                    // Extract level percentage
                    std::string level_str = line.substr(level + 6);
                    // Remove trailing % or whitespace
                    size_t end = level_str.find('%');
                    if (end != std::string::npos) {
                        level_str = level_str.substr(0, end);
                    }
                    cart.level_pct = std::atoi(level_str.c_str());
                }
            }
            cartridges.push_back(cart);
        }
    }
    return cartridges;
}

}  // namespace epson

namespace epson {

PrinterStatus Printer::query_status() {
    PrinterStatus s;

    if (!pImpl_->connected && !pImpl_->open_socket()) {
        s.error_message = "cannot connect to " + pImpl_->host + ":" + std::to_string(pImpl_->port);
        return s;
    }

    // Send PJL ECHO to verify connectivity
    std::string echo_resp = send_pjl_command(pImpl_->sockfd, "@PJL ECHO");
    if (echo_resp.empty() || echo_resp.find("OK") == std::string::npos) {
        s.error_message = "PJL ECHO failed";
        return s;
    }

    // Query status
    std::string status_resp = send_pjl_command(pImpl_->sockfd, "@PJL INFO STATUS");
    if (!status_resp.empty()) {
        if (status_resp.find("READY") != std::string::npos) {
            s.is_ready = true;
        }
    }

    // Query media info
    std::string media_resp = send_pjl_command(pImpl_->sockfd, "@PJL INFO MEDIA");
    if (!media_resp.empty()) {
        size_t type_pos = media_resp.find("TYPE=");
        if (type_pos != std::string::npos) {
            size_t end = media_resp.find("\r\n", type_pos);
            if (end == std::string::npos) end = media_resp.size();
            s.media_type = media_resp.substr(type_pos + 5, end - type_pos - 5);
        }
    }

    if (s.error_message.empty()) {
        s.error_message = "ok";
    }
    return s;
}

std::vector<InkCartridge> Printer::read_cartridges() {
    if (!pImpl_->connected && !pImpl_->open_socket()) {
        return {};
    }
    std::string resp = send_pjl_command(pImpl_->sockfd, "@PJL INFO CARTRIDGE");
    return parse_cartridge_info(resp);
}

std::vector<InkCartridge> Printer::read_cartridges(const std::string& batch_id) {
    auto all = read_cartridges();
    if (batch_id.empty()) return all;
    std::vector<InkCartridge> filtered;
    for (const auto& c : all) {
        if (c.batch_id == batch_id) {
            filtered.push_back(c);
        }
    }
    return filtered;
}

std::vector<NozzleStatus> Printer::nozzle_check() {
    if (!pImpl_->connected && !pImpl_->open_socket()) {
        return {};
    }
    // Trigger nozzle check pattern via PJL
    // The pattern is printed and must be scanned visually or by i1Pro3
    send_pjl_command(pImpl_->sockfd, "@PJL RENDERMODEL NOZZLECHECK=ON");
    // Return empty - caller should scan the printed pattern with i1Pro3
    return {};
}

bool Printer::clean_head(CleanLevel level) {
    if (!pImpl_->connected && !pImpl_->open_socket()) {
        return false;
    }
    // Trigger head cleaning via PJL
    // EPSON supports different cleaning levels via RENDERMODEL CLEAN command
    std::string cmd = "@PJL RENDERMODEL CLEAN";
    switch (level) {
        case CleanLevel::Light:   cmd += "=LEVEL1"; break;
        case CleanLevel::Medium:  cmd += "=LEVEL2"; break;
        case CleanLevel::Heavy:   cmd += "=LEVEL3"; break;
    }
    std::string resp = send_pjl_command(pImpl_->sockfd, cmd);
    // Check for confirmation response
    return !resp.empty() && (resp.find("OK") != std::string::npos ||
                             resp.find("START") != std::string::npos);
}

bool Printer::set_black_overcoat(bool enable) {
    if (!pImpl_->connected && !pImpl_->open_socket()) {
        return false;
    }
    // Black Enhance Overcoat (BEO) / Gloss Optimizer control via PJL
    // EPSON UltraChrome PRO ink sets use @PJL RENDERMODEL GO=ON|OFF
    std::string cmd = "@PJL RENDERMODEL GO=";
    cmd += enable ? "ON" : "OFF";
    std::string resp = send_pjl_command(pImpl_->sockfd, cmd);
    return !resp.empty() && (resp.find("OK") != std::string::npos ||
                             resp.find("GO") != std::string::npos);
}

bool Printer::send_raw_data(const std::vector<uint8_t>& data) {
    if (data.empty()) return false;
    return pImpl_->send_all(data.data(), data.size());
}

}  // namespace epson
}  // namespace jcolour
