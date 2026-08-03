#include <catch2/catch_test_macros.hpp>
#include "db.h"

using namespace jcolour::calibration;

static StoredParamSet make_ps(const std::string& ink, const std::string& paper,
                               int workflow, const std::string& blob = "data",
                               int64_t cal_at = 1000, int64_t used_at = 2000) {
    StoredParamSet ps;
    ps.key.ink_batch_id = ink;
    ps.key.paper_id     = paper;
    ps.key.workflow     = workflow;
    ps.blob             = blob;
    ps.calibrated_at    = cal_at;
    ps.last_used_at     = used_at;
    return ps;
}

TEST_CASE("open in-memory database", "[db]") {
    REQUIRE_NOTHROW(ParamDb(":memory:"));
}

TEST_CASE("empty database has count 0", "[db]") {
    ParamDb db(":memory:");
    REQUIRE(db.count() == 0);
}

TEST_CASE("upsert and get round-trip", "[db]") {
    ParamDb db(":memory:");
    auto ps = make_ps("INK-A", "PAPER-1", 1, "params_blob", 1000, 2000);
    db.upsert(ps);

    StoredParamSet out;
    REQUIRE(db.get(ps.key, out) == true);
    REQUIRE(out.key.ink_batch_id == "INK-A");
    REQUIRE(out.key.paper_id     == "PAPER-1");
    REQUIRE(out.key.workflow     == 1);
    REQUIRE(out.blob             == "params_blob");
    REQUIRE(out.calibrated_at   == 1000);
    REQUIRE(out.last_used_at    == 2000);
}

TEST_CASE("get returns false for missing key", "[db]") {
    ParamDb db(":memory:");
    StoredParamSet out;
    ParamKey key{"INK-X", "PAPER-X", 1};
    REQUIRE(db.get(key, out) == false);
}

TEST_CASE("upsert updates existing row", "[db]") {
    ParamDb db(":memory:");
    auto ps = make_ps("INK-A", "PAPER-1", 1, "v1", 100, 200);
    db.upsert(ps);

    ps.blob          = "v2";
    ps.calibrated_at = 999;
    ps.last_used_at  = 888;
    db.upsert(ps);

    REQUIRE(db.count() == 1);

    StoredParamSet out;
    db.get(ps.key, out);
    REQUIRE(out.blob          == "v2");
    REQUIRE(out.calibrated_at == 999);
    REQUIRE(out.last_used_at  == 888);
}

TEST_CASE("UNIQUE constraint: same key only one row", "[db]") {
    ParamDb db(":memory:");
    db.upsert(make_ps("INK-A", "PAPER-1", 1, "first"));
    db.upsert(make_ps("INK-A", "PAPER-1", 1, "second"));
    REQUIRE(db.count() == 1);
}

TEST_CASE("different keys stored independently", "[db]") {
    ParamDb db(":memory:");
    db.upsert(make_ps("INK-A", "PAPER-1", 1));
    db.upsert(make_ps("INK-A", "PAPER-1", 2));
    db.upsert(make_ps("INK-B", "PAPER-1", 1));
    db.upsert(make_ps("INK-A", "PAPER-2", 1));
    REQUIRE(db.count() == 4);
}

TEST_CASE("remove deletes existing row", "[db]") {
    ParamDb db(":memory:");
    auto ps = make_ps("INK-A", "PAPER-1", 1);
    db.upsert(ps);
    REQUIRE(db.count() == 1);
    REQUIRE(db.remove(ps.key) == true);
    REQUIRE(db.count() == 0);
}

TEST_CASE("remove returns false for missing key", "[db]") {
    ParamDb db(":memory:");
    ParamKey key{"INK-X", "PAPER-X", 1};
    REQUIRE(db.remove(key) == false);
}

TEST_CASE("list_mru returns entries newest first", "[db]") {
    ParamDb db(":memory:");
    db.upsert(make_ps("INK-A", "PAPER-1", 1, "d", 0, 100));
    db.upsert(make_ps("INK-A", "PAPER-1", 2, "d", 0, 300));
    db.upsert(make_ps("INK-A", "PAPER-1", 3, "d", 0, 200));

    auto mru = db.list_mru(10);
    REQUIRE(mru.size() == 3);
    REQUIRE(mru[0].last_used_at == 300);
    REQUIRE(mru[1].last_used_at == 200);
    REQUIRE(mru[2].last_used_at == 100);
}

TEST_CASE("list_mru respects limit", "[db]") {
    ParamDb db(":memory:");
    for (int i = 0; i < 5; ++i)
        db.upsert(make_ps("INK-A", "PAPER-1", i + 1, "d", 0, i * 10));

    auto mru = db.list_mru(3);
    REQUIRE(mru.size() == 3);
}

TEST_CASE("list_mru on empty db returns empty", "[db]") {
    ParamDb db(":memory:");
    REQUIRE(db.list_mru().empty());
}

TEST_CASE("blob survives round-trip with special characters", "[db]") {
    ParamDb db(":memory:");
    std::string blob = "line1\nline2\ttab\x01\x02binary";
    db.upsert(make_ps("INK-A", "PAPER-1", 1, blob));
    StoredParamSet out;
    db.get({"INK-A", "PAPER-1", 1}, out);
    REQUIRE(out.blob == blob);
}
