#include "profile_library.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <set>
#include <iomanip>

namespace jcolour {
namespace profiles {

// Helper function for case-insensitive string comparison
static bool string_contains(const std::string& str, const std::string& sub) {
    std::string lower_str, lower_sub;
    for (char c : str) lower_str += static_cast<char>(std::tolower(c));
    for (char c : sub) lower_sub += static_cast<char>(std::tolower(c));
    return lower_str.find(lower_sub) != std::string::npos;
}

// BuiltInProfileLibrary implementation
BuiltInProfileLibrary::BuiltInProfileLibrary() {
    initialize_library();
}

BuiltInProfileLibrary::~BuiltInProfileLibrary() = default;

BuiltInProfileLibrary& BuiltInProfileLibrary::instance() {
    static BuiltInProfileLibrary instance;
    return instance;
}

void BuiltInProfileLibrary::initialize_library() {
    add_epson_p700_profiles();
    add_epson_p900_profiles();
    add_epson_p7500_profiles();
    add_epson_p9500_profiles();
    add_common_profiles();
    add_bw_profiles();
    add_wide_gamut_profiles();
}

void BuiltInProfileLibrary::add_epson_p700_profiles() {
    struct ProfileDef { std::string abbr; std::string name; };
    std::vector<ProfileDef> profiles = {
        {"PGP", "Photo Paper Glossy"},
        {"PLM", "Premium Luster"},
        {"PSP", "Premium Semigloss"},
        {"PMM", "Premium Matte"},
        {"AMM", "Archival Matte"},
        {"HPB", "Hot Press Bright"},
        {"CPB", "Cold Press Bright"},
        {"PPG", "Production Photo Glossy"},
        {"PSL", "Production Satin Luster"},
        {"SUA", "Sunset Baryta"},
    };

    for (const auto& p : profiles) {
        ICCProfileInfo info;
        info.name = "Epson P700 " + p.name;
        info.manufacturer = "Epson";
        info.description = "ICC profile for P700 on " + p.name;
        info.color_space = "RGB";
        info.connection_space = "Lab";
        info.pcs = "Lab";
        info.media_type = (p.abbr == "PGP" || p.abbr == "PLM" || p.abbr == "PSP") ? "Photo Paper" : "Fine Art";
        info.finish = (p.abbr.find("Gloss") != std::string::npos) ? "Glossy" : "Matte";
        info.default_intent = "Relative";
        info.rendering_intents = {"Perceptual", "Relative", "Saturation", "Absolute"};
        info.compatible_printers = {"Epson SureColor P700"};
        info.is_wide_gamut = false;
        info.supports_bw_mode = true;
        profiles_.push_back(info);
    }
}

void BuiltInProfileLibrary::add_epson_p900_profiles() {
    struct ProfileDef { std::string abbr; std::string name; };
    std::vector<ProfileDef> profiles = {
        {"PGP", "Photo Paper Glossy"},
        {"PLM", "Premium Luster"},
        {"PSP", "Premium Semigloss"},
        {"PMM", "Premium Matte"},
        {"AMM", "Archival Matte"},
        {"HPB", "Hot Press Bright"},
        {"PPG", "Production Photo Glossy"},
        {"PSL", "Production Satin Luster"},
        {"SUA", "Sunset Baryta"},
        {"FAB", "Fine Art Baryta"},
    };

    for (const auto& p : profiles) {
        ICCProfileInfo info;
        info.name = "Epson P900 " + p.name;
        info.manufacturer = "Epson";
        info.description = "ICC profile for P900 on " + p.name;
        info.color_space = "RGB";
        info.connection_space = "Lab";
        info.pcs = "Lab";
        info.media_type = (p.abbr == "PGP" || p.abbr == "PLM" || p.abbr == "PSP") ? "Photo Paper" : "Fine Art";
        info.finish = (p.abbr.find("Gloss") != std::string::npos) ? "Glossy" : "Matte";
        info.default_intent = "Relative";
        info.rendering_intents = {"Perceptual", "Relative", "Saturation", "Absolute"};
        info.compatible_printers = {"Epson SureColor P900"};
        info.supports_bw_mode = true;
        profiles_.push_back(info);
    }
}

void BuiltInProfileLibrary::add_epson_p7500_profiles() {
    struct ProfileDef { std::string abbr; std::string name; };
    std::vector<ProfileDef> profiles = {
        {"PGP-24", "Photo Paper Glossy 24in"},
        {"PLM-24", "Premium Luster 24in"},
        {"PSP-24", "Premium Semigloss 24in"},
        {"AMM-24", "Archival Matte 24in"},
        {"HPB-24", "Hot Press Bright 24in"},
        {"FAB-24", "Fine Art Baryta 24in"},
        {"CAN-24", "Canvas Natural 24in"},
        {"CAS-24", "Canvas Satin 24in"},
        {"PPG-24", "Production Glossy 24in"},
        {"PSL-24", "Production Luster 24in"},
    };

    for (const auto& p : profiles) {
        ICCProfileInfo info;
        info.name = "Epson P7500 " + p.name;
        info.manufacturer = "Epson";
        info.color_space = "RGB";
        info.connection_space = "Lab";
        info.pcs = "Lab";
        info.media_type = (p.abbr.find("Canvas") != std::string::npos) ? "Canvas" : "Photo Paper";
        info.finish = (p.abbr.find("Gloss") != std::string::npos) ? "Glossy" :
                     (p.abbr.find("Satin") != std::string::npos || p.abbr.find("Lust") != std::string::npos) ? "Satin" : "Matte";
        info.default_intent = "Relative";
        info.compatible_printers = {"Epson SureColor P7500"};
        info.supports_bw_mode = true;
        profiles_.push_back(info);
    }
}

void BuiltInProfileLibrary::add_epson_p9500_profiles() {
    struct ProfileDef { std::string abbr; std::string name; };
    std::vector<ProfileDef> profiles = {
        {"PGP-44", "Photo Paper Glossy 44in"},
        {"PLM-44", "Premium Luster 44in"},
        {"PSP-44", "Premium Semigloss 44in"},
        {"AMM-44", "Archival Matte 44in"},
        {"HPB-44", "Hot Press Bright 44in"},
        {"FAB-44", "Fine Art Baryta 44in"},
        {"CAN-44", "Canvas Natural 44in"},
        {"CAS-44", "Canvas Satin 44in"},
        {"PPG-44", "Production Glossy 44in"},
        {"PSL-44", "Production Luster 44in"},
        {"FAP-44", "Fine Art Pearl 44in"},
        {"FAM-44", "Fine Art Manga 44in"},
    };

    for (const auto& p : profiles) {
        ICCProfileInfo info;
        info.name = "Epson P9500 " + p.name;
        info.manufacturer = "Epson";
        info.color_space = "RGB";
        info.connection_space = "Lab";
        info.pcs = "Lab";
        info.media_type = (p.abbr.find("Canvas") != std::string::npos) ? "Canvas" :
                        (p.abbr.find("Fine Art") != std::string::npos) ? "Fine Art" : "Photo Paper";
        info.finish = (p.abbr.find("Gloss") != std::string::npos) ? "Glossy" :
                     (p.abbr.find("Satin") != std::string::npos || p.abbr.find("Pearl") != std::string::npos) ? "Satin" : "Matte";
        info.default_intent = "Relative";
        info.compatible_printers = {"Epson SureColor P9500"};
        info.supports_bw_mode = true;
        info.is_wide_gamut = true;
        profiles_.push_back(info);
    }
}

void BuiltInProfileLibrary::add_common_profiles() {
    struct ProfileDef { std::string name; std::string mfr; std::string media; std::string finish; };

    // Hahnemühle profiles
    std::vector<ProfileDef> hahnemuellerProfiles = {
        {"Hahnemuhle Photo Rag 308", "Hahnemuhle", "Photo Rag", "Matte"},
        {"Hahnemuhle Photo Rag Ultra Smooth 305", "Hahnemuhle", "Photo Rag", "Satin"},
        {"Hahnemuhle Photo Rag Satin 310", "Hahnemuhle", "Photo Rag", "Satin"},
        {"Hahnemuhle Photo Rag Bright White 310", "Hahnemuhle", "Photo Rag", "Matte"},
        {"Hahnemuhle German Etching 310", "Hahnemuhle", "Fine Art", "Matte"},
        {"Hahnemuhle Rice Paper 100", "Hahnemuhle", "Specialty", "Matte"},
        {"Hahnemuhle Moulin Du Roy 310", "Hahnemuhle", "Fine Art", "Satin"},
        {"Hahnemuhle William Turner 310", "Hahnemuhle", "Fine Art", "Matte"},
        {"Hahnemuhle William Turner Rough 310", "Hahnemuhle", "Fine Art", "Matte"},
        {"Hahnemuhle Agave 350", "Hahnemuhle", "Fine Art", "Matte"},
        {"Hahnemuhle Hemp 330", "Hahnemuhle", "Fine Art", "Matte"},
        {"Hahnemuhle Bamboo 290", "Hahnemuhle", "Fine Art", "Matte"},
        {"Hahnemuhle Natural Line Bamboo 300", "Hahnemuhle", "Fine Art", "Matte"},
        {"Hahnemuhle Platinum Rag 300", "Hahnemuhle", "Fine Art", "Satin"},
        {"Hahnemuhle Baryta 325", "Hahnemuhle", "Fine Art", "Glossy"},
        {"Hahnemuhle Baryta Satin 310", "Hahnemuhle", "Fine Art", "Satin"},
        {"Hahnemuhle Photo Pearl 310", "Hahnemuhle", "Photo Paper", "Pearl"},
        {"Hahnemuhle Photo Matt Fiber 300", "Hahnemuhle", "Photo Paper", "Matte"},
    };

    for (const auto& p : hahnemuellerProfiles) {
        ICCProfileInfo info;
        info.name = p.name;
        info.manufacturer = p.mfr;
        info.color_space = "RGB";
        info.connection_space = "Lab";
        info.pcs = "Lab";
        info.media_type = p.media;
        info.finish = p.finish;
        info.default_intent = "Perceptual";
        info.rendering_intents = {"Perceptual", "Relative", "Saturation"};
        info.supports_bw_mode = true;
        profiles_.push_back(info);
    }

    // Canson profiles
    std::vector<ProfileDef> cansonProfiles = {
        {"Canson Infinity Platine Fibre RAG 310", "Canson", "Fine Art", "Glossy"},
        {"Canson Infinity Platine Baryta 310", "Canson", "Fine Art", "Glossy"},
        {"Canson Infinity Baryta Photographique 310", "Canson", "Fine Art", "Glossy"},
        {"Canson Infinity Edition Etching Rag 310", "Canson", "Fine Art", "Matte"},
        {"Canson Infinity Arches Velin BFK Rives 310", "Canson", "Fine Art", "Matte"},
        {"Canson Infinity Arches Aquarelle Rag 310", "Canson", "Fine Art", "Matte"},
        {"Canson Infinity Montval Aquarelle 300", "Canson", "Fine Art", "Matte"},
        {"Canson Infinity Mi-Teintes 320", "Canson", "Specialty", "Matte"},
        {"Canson Infinity 1000 Tags 250", "Canson", "Specialty", "Matte"},
        {"Canson Infinity Drawing 180", "Canson", "Specialty", "Matte"},
    };

    for (const auto& p : cansonProfiles) {
        ICCProfileInfo info;
        info.name = p.name;
        info.manufacturer = p.mfr;
        info.color_space = "RGB";
        info.connection_space = "Lab";
        info.media_type = p.media;
        info.finish = p.finish;
        info.default_intent = "Perceptual";
        info.supports_bw_mode = true;
        profiles_.push_back(info);
    }

    // Moab profiles
    std::vector<ProfileDef> moabProfiles = {
        {"Moab Moabber Los 100", "Moab", "Fine Art", "Matte"},
        {"Moab Slickrock Metallic 260", "Moab", "Photo Paper", "Metallic"},
        {"Moab Juniper Baryta 310", "Moab", "Fine Art", "Glossy"},
        {"Moab Charcoal Natural 310", "Moab", "Fine Art", "Matte"},
        {"Moab Bright White 230", "Moab", "Fine Art", "Matte"},
        {"Moab Desert Varnish Iron Hinge 255", "Moab", "Specialty", "Glossy"},
        {"Moab Maverick Matte Canvas 380", "Moab", "Canvas", "Matte"},
        {"Moab Maverick Luster Canvas 400", "Moab", "Canvas", "Satin"},
    };

    for (const auto& p : moabProfiles) {
        ICCProfileInfo info;
        info.name = p.name;
        info.manufacturer = p.mfr;
        info.color_space = "RGB";
        info.connection_space = "Lab";
        info.media_type = p.media;
        info.finish = p.finish;
        info.default_intent = "Perceptual";
        info.supports_bw_mode = true;
        profiles_.push_back(info);
    }

    // Epson Legacy profiles
    std::vector<ProfileDef> epsonLegacyProfiles = {
        {"Epson Legacy Satellite Matte 240", "Epson", "Photo Paper", "Matte"},
        {"Epson Legacy Satellite Pearl 260", "Epson", "Photo Paper", "Pearl"},
        {"Epson Legacy Baryta 325", "Epson", "Fine Art", "Glossy"},
        {"Epson Legacy Etching Rag 260", "Epson", "Fine Art", "Matte"},
        {"Epson Legacy Platine 255", "Epson", "Fine Art", "Glossy"},
        {"Epson Legacy Crystal Bragg 310", "Epson", "Fine Art", "Glossy"},
        {"Epson Legacy Fibre", "Epson", "Fine Art", "Satin"},
        {"Epson Legacy Metallic", "Epson", "Photo Paper", "Metallic"},
    };

    for (const auto& p : epsonLegacyProfiles) {
        ICCProfileInfo info;
        info.name = p.name;
        info.manufacturer = p.mfr;
        info.color_space = "RGB";
        info.connection_space = "Lab";
        info.media_type = p.media;
        info.finish = p.finish;
        info.default_intent = "Relative";
        info.supports_bw_mode = true;
        profiles_.push_back(info);
    }

    // Innova profiles
    std::vector<ProfileDef> innovaProfiles = {
        {"Innova FibaPrint Baryta 320", "Innova", "Fine Art", "Glossy"},
        {"Innova FibaPrint White Matte 285", "Innova", "Fine Art", "Matte"},
        {"Innova FibaPrint Warm Cotton Gloss 310", "Innova", "Fine Art", "Glossy"},
        {"Innova FibaPrint Ultra Gloss 300", "Innova", "Photo Paper", "Glossy"},
        {"Innova FibaPrint Rough Matte 300", "Innova", "Fine Art", "Matte"},
        {"Innova Photo Cotton Satin 300", "Innova", "Photo Paper", "Satin"},
        {"Innova Photo Cotton Natural 280", "Innova", "Photo Paper", "Matte"},
    };

    for (const auto& p : innovaProfiles) {
        ICCProfileInfo info;
        info.name = p.name;
        info.manufacturer = p.mfr;
        info.color_space = "RGB";
        info.connection_space = "Lab";
        info.media_type = p.media;
        info.finish = p.finish;
        info.default_intent = "Perceptual";
        info.supports_bw_mode = true;
        profiles_.push_back(info);
    }
}

void BuiltInProfileLibrary::add_bw_profiles() {
    struct BWProfile { std::string name; std::string mfr; std::string media; std::string finish; std::string ink; };
    std::vector<BWProfile> bwProfiles = {
        {"P700 B&W Photo", "Epson", "Photo Paper", "Matte", "PK/MK/LK/LLK"},
        {"P700 B&W Matte", "Epson", "Fine Art", "Matte", "MK only"},
        {"P900 B&W Photo", "Epson", "Photo Paper", "Matte", "PK/MK/LK/LLK"},
        {"P900 B&W Matte", "Epson", "Fine Art", "Matte", "MK only"},
        {"P9500 B&W Gloss", "Epson", "Photo Paper", "Glossy", "PK/MK/LK/LLK"},
        {"P9500 B&W Luster", "Epson", "Photo Paper", "Satin", "PK/MK/LK/LLK"},
        {"P9500 B&W Matte", "Epson", "Fine Art", "Matte", "PK/MK/LK/LLK"},
        {"Hahnemuhle Photo Rag B&W", "Hahnemuhle", "Fine Art", "Matte", "Multi-black"},
        {"Canson Platine B&W", "Canson", "Fine Art", "Glossy", "Multi-black"},
    };

    for (const auto& p : bwProfiles) {
        ICCProfileInfo info;
        info.name = p.name;
        info.manufacturer = p.mfr;
        info.color_space = "Gray";
        info.connection_space = "Lab";
        info.pcs = "Lab";
        info.media_type = p.media;
        info.finish = p.finish;
        info.default_intent = "Relative";
        info.is_gray = true;
        info.supports_bw_mode = true;
        profiles_.push_back(info);
    }
}

void BuiltInProfileLibrary::add_wide_gamut_profiles() {
    std::vector<std::string> wgProfiles = {
        "P9500 Wide Gamut Photo",
        "P9500 Wide Gamut Proofing",
        "P9500 Wide Gamut Marketing",
    };

    for (const std::string& name : wgProfiles) {
        ICCProfileInfo info;
        info.name = name;
        info.manufacturer = "Epson";
        info.color_space = "RGB";
        info.connection_space = "Lab";
        info.media_type = "Photo Paper";
        info.finish = "Glossy";
        info.default_intent = "Perceptual";
        info.is_wide_gamut = true;
        info.supports_bw_mode = true;
        profiles_.push_back(info);
    }
}

std::vector<ICCProfileInfo> BuiltInProfileLibrary::get_all_profiles() {
    return profiles_;
}

std::vector<ICCProfileInfo> BuiltInProfileLibrary::get_profiles_by_printer(const std::string& printer) {
    std::vector<ICCProfileInfo> result;
    for (const auto& profile : profiles_) {
        for (const auto& p : profile.compatible_printers) {
            if (p == printer) {
                result.push_back(profile);
                break;
            }
        }
    }
    return result;
}

std::vector<ICCProfileInfo> BuiltInProfileLibrary::get_profiles_by_media(const std::string& media) {
    std::vector<ICCProfileInfo> result;
    for (const auto& profile : profiles_) {
        if (string_contains(profile.media_type, media)) {
            result.push_back(profile);
        }
    }
    return result;
}

std::vector<ICCProfileInfo> BuiltInProfileLibrary::get_profiles_by_color_space(const std::string& color_space) {
    std::vector<ICCProfileInfo> result;
    for (const auto& profile : profiles_) {
        if (profile.color_space == color_space) {
            result.push_back(profile);
        }
    }
    return result;
}

ICCProfileInfo* BuiltInProfileLibrary::find_profile(const std::string& name) {
    for (auto& profile : profiles_) {
        if (profile.name == name) {
            return &profile;
        }
    }
    return nullptr;
}

ICCProfileInfo* BuiltInProfileLibrary::find_profile_by_md5(const std::string& md5) {
    for (auto& profile : profiles_) {
        if (!profile.profile_data.empty()) {
            std::string hash = ProfileValidator::compute_md5(profile.profile_data);
            if (hash == md5) {
                return &profile;
            }
        }
    }
    return nullptr;
}

std::vector<std::string> BuiltInProfileLibrary::get_printer_models() {
    std::vector<std::string> models;
    std::set<std::string> seen;
    for (const auto& profile : profiles_) {
        for (const auto& printer : profile.compatible_printers) {
            if (seen.find(printer) == seen.end()) {
                seen.insert(printer);
                models.push_back(printer);
            }
        }
    }
    return models;
}

std::vector<std::string> BuiltInProfileLibrary::get_media_types() {
    std::vector<std::string> types;
    std::set<std::string> seen;
    for (const auto& profile : profiles_) {
        if (seen.find(profile.media_type) == seen.end()) {
            seen.insert(profile.media_type);
            types.push_back(profile.media_type);
        }
    }
    return types;
}

std::vector<std::string> BuiltInProfileLibrary::get_color_spaces() {
    std::vector<std::string> spaces;
    std::set<std::string> seen;
    for (const auto& profile : profiles_) {
        if (seen.find(profile.color_space) == seen.end()) {
            seen.insert(profile.color_space);
            spaces.push_back(profile.color_space);
        }
    }
    return spaces;
}

std::vector<ProfileRecommendation> BuiltInProfileLibrary::recommend_profiles(
    const std::string& printer,
    const std::string& media,
    const std::string& color_space,
    const std::string& intent) {

    std::vector<ProfileRecommendation> recommendations;
    double base_confidence = 0.7;

    for (const auto& profile : profiles_) {
        double confidence = base_confidence;

        for (const auto& p : profile.compatible_printers) {
            if (p == printer) {
                confidence += 0.2;
                break;
            }
        }

        if (string_contains(profile.media_type, media)) {
            confidence += 0.1;
        }

        if (profile.color_space == color_space) {
            confidence += 0.1;
        }

        if (profile.default_intent == intent) {
            confidence += 0.05;
        }

        if (confidence > 0.6) {
            ProfileRecommendation rec;
            rec.profile_name = profile.name;
            rec.confidence = std::min(1.0, confidence);
            rec.reason = "Matches printer=" + printer + ", media=" + media;
            rec.suggested_intent = profile.default_intent;
            recommendations.push_back(rec);
        }
    }

    std::sort(recommendations.begin(), recommendations.end(),
              [](const ProfileRecommendation& a, const ProfileRecommendation& b) {
                  return a.confidence > b.confidence;
              });

    return recommendations;
}

bool BuiltInProfileLibrary::load_profile_data(ICCProfileInfo& info) {
    (void)info;
    return false;
}

std::vector<uint8_t> BuiltInProfileLibrary::get_profile_data(const std::string& name) {
    ICCProfileInfo* info = find_profile(name);
    if (info && !info->profile_data.empty()) {
        return info->profile_data;
    }
    return std::vector<uint8_t>();
}

// ProfileValidator Implementation
ProfileValidator::ValidationResult ProfileValidator::validate(const std::vector<uint8_t>& profile_data) {
    ValidationResult result;

    if (profile_data.empty()) {
        result.error_message = "Empty profile data";
        return result;
    }

    if (profile_data.size() < 128) {
        result.error_message = "Profile too small";
        return result;
    }

    result.is_valid = true;
    return result;
}

ProfileValidator::ValidationResult ProfileValidator::validate_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        ValidationResult result;
        result.error_message = "Cannot open file";
        return result;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    return validate(data);
}

bool ProfileValidator::is_gray_profile(const std::vector<uint8_t>& profile_data) {
    (void)profile_data;
    return false;
}

bool ProfileValidator::is_rgb_profile(const std::vector<uint8_t>& profile_data) {
    (void)profile_data;
    return true;
}

bool ProfileValidator::is_cmyk_profile(const std::vector<uint8_t>& profile_data) {
    (void)profile_data;
    return false;
}

std::string ProfileValidator::compute_md5(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    for (size_t i = 0; i < std::min(data.size(), size_t(16)); ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return oss.str();
}

// ProfileComparator Implementation
ProfileComparator::Comparison ProfileComparator::compare(const ICCProfileInfo& a,
                                                          const ICCProfileInfo& b) {
    Comparison result;
    result.gamut_volume_diff_percent = std::abs(estimate_gamut_volume(a) - estimate_gamut_volume(b));
    result.avg_delta_e = 0.5;
    return result;
}

double ProfileComparator::estimate_gamut_volume(const ICCProfileInfo& profile) {
    if (profile.color_space == "CMYK") return 120.0;
    if (profile.is_wide_gamut) return 180.0;
    return 100.0;
}

// ICCProfileGenerator Implementation
std::vector<uint8_t> ICCProfileGenerator::generate(const GeneratorParams& params) {
    (void)params;
    return std::vector<uint8_t>();
}

std::vector<uint8_t> ICCProfileGenerator::generate_srgb() {
    GeneratorParams params;
    params.name = "sRGB";
    params.color_space = "RGB";
    params.gamma = 2.2;
    return generate(params);
}

std::vector<uint8_t> ICCProfileGenerator::generate_adobergb() {
    GeneratorParams params;
    params.name = "Adobe RGB";
    params.color_space = "RGB";
    params.red_primary = {0.64, 0.33, 1.0};
    params.green_primary = {0.21, 0.72, 1.0};
    params.blue_primary = {0.15, 0.06, 1.0};
    params.gamma = 2.2;
    return generate(params);
}

std::vector<uint8_t> ICCProfileGenerator::generate_prophoto_rgb() {
    GeneratorParams params;
    params.name = "ProPhoto RGB";
    params.color_space = "RGB";
    params.gamma = 1.8;
    params.white_point = {0.9642, 1.0, 0.8249};
    return generate(params);
}

std::vector<uint8_t> ICCProfileGenerator::generate_wide_gamut_rgb() {
    GeneratorParams params;
    params.name = "Wide Gamut RGB";
    params.color_space = "RGB";
    return generate(params);
}

// ProfilePresets Implementation
ICCProfileInfo ProfilePresets::photo_glossy_standard() {
    ICCProfileInfo info;
    info.name = "Photo Glossy Standard";
    info.media_type = "Photo Paper";
    info.finish = "Glossy";
    info.default_intent = "Relative";
    return info;
}

ICCProfileInfo ProfilePresets::photo_luster_premium() {
    ICCProfileInfo info;
    info.name = "Photo Luster Premium";
    info.media_type = "Photo Paper";
    info.finish = "Satin";
    info.default_intent = "Relative";
    return info;
}

ICCProfileInfo ProfilePresets::fine_art_bright() {
    ICCProfileInfo info;
    info.name = "Fine Art Bright";
    info.media_type = "Fine Art";
    info.finish = "Matte";
    info.default_intent = "Perceptual";
    return info;
}

ICCProfileInfo ProfilePresets::fine_art_warm() {
    ICCProfileInfo info;
    info.name = "Fine Art Warm";
    info.media_type = "Fine Art";
    info.finish = "Matte";
    info.default_intent = "Perceptual";
    return info;
}

ICCProfileInfo ProfilePresets::canvas_natural() {
    ICCProfileInfo info;
    info.name = "Canvas Natural";
    info.media_type = "Canvas";
    info.finish = "Matte";
    info.default_intent = "Perceptual";
    return info;
}

ICCProfileInfo ProfilePresets::matte_paper_heavyweight() {
    ICCProfileInfo info;
    info.name = "Matte Paper Heavyweight";
    info.media_type = "Photo Paper";
    info.finish = "Matte";
    info.default_intent = "Relative";
    return info;
}

ICCProfileInfo ProfilePresets::bw_photo_um() {
    ICCProfileInfo info;
    info.name = "B&W Photo UM";
    info.color_space = "Gray";
    info.media_type = "Fine Art";
    info.finish = "Matte";
    info.is_gray = true;
    return info;
}

ICCProfileInfo ProfilePresets::bw_hifi_contrast() {
    ICCProfileInfo info;
    info.name = "B&W Hi-Fi Contrast";
    info.color_space = "Gray";
    info.media_type = "Photo Paper";
    info.finish = "Glossy";
    info.is_gray = true;
    return info;
}

}
}