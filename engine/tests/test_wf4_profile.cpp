#include <catch2/catch_test_macros.hpp>
#include "wf4_profile.h"
#include <fstream>
#include <cstdio>

using namespace jcolour::workflow;

TEST_CASE("[wf4] generate_ti1_file creates valid file", "[wf4]") {
    auto data = generate_ti1_file("/tmp/test_wf4.ti1", 100);
    REQUIRE(data.patch_names.size() == 100);
    REQUIRE(data.rgb_values.size() == 300);

    std::ifstream in("/tmp/test_wf4.ti1");
    REQUIRE(in.good());

    std::string line;
    bool found_data = false;
    while (std::getline(in, line)) {
        if (line.find("BEGIN_DATA") != std::string::npos) {
            found_data = true;
            break;
        }
    }
    REQUIRE(found_data);
}

TEST_CASE("[wf4] TI1 contains valid RGB values", "[wf4]") {
    auto data = generate_ti1_file("/tmp/test_wf4_rgb.ti1", 50);

    for (size_t i = 0; i < data.rgb_values.size(); ++i) {
        REQUIRE(data.rgb_values[i] >= 0.0f);
        REQUIRE(data.rgb_values[i] <= 1.0f);
    }
}

TEST_CASE("[wf4] parse_ti3_file handles empty file", "[wf4]") {
    std::ofstream out("/tmp/empty.ti3");
    out << "CTI3\n";
    out << "BEGIN_DATA\n";
    out << "END_DATA\n";
    out.close();

    auto data = parse_ti3_file("/tmp/empty.ti3");
    REQUIRE(data.patch_names.empty());
}

TEST_CASE("[wf4] WF4Profile default values", "[wf4]") {
    WF4Profile profile;
    REQUIRE(profile.avg_delta_e == 0.0);
    REQUIRE(profile.max_delta_e == 0.0);
}

TEST_CASE("[wf4] ProfileCheckResult default values", "[wf4]") {
    ProfileCheckResult result;
    REQUIRE(result.avg_delta_e == 0.0);
    REQUIRE(result.max_delta_e == 0.0);
    REQUIRE(result.patches_tested == 0);
    REQUIRE(result.passed == false);
}

TEST_CASE("[wf4] verify_wf4_profile passes with good values", "[wf4]") {
    WF4Profile profile;
    profile.avg_delta_e = 1.0;
    profile.max_delta_e = 3.0;

    TI3Data ref;
    for (int i = 0; i < 100; ++i) {
        ref.patch_names.push_back("P" + std::to_string(i));
    }

    auto result = verify_wf4_profile(profile, ref);
    REQUIRE(result.passed == true);
    REQUIRE(result.patches_tested == 100);
}

TEST_CASE("[wf4] verify_wf4_profile fails with bad values", "[wf4]") {
    WF4Profile profile;
    profile.avg_delta_e = 2.0;
    profile.max_delta_e = 5.0;

    TI3Data ref;
    ref.patch_names.push_back("P1");

    auto result = verify_wf4_profile(profile, ref);
    REQUIRE(result.passed == false);
}

TEST_CASE("[wf4] write_ti3_file creates valid file", "[wf4]") {
    TI3Data data;
    data.patch_names = {"P1", "P2", "P3"};
    data.lab_values = {50.0f, 2.0f, 3.0f, 60.0f, 4.0f, 5.0f, 70.0f, 6.0f, 7.0f};

    bool result = write_ti3_file(data, "/tmp/test_wf4.ti3");
    REQUIRE(result == true);

    std::ifstream in("/tmp/test_wf4.ti3");
    REQUIRE(in.good());

    std::string line;
    bool found_data = false;
    while (std::getline(in, line)) {
        if (line.find("BEGIN_DATA") != std::string::npos) {
            found_data = true;
            break;
        }
    }
    REQUIRE(found_data);
}

TEST_CASE("[wf4] write_ti3_file handles empty data", "[wf4]") {
    TI3Data data;
    bool result = write_ti3_file(data, "/tmp/empty.ti3");
    REQUIRE(result == false);
}

TEST_CASE("[wf4] generate_wf4_icc_profile with options", "[wf4]") {
    TI3Data data;
    data.patch_names = {"P1", "P2"};
    data.lab_values = {50.0f, 2.0f, 3.0f, 60.0f, 4.0f, 5.0f};

    ArgyllProfileOptions opts;
    opts.profile_name = "test_profile";
    opts.quality = 'm';
    opts.perceptual_table = true;
    opts.relcol_table = true;
    opts.saturation_table = false;

    auto profile = generate_wf4_icc_profile(data, "test", "/tmp", opts);
    // Argyll may not be installed, so profile may be empty
    // Just verify it doesn't crash
    REQUIRE(profile.name == "test");
}

TEST_CASE("[wf4] ArgyllTargetOptions default values", "[wf4]") {
    ArgyllTargetOptions opts;
    REQUIRE(opts.patch_count == 1500);
    REQUIRE(opts.lut_size == 33);
    REQUIRE(opts.include_extended_inks == true);
}

TEST_CASE("[wf4] ArgyllProfileOptions default values", "[wf4]") {
    ArgyllProfileOptions opts;
    REQUIRE(opts.profile_name == "jcolour");
    REQUIRE(opts.quality == 'h');
    REQUIRE(opts.perceptual_table == true);
}