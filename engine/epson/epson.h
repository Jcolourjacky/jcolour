#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace jcolour {
namespace epson {

// Discovered printer on the network.
struct DiscoveredPrinter {
    std::string name;      // e.g. "EPSON_P9580"
    std::string host;      // IP address
    int         port = 9100;
};

// Discover printers via mDNS/Bonjour _pdl-datastream._tcp service.
std::vector<DiscoveredPrinter> discover_printers();

struct InkCartridge {
    int         slot        = -1;
    std::string batch_id;
    int         level_pct   = -1;   // 0-100; -1 = unknown
    std::string color_name;
};

struct NozzleStatus {
    int  channel          = 0;
    bool has_clog         = false;
    int  missing_nozzles  = 0;
};

enum class CleanLevel { Light, Medium, Heavy };

struct PrinterStatus {
    bool                      is_ready      = false;
    std::string               error_message;
    std::vector<InkCartridge> cartridges;
    std::vector<NozzleStatus> nozzles;
    int                       media_width_mm = 0;
    std::string               media_type;
};

class Printer {
public:
    explicit Printer(const std::string& host, int port = 9100);
    ~Printer();

    bool connect();
    void disconnect();
    bool is_connected() const;

    PrinterStatus query_status();

    // Returns all 12 cartridges.
    std::vector<InkCartridge> read_cartridges();
    // Returns only cartridges whose batch_id matches.
    std::vector<InkCartridge> read_cartridges(const std::string& batch_id);

    // Trigger nozzle-check print; returns per-channel clog status.
    std::vector<NozzleStatus> nozzle_check();

    // Trigger head-cleaning cycle.
    bool clean_head(CleanLevel level = CleanLevel::Light);

    // Enable/disable gloss-optimizer / black overcoat layer.
    bool set_black_overcoat(bool enable);

    // Send raw ESC/P-R or PJL bytes directly to port 9100.
    bool send_raw_data(const std::vector<uint8_t>& data);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace epson
}  // namespace jcolour
