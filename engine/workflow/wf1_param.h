#pragma once

#include <array>
#include <string>
#include <cstdint>
#include "../src/channel_layout.h"

namespace jcolour {
namespace workflow {

// BlackKind canonical definition lives in channel_layout.h. Re-export it
// into the workflow namespace so existing wf*_param call sites that wrote
// "BlackKind" unqualified continue to compile.
using BlackKind = jcolour::BlackKind;

// Calibration data for workflow 1 (single black)
struct Workflow1Data {
    BlackKind black_kind = BlackKind::PK;

    // L* (0-255) → ink percentage (0.0-1.0)
    // Index i corresponds to L* = i * 100.0 / 255.0
    std::array<float, 256> lstar_to_ink;

    // Ink limit determined during calibration
    float ink_limit = 0.0f;

    // Calibration metadata
    std::string paper_id;
    std::string ink_batch_id;
    int64_t calibrated_at = 0;  // Unix timestamp
};

// Serialize Workflow1Data to JSON string for storage
std::string serialize_workflow1(const Workflow1Data& data);

// Deserialize Workflow1Data from JSON string
Workflow1Data deserialize_workflow1(const std::string& json);

// Create Workflow1Data from LUT and metadata
Workflow1Data create_workflow1_data(
    BlackKind black_kind,
    const std::array<float, 256>& lut,
    float ink_limit,
    const std::string& paper_id,
    const std::string& ink_batch_id
);

// Validate Workflow1Data completeness
struct Workflow1Validation {
    bool valid = false;
    std::string error_message;
    bool has_lut = false;
    bool has_metadata = false;
    bool lut_is_monotonic = false;
};

Workflow1Validation validate_workflow1_data(const Workflow1Data& data);

}  // namespace workflow
}  // namespace jcolour
