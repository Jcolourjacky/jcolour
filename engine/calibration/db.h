#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace jcolour {
namespace calibration {

// Thrown on any SQLite error.
struct DbError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Identifies a unique calibration parameter set.
struct ParamKey {
    std::string ink_batch_id;
    std::string paper_id;
    int         workflow = 0;
};

// A stored calibration parameter set.
struct StoredParamSet {
    int64_t     id           = 0;
    ParamKey    key;
    std::string blob;          // serialised params (opaque bytes, base64 or raw)
    int64_t     calibrated_at = 0;  // Unix seconds
    int64_t     last_used_at  = 0;  // Unix seconds (MRU)
};

// SQLite-backed store for calibration parameter sets.
// Thread-safety: single-threaded use only.
class ParamDb {
public:
    // Opens (or creates) the database at `path`.
    // Pass ":memory:" for an in-memory database.
    explicit ParamDb(const std::string& path);
    ~ParamDb();

    // Non-copyable, movable.
    ParamDb(const ParamDb&)            = delete;
    ParamDb& operator=(const ParamDb&) = delete;
    ParamDb(ParamDb&&)                 = default;
    ParamDb& operator=(ParamDb&&)      = default;

    // Insert or replace a parameter set.
    // Returns the rowid of the upserted row.
    int64_t upsert(const StoredParamSet& ps);

    // Fetch by primary key (ink_batch_id, paper_id, workflow).
    // Returns false if not found; fills `out` on success.
    bool get(const ParamKey& key, StoredParamSet& out);

    // Delete by primary key. Returns true if a row was deleted.
    bool remove(const ParamKey& key);

    // Return up to `limit` most-recently-used entries, newest first.
    std::vector<StoredParamSet> list_mru(int limit = 20);

    // Total number of stored parameter sets.
    int count();

private:
    struct Impl;
    Impl* impl_ = nullptr;

    void apply_schema();
};

}  // namespace calibration
}  // namespace jcolour
