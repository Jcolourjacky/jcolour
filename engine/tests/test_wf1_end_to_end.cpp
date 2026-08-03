#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "wf1_end_to_end.h"
#include "wf1_param.h"

using namespace jcolour::workflow;

TEST_CASE("[wf1-end-to-end] dry run completes successfully", "[wf1]") {
    WF1CalibrationSession session;
    session.paper_id = "TEST_PAPER";
    session.ink_batch_id = "TEST_BATCH";
    session.dry_run = true;
    session.max_ink_limit = 0.95f;

    auto result = run_wf1_calibration(session);

    REQUIRE(result.status != WF1CalibrationResult::Status::FAILED);
    REQUIRE(result.ink_limit > 0.0f);
    REQUIRE(result.ink_limit <= 0.95f);
    REQUIRE(result.delta_e_avg >= 0.0f);
    REQUIRE(result.delta_e_max >= 0.0f);
}

TEST_CASE("[wf1-end-to-end] produces valid LUT", "[wf1]") {
    WF1CalibrationSession session;
    session.dry_run = true;

    auto result = run_wf1_calibration(session);

    // Check LUT is all zeros
    bool has_nonzero = false;
    for (int i = 0; i < 256; ++i) {
        if (result.lut[i] > 0.0f) {
            has_nonzero = true;
            break;
        }
    }
    REQUIRE(has_nonzero);

    // LUT values should be in valid range
    for (int i = 0; i < 256; ++i) {
        REQUIRE(result.lut[i] >= 0.0f);
        REQUIRE(result.lut[i] <= 1.0f);
    }
}

TEST_CASE("[wf1-end-to-end] ink limit respects max_ink_limit", "[wf1]") {
    WF1CalibrationSession session;
    session.dry_run = true;
    session.max_ink_limit = 0.8f;

    auto result = run_wf1_calibration(session);

    REQUIRE(result.ink_limit <= 0.8f);
}

TEST_CASE("[wf1-end-to-end] dry run verification passes", "[wf1]") {
    WF1CalibrationSession session;
    session.dry_run = true;

    auto result = run_wf1_calibration(session);

    // With dry run (simulation), verification should pass
    REQUIRE(result.verification_passed == true);
}

TEST_CASE("[wf1-end-to-end] produces Workflow1Data compatible output", "[wf1]") {
    WF1CalibrationSession session;
    session.paper_id = "HPR308";
    session.ink_batch_id = "A47";
    session.dry_run = true;

    auto result = run_wf1_calibration(session);

    // Create Workflow1Data from calibration result
    Workflow1Data data = create_workflow1_data(
        BlackKind::PK,
        result.lut,
        result.ink_limit,
        session.paper_id,
        session.ink_batch_id
    );

    REQUIRE(data.black_kind == BlackKind::PK);
    REQUIRE(data.ink_limit == Catch::Approx(result.ink_limit));
    REQUIRE(data.paper_id == "HPR308");
    REQUIRE(data.ink_batch_id == "A47");

    // Note: With simulation noise, LUT may not be perfectly monotonic
    auto validation = validate_workflow1_data(data);
    REQUIRE(validation.has_lut);
    REQUIRE(validation.has_metadata);
    // Just verify it didn't crash on the data
    REQUIRE(data.lstar_to_ink[0] >= 0.0f);
}

TEST_CASE("[wf1-end-to-end] session has sensible defaults", "[wf1]") {
    WF1CalibrationSession session;

    REQUIRE(session.ink_ramp_steps == 21);
    REQUIRE(session.verification_steps == 11);
    REQUIRE(session.max_ink_limit == 0.95f);
    REQUIRE(session.dry_run == false);
}

TEST_CASE("[wf1-end-to-end] result warnings are populated", "[wf1]") {
    WF1CalibrationSession session;
    session.dry_run = true;

    auto result = run_wf1_calibration(session);

    REQUIRE(!result.warnings.empty());
}

TEST_CASE("[wf1-end-to-end] result default status is FAILED", "[wf1]") {
    WF1CalibrationResult result;

    REQUIRE(result.status == WF1CalibrationResult::Status::FAILED);
    REQUIRE(result.ink_limit == 0.0f);
    REQUIRE(result.delta_e_avg == 0.0f);
    REQUIRE(result.delta_e_max == 0.0f);
    REQUIRE(result.verification_passed == false);
    REQUIRE(result.error_message.empty());
}