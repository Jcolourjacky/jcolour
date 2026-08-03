#include "beo.h"
#include <sstream>
#include <iomanip>

namespace jcolour {
namespace epson {

BEOCapability query_beo_capability(const std::string& printer_host) {
    (void)printer_host;

    BEOCapability cap;
    cap.supported = true;  // P9580 supports BEO
    cap.supported_modes = {BEOMode::OFF, BEOMode::STANDARD, BEOMode::ENHANCED};
    cap.max_lgy_reserve = 0.20f;  // Up to 20% LGY reserve
    return cap;
}

bool set_beo_config(const std::string& printer_host, const BEOConfig& config) {
    (void)printer_host;
    (void)config;

    // TODO: Send PJL commands to configure BEO
    // For now, just validate the configuration
    if (config.lgy_reserve < 0.0f || config.lgy_reserve > 0.2f) {
        return false;
    }
    if (config.beo_density < 0.5f || config.beo_density > 1.0f) {
        return false;
    }
    return true;
}

BEOConfig get_beo_config(const std::string& printer_host) {
    (void)printer_host;

    BEOConfig config;
    config.mode = BEOMode::OFF;
    config.lgy_reserve = 0.0f;
    config.beo_density = 1.0f;
    config.auto_trigger = false;
    return config;
}

BEOStatus query_beo_status(const std::string& printer_host) {
    (void)printer_host;

    BEOStatus status;
    status.active = false;
    status.lgy_usage_percent = 0.0f;
    status.error_message = "BEO status query not yet implemented";
    return status;
}

std::string beo_mode_to_pjl(BEOMode mode) {
    switch (mode) {
        case BEOMode::OFF:
            return beo_commands::MODE_OFF;
        case BEOMode::STANDARD:
            return beo_commands::MODE_STANDARD;
        case BEOMode::ENHANCED:
            return beo_commands::MODE_ENHANCED;
        default:
            return beo_commands::MODE_OFF;
    }
}

std::string build_beo_pjl_command(const BEOConfig& config) {
    std::ostringstream cmd;

    // BEO mode
    cmd << beo_mode_to_pjl(config.mode);

    // LGY reserve
    if (config.lgy_reserve > 0.0f) {
        int reserve_pct = static_cast<int>(config.lgy_reserve * 100.0f);
        cmd << beo_commands::RESERVE_BASE << reserve_pct << "\r\n";
    }

    // BEO density
    if (config.beo_density != 1.0f) {
        int density_pct = static_cast<int>(config.beo_density * 100.0f);
        cmd << beo_commands::DENSITY_BASE << density_pct << "\r\n";
    }

    return cmd.str();
}

}  // namespace epson
}  // namespace jcolour
