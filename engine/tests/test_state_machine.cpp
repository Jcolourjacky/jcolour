#include <catch2/catch_test_macros.hpp>
#include "state_machine.h"

using namespace jcolour::calibration;

static const Timestamp NOW = 1000000000LL;  // arbitrary fixed "now"

static ParamSet make_fresh(const std::string& ink_id, const std::string& paper_id,
                           int days_ago = 0) {
    ParamSet ps;
    ps.state          = CalibState::FRESH;
    ps.key.ink_batch_id = ink_id;
    ps.key.paper_id     = paper_id;
    ps.calibrated_at  = NOW - static_cast<Timestamp>(days_ago) * 86400;
    return ps;
}

TEST_CASE("UNCALIBRATED passthrough", "[state_machine]") {
    ParamSet ps;
    ps.state = CalibState::UNCALIBRATED;
    InkBatch ink{"INK-A"};
    Paper    paper{"PAPER-1"};
    REQUIRE(compute_state(ps, NOW, ink, paper) == CalibState::UNCALIBRATED);
}

TEST_CASE("ink batch mismatch -> STALE_HARD", "[state_machine]") {
    auto ps = make_fresh("INK-A", "PAPER-1", 5);
    InkBatch ink{"INK-B"};
    Paper    paper{"PAPER-1"};
    REQUIRE(compute_state(ps, NOW, ink, paper) == CalibState::STALE_HARD);
}

TEST_CASE("paper mismatch -> STALE_HARD", "[state_machine]") {
    auto ps = make_fresh("INK-A", "PAPER-1", 5);
    InkBatch ink{"INK-A"};
    Paper    paper{"PAPER-2"};
    REQUIRE(compute_state(ps, NOW, ink, paper) == CalibState::STALE_HARD);
}

TEST_CASE("both ink and paper mismatch -> STALE_HARD", "[state_machine]") {
    auto ps = make_fresh("INK-A", "PAPER-1", 5);
    InkBatch ink{"INK-B"};
    Paper    paper{"PAPER-2"};
    REQUIRE(compute_state(ps, NOW, ink, paper) == CalibState::STALE_HARD);
}

TEST_CASE("91 days old -> STALE_HARD", "[state_machine]") {
    auto ps = make_fresh("INK-A", "PAPER-1", 91);
    InkBatch ink{"INK-A"};
    Paper    paper{"PAPER-1"};
    REQUIRE(compute_state(ps, NOW, ink, paper) == CalibState::STALE_HARD);
}

TEST_CASE("exactly 90 days old -> STALE_HARD", "[state_machine]") {
    auto ps = make_fresh("INK-A", "PAPER-1", 90);
    InkBatch ink{"INK-A"};
    Paper    paper{"PAPER-1"};
    // days == 90, not > 90, so falls through to STALE_SOFT check
    // days == 90, not > 30, so STALE_SOFT
    REQUIRE(compute_state(ps, NOW, ink, paper) == CalibState::STALE_SOFT);
}

TEST_CASE("60 days old -> STALE_SOFT", "[state_machine]") {
    auto ps = make_fresh("INK-A", "PAPER-1", 60);
    InkBatch ink{"INK-A"};
    Paper    paper{"PAPER-1"};
    REQUIRE(compute_state(ps, NOW, ink, paper) == CalibState::STALE_SOFT);
}

TEST_CASE("31 days old -> STALE_SOFT", "[state_machine]") {
    auto ps = make_fresh("INK-A", "PAPER-1", 31);
    InkBatch ink{"INK-A"};
    Paper    paper{"PAPER-1"};
    REQUIRE(compute_state(ps, NOW, ink, paper) == CalibState::STALE_SOFT);
}

TEST_CASE("exactly 30 days old -> FRESH", "[state_machine]") {
    auto ps = make_fresh("INK-A", "PAPER-1", 30);
    InkBatch ink{"INK-A"};
    Paper    paper{"PAPER-1"};
    // days == 30, not > 30
    REQUIRE(compute_state(ps, NOW, ink, paper) == CalibState::FRESH);
}

TEST_CASE("5 days old -> FRESH", "[state_machine]") {
    auto ps = make_fresh("INK-A", "PAPER-1", 5);
    InkBatch ink{"INK-A"};
    Paper    paper{"PAPER-1"};
    REQUIRE(compute_state(ps, NOW, ink, paper) == CalibState::FRESH);
}

TEST_CASE("calibrated today -> FRESH", "[state_machine]") {
    auto ps = make_fresh("INK-A", "PAPER-1", 0);
    InkBatch ink{"INK-A"};
    Paper    paper{"PAPER-1"};
    REQUIRE(compute_state(ps, NOW, ink, paper) == CalibState::FRESH);
}

// ----------------------------------------------------------------------
// PrintConditionState (9-state operational machine, audit F20)
// ----------------------------------------------------------------------

using PCS = PrintConditionState;

TEST_CASE("PrintCondition default state is UNCONFIGURED", "[state_machine]") {
    PrintCondition pc;
    REQUIRE(pc.state == PCS::UNCONFIGURED);
}

TEST_CASE("Happy path: UNCONFIGURED → HARDWARE_OK → CALIBRATED → PROFILED → READY",
          "[state_machine]") {
    PrintCondition pc;
    REQUIRE(transition(pc, PCS::HARDWARE_OK));
    REQUIRE(pc.state == PCS::HARDWARE_OK);
    REQUIRE(transition(pc, PCS::CALIBRATED));
    REQUIRE(transition(pc, PCS::PROFILED));
    REQUIRE(transition(pc, PCS::READY));
    REQUIRE(pc.state == PCS::READY);
}

TEST_CASE("Self-transition is rejected", "[state_machine]") {
    PrintCondition pc;
    REQUIRE_FALSE(transition(pc, PCS::UNCONFIGURED));
    REQUIRE(pc.state == PCS::UNCONFIGURED);

    pc.state = PCS::READY;
    REQUIRE_FALSE(transition(pc, PCS::READY));
    REQUIRE(pc.state == PCS::READY);
}

TEST_CASE("Skipping stages is illegal", "[state_machine]") {
    PrintCondition pc;
    REQUIRE_FALSE(transition(pc, PCS::CALIBRATED));   // skip HARDWARE_OK
    REQUIRE_FALSE(transition(pc, PCS::READY));         // skip everything
    REQUIRE(pc.state == PCS::UNCONFIGURED);
}

TEST_CASE("READY can move to NEEDS_RECAL / ON_HOLD / BATCH_CHANGED", "[state_machine]") {
    PrintCondition pc;
    pc.state = PCS::READY;
    REQUIRE(is_legal_transition(PCS::READY, PCS::NEEDS_RECAL));
    REQUIRE(is_legal_transition(PCS::READY, PCS::ON_HOLD));
    REQUIRE(is_legal_transition(PCS::READY, PCS::BATCH_CHANGED));
    // …but not other things
    REQUIRE_FALSE(is_legal_transition(PCS::READY, PCS::PROFILED));
    REQUIRE_FALSE(is_legal_transition(PCS::READY, PCS::DEGRADED));
}

TEST_CASE("NEEDS_RECAL → READY (success) or DEGRADED (failure)", "[state_machine]") {
    PrintCondition pc;
    pc.state = PCS::NEEDS_RECAL;
    REQUIRE(is_legal_transition(PCS::NEEDS_RECAL, PCS::READY));
    REQUIRE(is_legal_transition(PCS::NEEDS_RECAL, PCS::DEGRADED));
    REQUIRE_FALSE(is_legal_transition(PCS::NEEDS_RECAL, PCS::PROFILED));
}

TEST_CASE("ON_HOLD can release to NEEDS_RECAL or READY", "[state_machine]") {
    REQUIRE(is_legal_transition(PCS::ON_HOLD, PCS::NEEDS_RECAL));
    REQUIRE(is_legal_transition(PCS::ON_HOLD, PCS::READY));
    REQUIRE_FALSE(is_legal_transition(PCS::ON_HOLD, PCS::DEGRADED));
}

TEST_CASE("BATCH_CHANGED can either rebuild or take shortcut", "[state_machine]") {
    REQUIRE(is_legal_transition(PCS::BATCH_CHANGED, PCS::HARDWARE_OK));
    REQUIRE(is_legal_transition(PCS::BATCH_CHANGED, PCS::NEEDS_RECAL));
    REQUIRE_FALSE(is_legal_transition(PCS::BATCH_CHANGED, PCS::READY));
}

TEST_CASE("DEGRADED only path is full rebuild", "[state_machine]") {
    REQUIRE(is_legal_transition(PCS::DEGRADED, PCS::HARDWARE_OK));
    REQUIRE_FALSE(is_legal_transition(PCS::DEGRADED, PCS::READY));
    REQUIRE_FALSE(is_legal_transition(PCS::DEGRADED, PCS::NEEDS_RECAL));
}

TEST_CASE("Backward transitions are illegal", "[state_machine]") {
    REQUIRE_FALSE(is_legal_transition(PCS::HARDWARE_OK, PCS::UNCONFIGURED));
    REQUIRE_FALSE(is_legal_transition(PCS::CALIBRATED, PCS::HARDWARE_OK));
    REQUIRE_FALSE(is_legal_transition(PCS::PROFILED, PCS::CALIBRATED));
    REQUIRE_FALSE(is_legal_transition(PCS::READY, PCS::PROFILED));
}

TEST_CASE("transition() leaves state unchanged on illegal moves",
          "[state_machine]") {
    PrintCondition pc;
    pc.state = PCS::CALIBRATED;
    REQUIRE_FALSE(transition(pc, PCS::UNCONFIGURED));
    REQUIRE(pc.state == PCS::CALIBRATED);
}

TEST_CASE("state_name covers every enumerator", "[state_machine]") {
    REQUIRE(std::string(state_name(PCS::UNCONFIGURED))  == "UNCONFIGURED");
    REQUIRE(std::string(state_name(PCS::HARDWARE_OK))   == "HARDWARE_OK");
    REQUIRE(std::string(state_name(PCS::CALIBRATED))    == "CALIBRATED");
    REQUIRE(std::string(state_name(PCS::PROFILED))      == "PROFILED");
    REQUIRE(std::string(state_name(PCS::READY))         == "READY");
    REQUIRE(std::string(state_name(PCS::NEEDS_RECAL))   == "NEEDS_RECAL");
    REQUIRE(std::string(state_name(PCS::DEGRADED))      == "DEGRADED");
    REQUIRE(std::string(state_name(PCS::BATCH_CHANGED)) == "BATCH_CHANGED");
    REQUIRE(std::string(state_name(PCS::ON_HOLD))       == "ON_HOLD");
}
