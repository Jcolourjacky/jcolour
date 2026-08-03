#include "wf1_param.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>

namespace jcolour {
namespace workflow {

// Simple JSON serialization (no external dependency)
std::string serialize_workflow1(const Workflow1Data& data) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"black_kind\":" << (data.black_kind == BlackKind::PK ? "\"PK\"" : "\"MK\"") << ",";
    oss << "\"ink_limit\":" << std::fixed << std::setprecision(4) << data.ink_limit << ",";
    oss << "\"paper_id\":\"" << data.paper_id << "\",";
    oss << "\"ink_batch_id\":\"" << data.ink_batch_id << "\",";
    oss << "\"calibrated_at\":" << data.calibrated_at << ",";
    oss << "\"lstar_to_ink\":[";
    for (int i = 0; i < 256; ++i) {
        if (i > 0) oss << ",";
        oss << std::fixed << std::setprecision(6) << data.lstar_to_ink[i];
    }
    oss << "]";
    oss << "}";
    return oss.str();
}

// Simple JSON parsing (no external dependency)
Workflow1Data deserialize_workflow1(const std::string& json) {
    Workflow1Data data;

    // Parse black_kind
    if (json.find("\"PK\"") != std::string::npos) {
        data.black_kind = BlackKind::PK;
    } else {
        data.black_kind = BlackKind::MK;
    }

    // Parse ink_limit
    auto ink_pos = json.find("\"ink_limit\"");
    if (ink_pos != std::string::npos) {
        auto colon_pos = json.find(":", ink_pos);
        auto comma_pos = json.find(",", colon_pos);
        std::string val = json.substr(colon_pos + 1, comma_pos - colon_pos - 1);
        data.ink_limit = std::stod(val);
    }

    // Parse paper_id
    auto paper_pos = json.find("\"paper_id\"");
    if (paper_pos != std::string::npos) {
        auto colon_pos = json.find(":", paper_pos);
        auto start_quote = json.find("\"", colon_pos + 1);
        auto end_quote = json.find("\"", start_quote + 1);
        data.paper_id = json.substr(start_quote + 1, end_quote - start_quote - 1);
    }

    // Parse ink_batch_id
    auto batch_pos = json.find("\"ink_batch_id\"");
    if (batch_pos != std::string::npos) {
        auto colon_pos = json.find(":", batch_pos);
        auto start_quote = json.find("\"", colon_pos + 1);
        auto end_quote = json.find("\"", start_quote + 1);
        data.ink_batch_id = json.substr(start_quote + 1, end_quote - start_quote - 1);
    }

    // Parse calibrated_at
    auto cal_pos = json.find("\"calibrated_at\"");
    if (cal_pos != std::string::npos) {
        auto colon_pos = json.find(":", cal_pos);
        auto comma_pos = json.find(",", colon_pos);
        std::string val = json.substr(colon_pos + 1, comma_pos - colon_pos - 1);
        data.calibrated_at = std::stoll(val);
    }

    // Parse lstar_to_ink array
    auto arr_pos = json.find("\"lstar_to_ink\"");
    if (arr_pos != std::string::npos) {
        auto bracket_start = json.find("[", arr_pos);
        auto bracket_end = json.find("]", bracket_start);
        std::string arr_str = json.substr(bracket_start + 1, bracket_end - bracket_start - 1);

        std::replace(arr_str.begin(), arr_str.end(), '[', ' ');
        std::replace(arr_str.begin(), arr_str.end(), ']', ' ');

        std::istringstream iss(arr_str);
        std::string val;
        int idx = 0;
        while (std::getline(iss, val, ',') && idx < 256) {
            val.erase(std::remove_if(val.begin(), val.end(), ::isspace), val.end());
            if (!val.empty()) {
                data.lstar_to_ink[idx++] = std::stod(val);
            }
        }
    }

    return data;
}

Workflow1Data create_workflow1_data(
    BlackKind black_kind,
    const std::array<float, 256>& lut,
    float ink_limit,
    const std::string& paper_id,
    const std::string& ink_batch_id) {

    Workflow1Data data;
    data.black_kind = black_kind;
    data.lstar_to_ink = lut;
    data.ink_limit = ink_limit;
    data.paper_id = paper_id;
    data.ink_batch_id = ink_batch_id;
    data.calibrated_at = std::time(nullptr);
    return data;
}

// Check if LUT is monotonic (ink increases as L* decreases)
bool is_lut_monotonic(const std::array<float, 256>& lut) {
    // For L*=0 (idx 0), we expect high ink (dark)
    // For L*=100 (idx 255), we expect low/zero ink (light)
    // So as index increases, ink should decrease

    bool increasing = true;
    bool decreasing = true;

    for (int i = 1; i < 256; ++i) {
        if (lut[i] < lut[i-1] - 0.001f) {
            decreasing = false;  // Not consistently going down
        }
        if (lut[i] > lut[i-1] + 0.001f) {
            increasing = false;  // Not consistently going up
        }
    }

    return increasing || decreasing;
}

Workflow1Validation validate_workflow1_data(const Workflow1Data& data) {
    Workflow1Validation result;

    // Check LUT
    result.has_lut = true;
    for (int i = 0; i < 256; ++i) {
        if (data.lstar_to_ink[i] < 0.0f || data.lstar_to_ink[i] > 1.0f) {
            result.valid = false;
            result.error_message = "LUT values must be in [0, 1] range";
            return result;
        }
    }

    // Check metadata
    result.has_metadata = !data.paper_id.empty() && !data.ink_batch_id.empty();

    // Check monotonicity
    result.lut_is_monotonic = is_lut_monotonic(data.lstar_to_ink);

    result.valid = result.has_lut && result.has_metadata && result.lut_is_monotonic;

    if (!result.has_metadata) {
        result.error_message = "Missing paper_id or ink_batch_id";
    } else if (!result.lut_is_monotonic) {
        result.error_message = "LUT is not monotonic";
    }

    return result;
}

}  // namespace workflow
}  // namespace jcolour