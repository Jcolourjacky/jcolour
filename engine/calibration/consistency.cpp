#include "consistency.h"

namespace jcolour {
namespace calibration {

LoadResult select_param_set(const std::vector<StoredParamSet>& candidates,
                            const PrintContext& ctx) {
    if (candidates.empty()) {
        return {LoadResultKind::UNCALIBRATED, {}, {}};
    }

    const auto& top = candidates[0];
    bool top_matches = (top.key.ink_batch_id == ctx.ink_batch_id &&
                        top.key.paper_id     == ctx.paper_id);

    if (top_matches) {
        return {LoadResultKind::MRU_MATCH, top, candidates};
    }

    // Search remaining candidates for an exact match.
    for (std::size_t i = 1; i < candidates.size(); ++i) {
        const auto& c = candidates[i];
        if (c.key.ink_batch_id == ctx.ink_batch_id &&
            c.key.paper_id     == ctx.paper_id) {
            return {LoadResultKind::EXACT_MATCH, c, candidates};
        }
    }

    return {LoadResultKind::NO_MATCH, {}, candidates};
}

LoadResult load_default_for_workflow(ParamDb& db, int workflow,
                                     const PrintContext& ctx) {
    // Fetch all entries for this workflow, MRU order.
    auto all = db.list_mru(100);
    std::vector<StoredParamSet> candidates;
    for (auto& ps : all) {
        if (ps.key.workflow == workflow)
            candidates.push_back(std::move(ps));
    }
    return select_param_set(candidates, ctx);
}

}  // namespace calibration
}  // namespace jcolour
