#include "state_machine.h"

namespace jcolour {
namespace calibration {

CalibState compute_state(const ParamSet& ps, Timestamp now,
                         const InkBatch& current_ink,
                         const Paper& current_paper) {
    if (ps.state == CalibState::UNCALIBRATED)
        return CalibState::UNCALIBRATED;
    if (ps.key.ink_batch_id != current_ink.id)
        return CalibState::STALE_HARD;
    if (ps.key.paper_id != current_paper.id)
        return CalibState::STALE_HARD;
    auto days = (now - ps.calibrated_at) / 86400;
    if (days > 90) return CalibState::STALE_HARD;
    if (days > 30) return CalibState::STALE_SOFT;
    return CalibState::FRESH;
}

bool is_legal_transition(PrintConditionState from, PrintConditionState to) {
    using S = PrintConditionState;
    if (from == to) return false;  // self-transitions disallowed
    switch (from) {
        case S::UNCONFIGURED:
            return to == S::HARDWARE_OK;
        case S::HARDWARE_OK:
            return to == S::CALIBRATED;
        case S::CALIBRATED:
            return to == S::PROFILED;
        case S::PROFILED:
            return to == S::READY;
        case S::READY:
            return to == S::NEEDS_RECAL ||
                   to == S::ON_HOLD     ||
                   to == S::BATCH_CHANGED;
        case S::NEEDS_RECAL:
            return to == S::READY ||
                   to == S::DEGRADED;
        case S::ON_HOLD:
            return to == S::NEEDS_RECAL ||
                   to == S::READY;
        case S::BATCH_CHANGED:
            return to == S::HARDWARE_OK ||
                   to == S::NEEDS_RECAL;
        case S::DEGRADED:
            return to == S::HARDWARE_OK;
    }
    return false;
}

bool transition(PrintCondition& pc, PrintConditionState to) {
    if (!is_legal_transition(pc.state, to)) return false;
    pc.state = to;
    return true;
}

const char* state_name(PrintConditionState s) {
    switch (s) {
        case PrintConditionState::UNCONFIGURED:  return "UNCONFIGURED";
        case PrintConditionState::HARDWARE_OK:   return "HARDWARE_OK";
        case PrintConditionState::CALIBRATED:    return "CALIBRATED";
        case PrintConditionState::PROFILED:      return "PROFILED";
        case PrintConditionState::READY:         return "READY";
        case PrintConditionState::NEEDS_RECAL:   return "NEEDS_RECAL";
        case PrintConditionState::DEGRADED:      return "DEGRADED";
        case PrintConditionState::BATCH_CHANGED: return "BATCH_CHANGED";
        case PrintConditionState::ON_HOLD:       return "ON_HOLD";
    }
    return "UNKNOWN";
}

}  // namespace calibration
}  // namespace jcolour
