#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "wf1_param.h"
#include <algorithm>

using namespace jcolour::workflow;

namespace {

Workflow1Data make_test_data() {
    Workflow1Data data;
    data.black_kind = BlackKind::PK;
    data.ink_limit = 0.85f;
    data.paper_id = "HPR308";
    data.ink_batch_id = "A47";
    data.calibrated_at = 1715000000;

    // Create a monotonic LUT: high ink at L*=0, low at L*=100
    for (int i = 0; i < 256; ++i) {
        float lstar = static_cast<float>(i) * 100.0f / 255.0f;
        data.lstar_to_ink[i] = 0.85f * (1.0f - lstar / 100.0f);  // Linear decreasing
    }
    return data;
}

}  // anonymous namespace

TEST_CASE("[wf1-param] Workflow1Data serialization roundtrip", "[wf1-param]") {
    auto original = make_test_data();

    SECTION("serialize and deserialize preserves all fields") {
        std::string json = serialize_workflow1(original);
        REQUIRE(!json.empty());

        auto restored = deserialize_workflow1(json);

        REQUIRE(restored.black_kind == original.black_kind);
        REQUIRE(restored.ink_limit == original.ink_limit);
        REQUIRE(restored.paper_id == original.paper_id);
        REQUIRE(restored.ink_batch_id == original.ink_batch_id);
        REQUIRE(restored.calibrated_at == original.calibrated_at);

        for (int i = 0; i < 256; ++i) {
            REQUIRE(restored.lstar_to_ink[i] == Catch::Approx(original.lstar_to_ink[i]).margin(0.001f));
        }
    }
}

TEST_CASE("[wf1-param] BlackKind serialization", "[wf1-param]") {
    SECTION("PK serializes correctly") {
        Workflow1Data data;
        data.black_kind = BlackKind::PK;
        auto json = serialize_workflow1(data);
        REQUIRE(json.find("\"PK\"") != std::string::npos);
    }

    SECTION("MK serializes correctly") {
        Workflow1Data data;
        data.black_kind = BlackKind::MK;
        auto json = serialize_workflow1(data);
        REQUIRE(json.find("\"MK\"") != std::string::npos);
    }
}

TEST_CASE("[wf1-param] Validation", "[wf1-param]") {
    SECTION("valid data passes validation") {
        auto data = make_test_data();
        auto result = validate_workflow1_data(data);
        REQUIRE(result.valid);
        REQUIRE(result.has_lut);
        REQUIRE(result.has_metadata);
        REQUIRE(result.lut_is_monotonic);
    }

    SECTION("missing paper_id fails") {
        auto data = make_test_data();
        data.paper_id = "";
        auto result = validate_workflow1_data(data);
        REQUIRE(!result.valid);
        REQUIRE(result.error_message.find("paper_id") != std::string::npos);
    }

    SECTION("missing ink_batch_id fails") {
        auto data = make_test_data();
        data.ink_batch_id = "";
        auto result = validate_workflow1_data(data);
        REQUIRE(!result.valid);
        REQUIRE(result.error_message.find("ink_batch_id") != std::string::npos);
    }

    SECTION("out of range LUT values fails") {
        auto data = make_test_data();
        // Make it out of range: value > 1.0
        data.lstar_to_ink[128] = 1.5f;
        auto result = validate_workflow1_data(data);
        REQUIRE(!result.valid);
        REQUIRE(!result.error_message.empty());
    }

    SECTION("negative LUT values fails") {
        auto data = make_test_data();
        data.lstar_to_ink[64] = -0.1f;
        auto result = validate_workflow1_data(data);
        REQUIRE(!result.valid);
    }
}

TEST_CASE("[wf1-param] create_workflow1_data", "[wf1-param]") {
    std::array<float, 256> lut{};
    for (int i = 0; i < 256; ++i) {
        lut[i] = 0.8f * (255 - i) / 255.0f;
    }

    auto data = create_workflow1_data(
        BlackKind::MK,
        lut,
        0.80f,
        "PM280",
        "B12"
    );

    REQUIRE(data.black_kind == BlackKind::MK);
    REQUIRE(data.ink_limit == 0.80f);
    REQUIRE(data.paper_id == "PM280");
    REQUIRE(data.ink_batch_id == "B12");
    REQUIRE(data.calibrated_at > 0);
    REQUIRE(data.lstar_to_ink[0] == Catch::Approx(0.8f));
}

TEST_CASE("[wf1-param] JSON format", "[wf1-param]") {
    auto data = make_test_data();
    auto json = serialize_workflow1(data);

    // Check JSON structure
    REQUIRE(json.front() == '{');
    REQUIRE(json.back() == '}');
    REQUIRE(json.find("\"lstar_to_ink\"") != std::string::npos);
    REQUIRE(json.find("\"ink_limit\"") != std::string::npos);
    REQUIRE(json.find("\"paper_id\"") != std::string::npos);
}

TEST_CASE("[wf1-param] LUT array size", "[wf1-param]") {
    Workflow1Data data;
    for (int i = 0; i < 256; ++i) {
        data.lstar_to_ink[i] = static_cast<float>(i) / 255.0f;
    }

    auto json = serialize_workflow1(data);
    auto restored = deserialize_workflow1(json);

    // Count commas in array section
    auto arr_start = json.find("[");
    auto arr_end = json.find("]");
    std::string arr_str = json.substr(arr_start + 1, arr_end - arr_start - 1);
    int commas = std::count(arr_str.begin(), arr_str.end(), ',');
    REQUIRE(commas == 255);  // 256 elements = 255 commas
}