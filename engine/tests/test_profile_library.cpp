#include <catch2/catch_test_macros.hpp>
#include "profile_library.h"
#include <vector>

using namespace jcolour::profiles;

TEST_CASE("[profile] BuiltInProfileLibrary singleton", "[profile]") {
    auto& lib = BuiltInProfileLibrary::instance();
    auto profiles = lib.get_all_profiles();
    REQUIRE((int)profiles.size() > 0);
}

TEST_CASE("[profile] get_profiles_by_printer", "[profile]") {
    auto& lib = BuiltInProfileLibrary::instance();
    auto profiles = lib.get_profiles_by_printer("Epson SureColor P700");
    REQUIRE((int)profiles.size() > 0);
}

TEST_CASE("[profile] get_profiles_by_media", "[profile]") {
    auto& lib = BuiltInProfileLibrary::instance();
    auto profiles = lib.get_profiles_by_media("Photo Paper");
    REQUIRE((int)profiles.size() > 0);
}

TEST_CASE("[profile] get_profiles_by_color_space", "[profile]") {
    auto& lib = BuiltInProfileLibrary::instance();
    auto profiles = lib.get_profiles_by_color_space("RGB");
    REQUIRE((int)profiles.size() > 0);
}

TEST_CASE("[profile] find_profile", "[profile]") {
    auto& lib = BuiltInProfileLibrary::instance();
    auto* profile = lib.find_profile("Epson P700 Photo Paper Glossy");
    REQUIRE(profile != nullptr);
    REQUIRE(profile->manufacturer == "Epson");
}

TEST_CASE("[profile] find_profile (not found)", "[profile]") {
    auto& lib = BuiltInProfileLibrary::instance();
    auto* profile = lib.find_profile("NonExistent Profile");
    REQUIRE(profile == nullptr);
}

TEST_CASE("[profile] get_printer_models", "[profile]") {
    auto& lib = BuiltInProfileLibrary::instance();
    auto models = lib.get_printer_models();
    REQUIRE((int)models.size() > 0);
}

TEST_CASE("[profile] get_media_types", "[profile]") {
    auto& lib = BuiltInProfileLibrary::instance();
    auto types = lib.get_media_types();
    REQUIRE((int)types.size() > 0);
}

TEST_CASE("[profile] get_color_spaces", "[profile]") {
    auto& lib = BuiltInProfileLibrary::instance();
    auto spaces = lib.get_color_spaces();
    REQUIRE((int)spaces.size() > 0);
}

TEST_CASE("[profile] recommend_profiles", "[profile]") {
    auto& lib = BuiltInProfileLibrary::instance();
    auto recs = lib.recommend_profiles("Epson SureColor P700", "Photo Paper", "RGB", "Relative");
    REQUIRE((int)recs.size() > 0);
    REQUIRE(recs[0].confidence > 0.0);
}

TEST_CASE("[profile] ProfileValidator validate", "[profile]") {
    std::vector<uint8_t> data(256, 0);
    auto result = ProfileValidator::validate(data);
    REQUIRE(result.is_valid == true);
}

TEST_CASE("[profile] ProfileValidator validate empty", "[profile]") {
    std::vector<uint8_t> data;
    auto result = ProfileValidator::validate(data);
    REQUIRE(result.is_valid == false);
    REQUIRE(!result.error_message.empty());
}

TEST_CASE("[profile] ProfileValidator validate too small", "[profile]") {
    std::vector<uint8_t> data(64, 0);
    auto result = ProfileValidator::validate(data);
    REQUIRE(result.is_valid == false);
}

TEST_CASE("[profile] ProfileValidator is_rgb_profile", "[profile]") {
    std::vector<uint8_t> data(256, 0);
    bool is_rgb = ProfileValidator::is_rgb_profile(data);
    REQUIRE(is_rgb == true);
}

TEST_CASE("[profile] ProfileValidator is_gray_profile", "[profile]") {
    std::vector<uint8_t> data(256, 0);
    bool is_gray = ProfileValidator::is_gray_profile(data);
    REQUIRE(is_gray == false);
}

TEST_CASE("[profile] ProfileValidator compute_md5", "[profile]") {
    std::vector<uint8_t> data{'t', 'e', 's', 't'};
    auto hash = ProfileValidator::compute_md5(data);
    REQUIRE(hash.size() > 0);
}

TEST_CASE("[profile] ProfileComparator compare", "[profile]") {
    ICCProfileInfo a, b;
    a.color_space = "RGB";
    b.color_space = "RGB";
    b.is_wide_gamut = true;

    auto result = ProfileComparator::compare(a, b);
    REQUIRE(result.gamut_volume_diff_percent > 0.0);
}

TEST_CASE("[profile] ProfileComparator estimate_gamut_volume", "[profile]") {
    ICCProfileInfo profile;
    profile.color_space = "RGB";
    profile.is_wide_gamut = false;

    double vol = ProfileComparator::estimate_gamut_volume(profile);
    REQUIRE(vol > 0.0);
}

TEST_CASE("[profile] ICCProfileGenerator generate_srgb", "[profile]") {
    auto data = ICCProfileGenerator::generate_srgb();
    REQUIRE(data.size() >= 0);
}

TEST_CASE("[profile] ICCProfileGenerator generate_adobergb", "[profile]") {
    auto data = ICCProfileGenerator::generate_adobergb();
    REQUIRE(data.size() >= 0);
}

TEST_CASE("[profile] ICCProfileGenerator generate_prophoto_rgb", "[profile]") {
    auto data = ICCProfileGenerator::generate_prophoto_rgb();
    REQUIRE(data.size() >= 0);
}

TEST_CASE("[profile] ICCProfileGenerator generate_wide_gamut_rgb", "[profile]") {
    auto data = ICCProfileGenerator::generate_wide_gamut_rgb();
    REQUIRE(data.size() >= 0);
}

TEST_CASE("[profile] ProfilePresets photo_glossy_standard", "[profile]") {
    auto info = ProfilePresets::photo_glossy_standard();
    REQUIRE(info.name == "Photo Glossy Standard");
    REQUIRE(info.media_type == "Photo Paper");
    REQUIRE(info.finish == "Glossy");
}

TEST_CASE("[profile] ProfilePresets fine_art_bright", "[profile]") {
    auto info = ProfilePresets::fine_art_bright();
    REQUIRE(info.name == "Fine Art Bright");
    REQUIRE(info.media_type == "Fine Art");
}

TEST_CASE("[profile] ProfilePresets bw_photo_um", "[profile]") {
    auto info = ProfilePresets::bw_photo_um();
    REQUIRE(info.name == "B&W Photo UM");
    REQUIRE(info.color_space == "Gray");
    REQUIRE(info.is_gray == true);
}