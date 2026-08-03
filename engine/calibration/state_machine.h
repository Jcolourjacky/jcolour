#pragma once

#include <cstdint>
#include <string>

namespace jcolour {
namespace calibration {

using Timestamp = int64_t;  // Unix seconds

// --------------------------------------------------------------------------
// Calibration freshness — answers "is this stored calibration still valid?"
// based on age, ink batch identity, and paper identity. Independent of the
// operational state machine below.
// --------------------------------------------------------------------------

enum class CalibState {
    UNCALIBRATED,
    FRESH,
    STALE_SOFT,
    STALE_HARD,
};

struct ParamSetKey {
    std::string ink_batch_id;
    std::string paper_id;
};

struct ParamSet {
    CalibState  state        = CalibState::UNCALIBRATED;
    ParamSetKey key;
    Timestamp   calibrated_at = 0;
};

struct InkBatch {
    std::string id;
};

struct Paper {
    std::string id;
};

CalibState compute_state(const ParamSet& ps, Timestamp now,
                         const InkBatch& current_ink,
                         const Paper& current_paper);

// --------------------------------------------------------------------------
// Print condition state machine — operational lifecycle of a {paper × ink
// batch} calibration session. Audit F20: spec (ok/WORK-PACKAGES.md P01)
// requires 9 states + a transition table; previously only 4 were modelled.
//
// States:
//   UNCONFIGURED   freshly created session, nothing done yet
//   HARDWARE_OK    nozzle check passed
//   CALIBRATED     ink_limit + g7_curve + linearization ready
//   PROFILED       printer.icc + (optionally) cynsn_model ready
//   READY          active.json bound, will accept jobs
//   NEEDS_RECAL    AutoCal AMBER (1.5 ≤ ΔE < 3.0) — soft recalibration suggested
//   DEGRADED       §15 light recalibration failed or profile checksum bad
//   BATCH_CHANGED  ink_batch_id changed since last successful READY
//   ON_HOLD        AutoCal RED (ΔE ≥ 3.0) or operator pause
// --------------------------------------------------------------------------

enum class PrintConditionState {
    UNCONFIGURED,
    HARDWARE_OK,
    CALIBRATED,
    PROFILED,
    READY,
    NEEDS_RECAL,
    DEGRADED,
    BATCH_CHANGED,
    ON_HOLD,
};

struct PrintCondition {
    PrintConditionState state = PrintConditionState::UNCONFIGURED;
    std::string id;            // UUID
    std::string paper_id;
    std::string ink_batch_id;
    Timestamp   last_cal_at = 0;
};

// Returns true if the given state→state transition is legal. The full
// transition table is documented in ok/WORK-PACKAGES.md P01:
//
//   UNCONFIGURED   → HARDWARE_OK
//   HARDWARE_OK    → CALIBRATED
//   CALIBRATED     → PROFILED
//   PROFILED       → READY
//   READY          → NEEDS_RECAL          (AutoCal AMBER)
//   READY          → ON_HOLD              (AutoCal RED / operator pause)
//   READY          → BATCH_CHANGED        (ink_batch_id rolled)
//   NEEDS_RECAL    → READY                (light recalibration succeeded)
//   NEEDS_RECAL    → DEGRADED             (light recalibration failed)
//   ON_HOLD        → NEEDS_RECAL          (operator releases hold for recal)
//   ON_HOLD        → READY                (operator releases hold directly)
//   BATCH_CHANGED  → HARDWARE_OK          (start over from scratch)
//   BATCH_CHANGED  → NEEDS_RECAL          (risk-accepted shortcut)
//   DEGRADED       → HARDWARE_OK          (operator decides to rebuild)
//
// All other transitions return false. Self-transitions return false too
// (no-op writes should never be required; if they are, the caller has a
// bug).
bool is_legal_transition(PrintConditionState from, PrintConditionState to);

// Apply a transition. Returns false (and leaves pc unchanged) if the
// transition is not legal under is_legal_transition().
bool transition(PrintCondition& pc, PrintConditionState to);

// Human-readable state name. Useful for logging, error messages, UI.
const char* state_name(PrintConditionState s);

}  // namespace calibration
}  // namespace jcolour
