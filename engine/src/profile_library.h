#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <set>

namespace jcolour {
namespace profiles {

struct ICCProfileInfo {
    std::string name;
    std::string manufacturer;
    std::string description;
    std::string version;
    std::string copyright;

    // Color space info
    std::string color_space;
    std::string connection_space;
    std::string pcs;

    // Rendering intents
    std::vector<std::string> rendering_intents;
    std::string default_intent;

    // Paper/media type
    std::string media_type;
    std::string finish;

    // ICC v2 or v4
    int icc_version_major = 4;
    int icc_version_minor = 0;

    // For built-in profiles, store the data
    std::vector<uint8_t> profile_data;

    // Profile classification
    enum class Category {
        Input,
        Display,
        Output,
        ColorSpace,
        Abstract,
        NamedColor
    };
    Category category = Category::Output;

    // Compatibility
    std::vector<std::string> compatible_printers;
    bool is_wide_gamut = false;
    bool is_gray = false;
    bool supports_bw_mode = false;
};

struct PrinterProfileSet {
    std::string printer_model;
    std::vector<std::string> media_compatibility;
    std::vector<ICCProfileInfo> profiles;
};

struct ProfileRecommendation {
    std::string profile_name;
    double confidence;
    std::string reason;
    std::string suggested_intent;
};

class BuiltInProfileLibrary {
public:
    BuiltInProfileLibrary();
    ~BuiltInProfileLibrary();

    static BuiltInProfileLibrary& instance();

    std::vector<ICCProfileInfo> get_all_profiles();
    std::vector<ICCProfileInfo> get_profiles_by_printer(const std::string& printer);
    std::vector<ICCProfileInfo> get_profiles_by_media(const std::string& media);
    std::vector<ICCProfileInfo> get_profiles_by_color_space(const std::string& color_space);

    ICCProfileInfo* find_profile(const std::string& name);
    ICCProfileInfo* find_profile_by_md5(const std::string& md5);

    std::vector<std::string> get_printer_models();
    std::vector<std::string> get_media_types();
    std::vector<std::string> get_color_spaces();

    std::vector<ProfileRecommendation> recommend_profiles(
        const std::string& printer,
        const std::string& media,
        const std::string& color_space,
        const std::string& intent);

    bool load_profile_data(ICCProfileInfo& info);
    std::vector<uint8_t> get_profile_data(const std::string& name);

private:
    void initialize_library();

    void add_epson_p700_profiles();
    void add_epson_p900_profiles();
    void add_epson_p7500_profiles();
    void add_epson_p9500_profiles();
    void add_common_profiles();
    void add_bw_profiles();
    void add_wide_gamut_profiles();

    std::vector<ICCProfileInfo> profiles_;
};

class ProfileValidator {
public:
    struct ValidationResult {
        bool is_valid = false;
        std::string error_message;
        std::string warning_message;
        std::string profile_class;
        std::string color_space;
        std::string profile_version;
        int pcs_channels = 0;
        bool is_embeddable = true;
        bool is_copyright_protected = false;
    };

    static ValidationResult validate(const std::vector<uint8_t>& profile_data);
    static ValidationResult validate_file(const std::string& path);

    static bool is_gray_profile(const std::vector<uint8_t>& profile_data);
    static bool is_rgb_profile(const std::vector<uint8_t>& profile_data);
    static bool is_cmyk_profile(const std::vector<uint8_t>& profile_data);

    static std::string compute_md5(const std::vector<uint8_t>& data);
};

class ProfileComparator {
public:
    struct Comparison {
        double gamut_volume_diff_percent = 0.0;
        double avg_delta_e = 0.0;
        std::string largest_difference_color;
        std::vector<std::string> differences;
    };

    static Comparison compare(const ICCProfileInfo& a, const ICCProfileInfo& b);
    static double estimate_gamut_volume(const ICCProfileInfo& profile);
};

class ProfileOptimizer {
public:
    struct OptimizationParams {
        double target_gamut_coverage = 95.0;
        int max_iterations = 100;
        bool preserve_skin_tones = true;
        bool preserve_neutrals = true;
        bool compress_shadows = false;
        bool compress_highlights = false;
    };

    static std::vector<uint8_t> optimize(
        const std::vector<uint8_t>& source_profile,
        const OptimizationParams& params);
};

class ICCProfileGenerator {
public:
    struct GeneratorParams {
        std::string name;
        std::string manufacturer;
        std::string color_space = "RGB";
        std::string pcs = "Lab";
        int version_major = 4;
        int version_minor = 2;

        std::string illuminant = "D50";

        std::array<double, 3> red_primary = {0.64, 0.33, 1.0};
        std::array<double, 3> green_primary = {0.21, 0.71, 1.0};
        std::array<double, 3> blue_primary = {0.15, 0.06, 1.0};
        std::array<double, 3> white_point = {0.9642, 1.0, 0.8249};
        std::array<double, 3> black_point = {0.0, 0.0, 0.0};

        int trc_type = 0;
        double gamma = 2.2;
        std::vector<double> parametric_params;

        std::string rendering_intent = "Perceptual";
    };

    static std::vector<uint8_t> generate(const GeneratorParams& params);
    static std::vector<uint8_t> generate_srgb();
    static std::vector<uint8_t> generate_adobergb();
    static std::vector<uint8_t> generate_prophoto_rgb();
    static std::vector<uint8_t> generate_wide_gamut_rgb();
};

class ProfilePresets {
public:
    static ICCProfileInfo photo_glossy_standard();
    static ICCProfileInfo photo_luster_premium();
    static ICCProfileInfo fine_art_bright();
    static ICCProfileInfo fine_art_warm();
    static ICCProfileInfo canvas_natural();
    static ICCProfileInfo matte_paper_heavyweight();
    static ICCProfileInfo bw_photo_um();
    static ICCProfileInfo bw_hifi_contrast();
};

}
}