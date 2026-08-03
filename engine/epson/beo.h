#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace jcolour {
namespace epson {

// Black Enhance Overcoat (BEO) control
// P9580 / P20580 support BEO which adds a clear coating over black areas
// to enhance depth and reduce bronzing

// BEO mode options
enum class BEOMode {
    OFF,              // No BEO
    STANDARD,         // Standard BEO over black areas
    ENHANCED          // Maximum BEO for deepest blacks
};

// BEO configuration for a print job
struct BEOConfig {
    BEOMode mode = BEOMode::OFF;
    float lgy_reserve = 0.0f;  // Amount of LGY ink to reserve (0.0-0.2)
    float beo_density = 1.0f;   // BEO density multiplier (0.5-1.0)
    bool auto_trigger = false;   // Auto-trigger based on ink coverage
};

// BEO detection result
struct BEOCapability {
    bool supported = false;
    std::string firmware_version;
    std::vector<BEOMode> supported_modes;
    float max_lgy_reserve = 0.0f;
};

// Query printer for BEO capabilities
BEOCapability query_beo_capability(const std::string& printer_host);

// Set BEO configuration for subsequent print jobs
// Returns true if configuration was accepted
bool set_beo_config(const std::string& printer_host, const BEOConfig& config);

// Read current BEO configuration from printer
BEOConfig get_beo_config(const std::string& printer_host);

// BEO status query (check if BEO system is functioning)
struct BEOStatus {
    bool active = false;
    float lgy_usage_percent = 0.0f;
    std::string error_message;
};

BEOStatus query_beo_status(const std::string& printer_host);

// Known BEO PJL commands (experimental - verify with actual printer)
// These are based on common ESC/P-R patterns and may need adjustment
namespace beo_commands {
    // BEO mode selection
    inline const char* MODE_OFF = "@PJL SET BEO=OFF\r\n";
    inline const char* MODE_STANDARD = "@PJL SET BEO=ON\r\n";
    inline const char* MODE_ENHANCED = "@PJL SET BEO=ENHANCED\r\n";

    // LGY reserve control
    inline const char* RESERVE_BASE = "@PJL SET LGYRESERVE=";  // Append percentage

    // BEO density
    inline const char* DENSITY_BASE = "@PJL SET BEODENSITY=";  // Append 50-100

    // Query BEO status
    inline const char* QUERY = "@PJL INFO BEOSTATUS\r\n";
}

// Utility: convert BEOMode to PJL string
std::string beo_mode_to_pjl(BEOMode mode);

// Utility: build complete BEO PJL command string
std::string build_beo_pjl_command(const BEOConfig& config);

}  // namespace epson
}  // namespace jcolour