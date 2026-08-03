#include <catch2/catch_test_macros.hpp>
#include "consistency.h"

using namespace jcolour::calibration;

static StoredParamSet make_candidate(const std::string& ink, const std::string& paper,
                                     int workflow, int64_t used_at) {
    StoredParamSet ps;
    ps.key.ink_batch_id = ink;
    ps.key.paper_id     = paper;
    ps.key.workflow     = workflow;
    ps.last_used_at     = used_at;
    return ps;
}

// ---------------------------------------------------------------------------
// select_param_set tests (pure logic, no DB)
// ---------------------------------------------------------------------------

TEST_CASE("empty candidates -> UNCALIBRATED", "[consistency]") {
    PrintContext ctx{"INK-A", "PAPER-1", "SN-001"};
    auto r = select_param_set({}, ctx);
    REQUIRE(r.kind == LoadResultKind::UNCALIBRATED);
}

TEST_CASE("top MRU matches context -> MRU_MATCH", "[consistency]") {
    std::vector<StoredParamSet> cands = {
        make_candidate("INK-A", "PAPER-1", 1, 300),
        make_candidate("INK-B", "PAPER-2", 1, 200),
    };
    PrintContext ctx{"INK-A", "PAPER-1", "SN-001"};
    auto r = select_param_set(cands, ctx);
    REQUIRE(r.kind == LoadResultKind::MRU_MATCH);
    REQUIRE(r.param_set.key.ink_batch_id == "INK-A");
    REQUIRE(r.param_set.key.paper_id     == "PAPER-1");
}

TEST_CASE("top MRU ink mismatch, second matches -> EXACT_MATCH", "[consistency]") {
    std::vector<StoredParamSet> cands = {
        make_candidate("INK-B", "PAPER-1", 1, 300),
        make_candidate("INK-A", "PAPER-1", 1, 200),
    };
    PrintContext ctx{"INK-A", "PAPER-1", "SN-001"};
    auto r = select_param_set(cands, ctx);
    REQUIRE(r.kind == LoadResultKind::EXACT_MATCH);
    REQUIRE(r.param_set.key.ink_batch_id == "INK-A");
}

TEST_CASE("top MRU paper mismatch, second matches -> EXACT_MATCH", "[consistency]") {
    std::vector<StoredParamSet> cands = {
        make_candidate("INK-A", "PAPER-2", 1, 300),
        make_candidate("INK-A", "PAPER-1", 1, 200),
    };
    PrintContext ctx{"INK-A", "PAPER-1", "SN-001"};
    auto r = select_param_set(cands, ctx);
    REQUIRE(r.kind == LoadResultKind::EXACT_MATCH);
    REQUIRE(r.param_set.key.paper_id == "PAPER-1");
}

TEST_CASE("no candidate matches context -> NO_MATCH", "[consistency]") {
    std::vector<StoredParamSet> cands = {
        make_candidate("INK-B", "PAPER-2", 1, 300),
        make_candidate("INK-C", "PAPER-3", 1, 200),
    };
    PrintContext ctx{"INK-A", "PAPER-1", "SN-001"};
    auto r = select_param_set(cands, ctx);
    REQUIRE(r.kind == LoadResultKind::NO_MATCH);
    REQUIRE(r.candidates.size() == 2);
}

TEST_CASE("single candidate matches -> MRU_MATCH", "[consistency]") {
    std::vector<StoredParamSet> cands = {
        make_candidate("INK-A", "PAPER-1", 1, 100),
    };
    PrintContext ctx{"INK-A", "PAPER-1", "SN-001"};
    auto r = select_param_set(cands, ctx);
    REQUIRE(r.kind == LoadResultKind::MRU_MATCH);
}

TEST_CASE("single candidate does not match -> NO_MATCH", "[consistency]") {
    std::vector<StoredParamSet> cands = {
        make_candidate("INK-B", "PAPER-1", 1, 100),
    };
    PrintContext ctx{"INK-A", "PAPER-1", "SN-001"};
    auto r = select_param_set(cands, ctx);
    REQUIRE(r.kind == LoadResultKind::NO_MATCH);
}

TEST_CASE("candidates list is returned in NO_MATCH for picker UI", "[consistency]") {
    std::vector<StoredParamSet> cands = {
        make_candidate("INK-B", "PAPER-2", 1, 300),
    };
    PrintContext ctx{"INK-A", "PAPER-1", "SN-001"};
    auto r = select_param_set(cands, ctx);
    REQUIRE(r.kind == LoadResultKind::NO_MATCH);
    REQUIRE(!r.candidates.empty());
}

// ---------------------------------------------------------------------------
// load_default_for_workflow tests (with in-memory DB)
// ---------------------------------------------------------------------------

static void insert(ParamDb& db, const std::string& ink, const std::string& paper,
                   int workflow, int64_t used_at) {
    StoredParamSet ps;
    ps.key.ink_batch_id = ink;
    ps.key.paper_id     = paper;
    ps.key.workflow     = workflow;
    ps.last_used_at     = used_at;
    ps.calibrated_at    = used_at;
    db.upsert(ps);
}

TEST_CASE("load_default: no entries -> UNCALIBRATED", "[consistency]") {
    ParamDb db(":memory:");
    PrintContext ctx{"INK-A", "PAPER-1", "SN-001"};
    auto r = load_default_for_workflow(db, 1, ctx);
    REQUIRE(r.kind == LoadResultKind::UNCALIBRATED);
}

TEST_CASE("load_default: MRU matches -> MRU_MATCH", "[consistency]") {
    ParamDb db(":memory:");
    insert(db, "INK-A", "PAPER-1", 1, 300);
    insert(db, "INK-B", "PAPER-2", 1, 200);
    PrintContext ctx{"INK-A", "PAPER-1", "SN-001"};
    auto r = load_default_for_workflow(db, 1, ctx);
    REQUIRE(r.kind == LoadResultKind::MRU_MATCH);
}

TEST_CASE("load_default: different workflow filtered out", "[consistency]") {
    ParamDb db(":memory:");
    insert(db, "INK-A", "PAPER-1", 2, 300);  // workflow 2, not 1
    PrintContext ctx{"INK-A", "PAPER-1", "SN-001"};
    auto r = load_default_for_workflow(db, 1, ctx);
    REQUIRE(r.kind == LoadResultKind::UNCALIBRATED);
}

TEST_CASE("load_default: ink batch changed -> EXACT_MATCH from history", "[consistency]") {
    ParamDb db(":memory:");
    insert(db, "INK-B", "PAPER-1", 1, 300);  // most recent, wrong ink
    insert(db, "INK-A", "PAPER-1", 1, 200);  // older, correct ink
    PrintContext ctx{"INK-A", "PAPER-1", "SN-001"};
    auto r = load_default_for_workflow(db, 1, ctx);
    REQUIRE(r.kind == LoadResultKind::EXACT_MATCH);
    REQUIRE(r.param_set.key.ink_batch_id == "INK-A");
}
