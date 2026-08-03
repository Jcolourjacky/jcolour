#include <catch2/catch_test_macros.hpp>
#include "self_test.h"
#include "state_machine.h"

using namespace jcolour::calibration;

TEST_CASE("[self-test] classify_result workflow thresholds", "[self-test]") {
    SECTION("workflow 1 (single black) has tight thresholds") {
        // Delta below both thresholds
        REQUIRE(classify_result(0.3, 1.0, 1) == SelfTestResult::Verdict::PASS);
        // Delta in light recalib range
        REQUIRE(classify_result(0.8, 2.0, 1) == SelfTestResult::Verdict::LIGHT_RECALIB);
        // Delta too high
        REQUIRE(classify_result(1.5, 3.0, 1) == SelfTestResult::Verdict::FULL_RECALIB);
    }

    SECTION("workflow 2 (four-level gray) has medium thresholds") {
        REQUIRE(classify_result(0.8, 1.5, 2) == SelfTestResult::Verdict::PASS);
        REQUIRE(classify_result(1.5, 3.0, 2) == SelfTestResult::Verdict::LIGHT_RECALIB);
        REQUIRE(classify_result(3.0, 5.0, 2) == SelfTestResult::Verdict::FULL_RECALIB);
    }

    SECTION("workflow 3 (gray + tone) has slightly looser thresholds") {
        REQUIRE(classify_result(1.0, 2.0, 3) == SelfTestResult::Verdict::PASS);
        REQUIRE(classify_result(2.5, 5.0, 3) == SelfTestResult::Verdict::LIGHT_RECALIB);
        REQUIRE(classify_result(5.0, 10.0, 3) == SelfTestResult::Verdict::FULL_RECALIB);
    }

    SECTION("workflow 4 (RGB) same thresholds as workflow 3") {
        REQUIRE(classify_result(1.0, 2.0, 4) == SelfTestResult::Verdict::PASS);
        REQUIRE(classify_result(2.5, 5.0, 4) == SelfTestResult::Verdict::LIGHT_RECALIB);
    }

    SECTION("workflow 6 (ProPhoto) same as workflow 4") {
        REQUIRE(classify_result(1.0, 2.0, 6) == SelfTestResult::Verdict::PASS);
    }

    SECTION("unknown workflow defaults to medium thresholds") {
        REQUIRE(classify_result(0.5, 1.0, 99) == SelfTestResult::Verdict::PASS);
        // 1.5 < 2.0 (2*1.0) and 3.0 < 4.0 (2*2.0) → LIGHT_RECALIB
        REQUIRE(classify_result(1.5, 3.0, 99) == SelfTestResult::Verdict::LIGHT_RECALIB);
        // 3.0 >= 2.0 → FULL_RECALIB
        REQUIRE(classify_result(3.0, 5.0, 99) == SelfTestResult::Verdict::FULL_RECALIB);
    }
}

TEST_CASE("[self-test] run_self_test dry run simulation", "[self-test]") {
    SelfTestOptions options;
    options.workflow = 1;
    options.ink_batch_id = "TEST123";
    options.paper_id = "HPR308";
    options.dry_run = true;

    auto result = run_self_test(options, "", 0);

    REQUIRE(result.delta_e_avg >= 0.0);
    REQUIRE(result.delta_e_max >= 0.0);
    REQUIRE(result.patch_count > 0);
    REQUIRE(!result.per_patch_delta_e.empty());
    REQUIRE(result.tested_at > 0);
    REQUIRE(result.error_message.empty());
}

TEST_CASE("[self-test] run_self_test with real i1 scan fails", "[self-test]") {
    SelfTestOptions options;
    options.workflow = 1;
    options.ink_batch_id = "TEST123";
    options.paper_id = "HPR308";
    options.dry_run = false;  // Real scan

    auto result = run_self_test(options, "", 0);

    // Should fail because i1Pro3 is not connected
    REQUIRE(result.error_message.find("not connected") != std::string::npos);
    REQUIRE(result.patch_count == 0);
}

TEST_CASE("[self-test] SelfTestResult default values", "[self-test]") {
    SelfTestResult result;

    REQUIRE(result.verdict == SelfTestResult::Verdict::FULL_RECALIB);
    REQUIRE(result.delta_e_avg == 0.0);
    REQUIRE(result.delta_e_max == 0.0);
    REQUIRE(result.patch_count == 0);
    REQUIRE(result.per_patch_delta_e.empty());
    REQUIRE(result.tested_at == 0);
    REQUIRE(result.error_message.empty());
}

TEST_CASE("[self-test] SelfTestOptions default values", "[self-test]") {
    SelfTestOptions options;

    REQUIRE(options.workflow == 1);
    REQUIRE(options.ink_batch_id.empty());
    REQUIRE(options.paper_id.empty());
    REQUIRE(options.dry_run == false);
}

TEST_CASE("[self-test] run_self_test captures per-patch delta-E", "[self-test]") {
    SelfTestOptions options;
    options.workflow = 1;
    options.dry_run = true;

    auto result = run_self_test(options, "", 0);

    // Verify per-patch data matches patch count
    REQUIRE(static_cast<int>(result.per_patch_delta_e.size()) == result.patch_count);

    // Verify all values are non-negative
    for (double de : result.per_patch_delta_e) {
        REQUIRE(de >= 0.0);
    }
}

TEST_CASE("[self-test] workflow 1 chart has expected patch count", "[self-test]") {
    SelfTestOptions options;
    options.workflow = 1;
    options.dry_run = true;

    auto result = run_self_test(options, "", 0);

    // Workflow 1 self-test should have 6 patches (5 ink levels + 1 paper white)
    REQUIRE(result.patch_count == 6);
}

TEST_CASE("[self-test] workflow 2 chart has expected patch count", "[self-test]") {
    SelfTestOptions options;
    options.workflow = 2;
    options.dry_run = true;

    auto result = run_self_test(options, "", 0);

    // Workflow 2 self-test should have 19 patches
    REQUIRE(result.patch_count == 19);
}