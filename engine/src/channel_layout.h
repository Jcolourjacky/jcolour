#pragma once

// channel_layout — single source of truth for jcolour's ink-channel model.
//
// Epson SureColor PRO12 (P7580 / P9580 / P20580 and the global P7570 / P9570 /
// P20570) markets "12 colors", but at print time only **11 are active**: Photo
// Black and Matte Black are loaded simultaneously into the printhead but are
// mutually exclusive within a single job — the firmware activates exactly one
// of them based on the loaded paper. The unused black slot prints zero ink.
//
// Internally jcolour models the 11 active channels (ActiveInks) plus a
// BlackKind tag. The 12-channel physical layout (PhysicalInks) is materialised
// only at the PJL boundary via expand_to_physical(). Doing so prevents the
// entire class of bugs where code accidentally writes both PK and MK on the
// same pixel — those bugs cannot type-check.
//
// See engine/workflow/README.md for the full canonical table.

#include <array>
#include <cstdint>

namespace jcolour {

// Which black is currently loaded as the active "main K" for this print job.
// Selected by paper type at session start; never changes mid-job.
enum class BlackKind : uint8_t {
    PK = 0,  // Photo Black: glossy / satin / baryta / photo papers
    MK = 1   // Matte Black: matte / cotton / fine-art / canvas papers
};

// jcolour's internal 11-channel active model. The MAIN_K slot at index 0 maps
// to whichever physical black is active for this job. Index order is fixed and
// must match engine/workflow/README.md.
enum class ActiveChannel : int {
    MAIN_K = 0,   // The active black (PK or MK, per BlackKind)
    LC     = 1,   // Light Cyan
    C      = 2,   // Cyan
    VLM    = 3,   // Vivid Light Magenta
    VM     = 4,   // Vivid Magenta
    Y      = 5,   // Yellow
    Or     = 6,   // Orange
    GY     = 7,   // Gray
    LGY    = 8,   // Light Gray
    V      = 9,   // Violet
    Gr     = 10   // Green
};

constexpr int NUM_ACTIVE_CHANNELS = 11;
constexpr int NUM_PHYSICAL_CHANNELS = 12;

// Physical PJL channel order (PRO12, all 12 slots; one of PK/MK is always 0):
//   0:PK 1:MK 2:LC 3:C 4:VLM 5:VM 6:Y 7:Or 8:GY 9:LGY 10:V 11:Gr
// Active-channel indices above map by inserting MAIN_K into slot 0 (PK) or
// slot 1 (MK) and shifting the rest into the remaining slots in the same
// internal order.
constexpr int PHYSICAL_PK_INDEX = 0;
constexpr int PHYSICAL_MK_INDEX = 1;

// Strong-typed aliases. ActiveInks and PhysicalInks are intentionally
// distinct sizes so that mixing them is a compile error, not a runtime bug.
using ActiveInks   = std::array<float, NUM_ACTIVE_CHANNELS>;
using PhysicalInks = std::array<float, NUM_PHYSICAL_CHANNELS>;

// Convenience accessor: ActiveChannel → int index.
constexpr int idx(ActiveChannel ch) { return static_cast<int>(ch); }

// Expand the 11-channel active representation into the 12-slot PJL layout
// that the printer hardware expects.
//
// - active[0] (MAIN_K) lands in slot 0 (PK) when kind == PK, slot 1 (MK)
//   when kind == MK. The unused black slot stays at 0.
// - active[1..10] map to physical slots 2..11 in the same order
//   (LC, C, VLM, VM, Y, Or, GY, LGY, V, Gr).
inline PhysicalInks expand_to_physical(const ActiveInks& active,
                                       BlackKind kind) {
    PhysicalInks out{};
    out[kind == BlackKind::PK ? PHYSICAL_PK_INDEX : PHYSICAL_MK_INDEX] =
        active[idx(ActiveChannel::MAIN_K)];
    out[2]  = active[idx(ActiveChannel::LC)];
    out[3]  = active[idx(ActiveChannel::C)];
    out[4]  = active[idx(ActiveChannel::VLM)];
    out[5]  = active[idx(ActiveChannel::VM)];
    out[6]  = active[idx(ActiveChannel::Y)];
    out[7]  = active[idx(ActiveChannel::Or)];
    out[8]  = active[idx(ActiveChannel::GY)];
    out[9]  = active[idx(ActiveChannel::LGY)];
    out[10] = active[idx(ActiveChannel::V)];
    out[11] = active[idx(ActiveChannel::Gr)];
    return out;
}

// Inverse of expand_to_physical. Reads MAIN_K from the slot indicated by
// kind; any ink present in the other black slot is silently dropped — the
// caller is asserting that the physical layout obeys the PK XOR MK rule.
//
// In strict callers that want to catch bad inputs, check that
//   physical[PHYSICAL_PK_INDEX] == 0 || physical[PHYSICAL_MK_INDEX] == 0
// before invoking this.
inline ActiveInks collapse_from_physical(const PhysicalInks& physical,
                                         BlackKind kind) {
    ActiveInks out{};
    out[idx(ActiveChannel::MAIN_K)] =
        physical[kind == BlackKind::PK ? PHYSICAL_PK_INDEX
                                       : PHYSICAL_MK_INDEX];
    out[idx(ActiveChannel::LC)]  = physical[2];
    out[idx(ActiveChannel::C)]   = physical[3];
    out[idx(ActiveChannel::VLM)] = physical[4];
    out[idx(ActiveChannel::VM)]  = physical[5];
    out[idx(ActiveChannel::Y)]   = physical[6];
    out[idx(ActiveChannel::Or)]  = physical[7];
    out[idx(ActiveChannel::GY)]  = physical[8];
    out[idx(ActiveChannel::LGY)] = physical[9];
    out[idx(ActiveChannel::V)]   = physical[10];
    out[idx(ActiveChannel::Gr)]  = physical[11];
    return out;
}

// Returns true if a PhysicalInks layout respects the PK XOR MK rule
// (at most one of the two black slots is non-zero). Use as an assertion
// or sanity check before sending to the printer.
inline bool obeys_black_exclusivity(const PhysicalInks& physical,
                                    float epsilon = 1e-6f) {
    return physical[PHYSICAL_PK_INDEX] <= epsilon ||
           physical[PHYSICAL_MK_INDEX] <= epsilon;
}

}  // namespace jcolour
