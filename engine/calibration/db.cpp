#include "db.h"
#include <sqlite3.h>
#include <cstring>

namespace jcolour {
namespace calibration {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void check(int rc, sqlite3* db, const char* ctx) {
    if (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE) {
        std::string msg = ctx;
        msg += ": ";
        msg += sqlite3_errmsg(db);
        throw DbError(msg);
    }
}

// RAII wrapper for sqlite3_stmt.
struct Stmt {
    sqlite3_stmt* s = nullptr;
    ~Stmt() { if (s) sqlite3_finalize(s); }
    operator sqlite3_stmt*() { return s; }
};

static Stmt prepare(sqlite3* db, const char* sql) {
    Stmt st;
    int rc = sqlite3_prepare_v2(db, sql, -1, &st.s, nullptr);
    check(rc, db, sql);
    return st;
}

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct ParamDb::Impl {
    sqlite3* db = nullptr;
};

// ---------------------------------------------------------------------------
// Schema (embedded)
// ---------------------------------------------------------------------------

static const char* SCHEMA = R"sql(
CREATE TABLE IF NOT EXISTS param_sets (
    ink_batch_id  TEXT    NOT NULL,
    paper_id      TEXT    NOT NULL,
    workflow      INTEGER NOT NULL,
    blob          TEXT    NOT NULL DEFAULT '',
    calibrated_at INTEGER NOT NULL DEFAULT 0,
    last_used_at  INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (ink_batch_id, paper_id, workflow)
);
CREATE INDEX IF NOT EXISTS idx_mru ON param_sets (last_used_at DESC);
)sql";

// ---------------------------------------------------------------------------
// ParamDb
// ---------------------------------------------------------------------------

ParamDb::ParamDb(const std::string& path) : impl_(new Impl) {
    int rc = sqlite3_open(path.c_str(), &impl_->db);
    if (rc != SQLITE_OK) {
        std::string msg = "sqlite3_open: ";
        msg += sqlite3_errmsg(impl_->db);
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
        throw DbError(msg);
    }
    sqlite3_exec(impl_->db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    apply_schema();
}

ParamDb::~ParamDb() {
    if (impl_) {
        if (impl_->db) sqlite3_close(impl_->db);
        delete impl_;
    }
}

void ParamDb::apply_schema() {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(impl_->db, SCHEMA, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = "schema: ";
        msg += errmsg;
        sqlite3_free(errmsg);
        throw DbError(msg);
    }
}

int64_t ParamDb::upsert(const StoredParamSet& ps) {
    static const char* SQL =
        "INSERT INTO param_sets "
        "  (ink_batch_id, paper_id, workflow, blob, calibrated_at, last_used_at) "
        "VALUES (?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(ink_batch_id, paper_id, workflow) DO UPDATE SET "
        "  blob          = excluded.blob, "
        "  calibrated_at = excluded.calibrated_at, "
        "  last_used_at  = excluded.last_used_at;";

    Stmt st = prepare(impl_->db, SQL);
    sqlite3_bind_text(st, 1, ps.key.ink_batch_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, ps.key.paper_id.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (st, 3, ps.key.workflow);
    sqlite3_bind_text(st, 4, ps.blob.c_str(),             -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 5, ps.calibrated_at);
    sqlite3_bind_int64(st, 6, ps.last_used_at);

    int rc = sqlite3_step(st);
    check(rc, impl_->db, SQL);
    return sqlite3_last_insert_rowid(impl_->db);
}

bool ParamDb::get(const ParamKey& key, StoredParamSet& out) {
    static const char* SQL =
        "SELECT rowid, blob, calibrated_at, last_used_at "
        "FROM param_sets "
        "WHERE ink_batch_id=? AND paper_id=? AND workflow=?;";

    Stmt st = prepare(impl_->db, SQL);
    sqlite3_bind_text(st, 1, key.ink_batch_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, key.paper_id.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (st, 3, key.workflow);

    int rc = sqlite3_step(st);
    if (rc == SQLITE_DONE) return false;
    check(rc, impl_->db, SQL);

    out.id            = sqlite3_column_int64(st, 0);
    out.key           = key;
    const char* blob  = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
    out.blob          = blob ? blob : "";
    out.calibrated_at = sqlite3_column_int64(st, 2);
    out.last_used_at  = sqlite3_column_int64(st, 3);
    return true;
}

bool ParamDb::remove(const ParamKey& key) {
    static const char* SQL =
        "DELETE FROM param_sets "
        "WHERE ink_batch_id=? AND paper_id=? AND workflow=?;";

    Stmt st = prepare(impl_->db, SQL);
    sqlite3_bind_text(st, 1, key.ink_batch_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, key.paper_id.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (st, 3, key.workflow);

    int rc = sqlite3_step(st);
    check(rc, impl_->db, SQL);
    return sqlite3_changes(impl_->db) > 0;
}

std::vector<StoredParamSet> ParamDb::list_mru(int limit) {
    static const char* SQL =
        "SELECT rowid, ink_batch_id, paper_id, workflow, blob, calibrated_at, last_used_at "
        "FROM param_sets "
        "ORDER BY last_used_at DESC "
        "LIMIT ?;";

    Stmt st = prepare(impl_->db, SQL);
    sqlite3_bind_int(st, 1, limit);

    std::vector<StoredParamSet> results;
    while (sqlite3_step(st) == SQLITE_ROW) {
        StoredParamSet ps;
        ps.id                  = sqlite3_column_int64(st, 0);
        const char* ink        = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
        const char* paper      = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
        ps.key.ink_batch_id    = ink   ? ink   : "";
        ps.key.paper_id        = paper ? paper : "";
        ps.key.workflow        = sqlite3_column_int(st, 3);
        const char* blob       = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
        ps.blob                = blob  ? blob  : "";
        ps.calibrated_at       = sqlite3_column_int64(st, 5);
        ps.last_used_at        = sqlite3_column_int64(st, 6);
        results.push_back(std::move(ps));
    }
    return results;
}

int ParamDb::count() {
    static const char* SQL = "SELECT COUNT(*) FROM param_sets;";
    Stmt st = prepare(impl_->db, SQL);
    int rc = sqlite3_step(st);
    check(rc, impl_->db, SQL);
    return sqlite3_column_int(st, 0);
}

}  // namespace calibration
}  // namespace jcolour
