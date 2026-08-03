#pragma once

#include "db.h"
#include <string>
#include <vector>

namespace jcolour {
namespace calibration {

// Physical context at the time of a pre-print check.
struct PrintContext {
    std::string ink_batch_id;
    std::string paper_id;
    std::string printer_serial;
};

// Result of load_default_for_workflow.
enum class LoadResultKind {
    MRU_MATCH,       // top MRU entry matches current context exactly
    EXACT_MATCH,     // found a non-top entry that matches exactly
    NO_MATCH,        // no stored entry matches; caller must prompt user
    UNCALIBRATED,    // no entries exist for this workflow
};

struct LoadResult {
    LoadResultKind      kind;
    StoredParamSet      param_set;   // valid when kind != UNCALIBRATED/NO_MATCH
    std::vector<StoredParamSet> candidates;  // all candidates (for picker UI)
};

// Pure matching logic extracted from §6.3.
// `candidates` must already be sorted by last_used_at DESC (as returned by
// ParamDb::list_mru).  `ctx` is the current physical context.
LoadResult select_param_set(const std::vector<StoredParamSet>& candidates,
                            const PrintContext& ctx);

// Convenience wrapper: queries `db` for the given workflow, then calls
// select_param_set.
LoadResult load_default_for_workflow(ParamDb& db, int workflow,
                                     const PrintContext& ctx);

}  // namespace calibration
}  // namespace jcolour
