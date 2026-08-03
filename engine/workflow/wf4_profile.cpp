#include "wf4_profile.h"
#include "../argyll/argyll.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <sys/stat.h>

namespace jcolour {
namespace workflow {

static bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

TI1Data generate_ti1_file(const std::string& output_path, int num_patches) {
    TI1Data data;
    data.filename = output_path;

    for (int i = 0; i < num_patches; ++i) {
        data.patch_names.push_back("P" + std::to_string(i + 1));
    }

    std::vector<float> rgb;
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    // Generate 17^3 grid (4913 patches max)
    for (int r = 0; r <= 16; r += 2) {
        for (int g = 0; g <= 16; g += 2) {
            for (int b = 0; b <= 16; b += 2) {
                rgb.push_back(r / 16.0f);
                rgb.push_back(g / 16.0f);
                rgb.push_back(b / 16.0f);
            }
        }
    }

    // Pad with random if needed
    while (rgb.size() < static_cast<size_t>(num_patches * 3)) {
        rgb.push_back(dist(gen));
        rgb.push_back(dist(gen));
        rgb.push_back(dist(gen));
    }

    rgb.resize(num_patches * 3);
    data.rgb_values = rgb;

    std::ofstream out(output_path);
    if (!out) return data;

    out << "CTI1\n";
    out << "DEVICE_CLASS OUTPUT\n";
    out << "MANUFACTURER jcolour\n";
    out << "MODEL P9580\n";
    out << "NUMBER_OF_SETS " << num_patches << "\n";
    out << "NUMBER_OF_FIELDS 4\n";
    out << "BEGIN_DATA_FORMAT\n";
    out << "RGB_I RGB_R RGB_G RGB_B\n";
    out << "END_DATA_FORMAT\n";
    out << "BEGIN_DATA\n";

    for (int i = 0; i < num_patches; ++i) {
        int idx = i * 3;
        out << (i + 1) << " " << std::fixed << std::setprecision(6)
            << rgb[idx] << " " << rgb[idx + 1] << " " << rgb[idx + 2] << "\n";
    }

    out << "END_DATA\n";
    return data;
}

TI3Data parse_ti3_file(const std::string& input_path) {
    TI3Data data;
    data.filename = input_path;

    std::ifstream in(input_path);
    if (!in) return data;

    std::string line;
    bool in_data = false;

    while (std::getline(in, line)) {
        if (line.find("BEGIN_DATA") != std::string::npos) {
            in_data = true;
            continue;
        }
        if (line.find("END_DATA") != std::string::npos) {
            break;
        }

        if (in_data && !line.empty()) {
            std::istringstream ss(line);
            std::string patch_name;
            float lab_l, lab_a, lab_b;

            if (ss >> patch_name >> lab_l >> lab_a >> lab_b) {
                data.patch_names.push_back(patch_name);
                data.lab_values.push_back(lab_l);
                data.lab_values.push_back(lab_a);
                data.lab_values.push_back(lab_b);
            }
        }
    }

    return data;
}

bool write_ti3_file(const TI3Data& data, const std::string& output_path) {
    std::ofstream out(output_path);
    if (!out) return false;

    int num_patches = static_cast<int>(data.patch_names.size());
    if (num_patches == 0) return false;

    out << "CTI3\n";
    out << "DEVICE_CLASS OUTPUT\n";
    out << "MANUFACTURER jcolour\n";
    out << "MODEL P9580\n";
    out << "CREATED \"jcolour v3\"\n";
    out << "KEYWORD \"DEVICE\"\n";
    out << "\"P9580\"\n";
    out << "KEYWORD \"TARGET_INSTRUMENT\"\n";
    out << "\"i1Pro3\"\n";
    out << "NUMBER_OF_SETS " << num_patches << "\n";
    out << "NUMBER_OF_FIELDS 3\n";
    out << "BEGIN_DATA_FORMAT\n";
    out << "LAB_ID LAB_L LAB_A LAB_B\n";
    out << "END_DATA_FORMAT\n";
    out << "BEGIN_DATA\n";

    for (size_t i = 0; i < data.patch_names.size(); ++i) {
        size_t idx = i * 3;
        if (idx + 2 < data.lab_values.size()) {
            out << data.patch_names[i] << " "
                << std::fixed << std::setprecision(6)
                << data.lab_values[idx] << " "
                << data.lab_values[idx + 1] << " "
                << data.lab_values[idx + 2] << "\n";
        }
    }

    out << "END_DATA\n";
    return true;
}

TI1Data generate_wf4_target(const std::string& work_dir,
                           const ArgyllTargetOptions& opts) {
    TI1Data result;

    // Build targen arguments
    std::vector<std::string> args;
    args.push_back("-d5");  // Output device class (display/emulator/printer)
    args.push_back("-G");   // Generate gamut boundary patches too
    args.push_back("-g" + std::to_string(opts.lut_size));  // LUT grid size
    args.push_back("-f" + std::to_string(opts.patch_count));  // Number of patches

    if (opts.include_extended_inks) {
        args.push_back("-e4");  // Include 4 channel extreme patches (CMYK)
    }

    // Output base path (without extension)
    std::string output_base = work_dir + "/wf4_target";
    args.push_back(output_base);

    // Run targen
    auto run_result = argyll::targen(output_base, args, 120);

    if (run_result.exit_code != 0) {
        // Fall back to manual TI1 generation
        return generate_ti1_file(work_dir + "/wf4_target.ti1", opts.patch_count);
    }

    // Parse generated TI1
    std::string ti1_path = output_base + ".ti1";
    if (file_exists(ti1_path)) {
        result.filename = ti1_path;
        // Read patch count from file
        std::ifstream in(ti1_path);
        std::string line;
        while (std::getline(in, line)) {
            if (line.find("NUMBER_OF_SETS") != std::string::npos) {
                std::istringstream ss(line);
                std::string keyword;
                int count;
                ss >> keyword >> count;
                result.patch_names.resize(count);
                for (int i = 0; i < count; ++i) {
                    result.patch_names[i] = "P" + std::to_string(i + 1);
                }
                break;
            }
        }
        if (result.patch_names.empty()) {
            result.patch_names.resize(opts.patch_count);
            for (int i = 0; i < opts.patch_count; ++i) {
                result.patch_names[i] = "P" + std::to_string(i + 1);
            }
        }
    }

    return result;
}

WF4Profile generate_wf4_icc_profile(const TI3Data& measurements,
                                     const std::string& profile_name,
                                     const std::string& work_dir) {
    ArgyllProfileOptions opts;
    opts.profile_name = profile_name;
    return generate_wf4_icc_profile(measurements, profile_name, work_dir, opts);
}

WF4Profile generate_wf4_icc_profile(const TI3Data& measurements,
                                     const std::string& profile_name,
                                     const std::string& work_dir,
                                     const ArgyllProfileOptions& opts) {
    WF4Profile profile;
    profile.name = profile_name;

    // Write TI3 file
    std::string ti3_path = work_dir + "/" + profile_name + ".ti3";
    if (!write_ti3_file(measurements, ti3_path)) {
        return profile;
    }

    // Build colprof arguments
    std::vector<std::string> args;
    args.push_back("-v");   // Verbose
    args.push_back("-A" + opts.manufacturer);  // Manufacturer
    args.push_back("-D" + opts.profile_name);  // Description
    args.push_back("-M" + opts.device);         // Model
    args.push_back(std::string("-q") + opts.quality);  // Quality (h/m/l)

    if (opts.perceptual_table) {
        args.push_back("-pp");  // Include perceptual table
    }
    if (opts.relcol_table) {
        args.push_back("-pr");  // Include relative colorimetric
    }
    if (opts.saturation_table) {
        args.push_back("-ps");  // Include saturation
    }

    // Output base path (without extension)
    std::string output_base = work_dir + "/" + profile_name;
    args.push_back(output_base);

    // Run colprof
    auto run_result = argyll::colprof(output_base, args, 600);

    // Read generated ICC file
    std::string icc_path = output_base + ".icc";
    if (file_exists(icc_path)) {
        std::ifstream icc_file(icc_path, std::ios::binary);
        if (icc_file) {
            icc_file.seekg(0, std::ios::end);
            size_t size = static_cast<size_t>(icc_file.tellg());
            profile.icc_blob.resize(size);
            icc_file.seekg(0, std::ios::beg);
            icc_file.read(reinterpret_cast<char*>(profile.icc_blob.data()), size);
        }
    }

    // Run profcheck to get accuracy
    auto check_result = verify_wf4_profile(icc_path, ti3_path);
    profile.avg_delta_e = check_result.avg_delta_e;
    profile.max_delta_e = check_result.max_delta_e;

    return profile;
}

ProfileCheckResult verify_wf4_profile(const WF4Profile& profile,
                                      const TI3Data& reference) {
    ProfileCheckResult result;
    result.patches_tested = static_cast<int>(reference.patch_names.size());
    result.avg_delta_e = profile.avg_delta_e;
    result.max_delta_e = profile.max_delta_e;
    result.passed = (profile.avg_delta_e < 1.5 && profile.max_delta_e < 4.0);
    return result;
}

ProfileCheckResult verify_wf4_profile(const std::string& icc_path,
                                       const std::string& ti3_path) {
    ProfileCheckResult result;

    if (!file_exists(icc_path) || !file_exists(ti3_path)) {
        return result;
    }

    auto run_result = argyll::profcheck(icc_path, ti3_path, {"-v"}, 120);

    if (run_result.exit_code == 0 || run_result.exit_code == 1) {
        // Parse output for delta E values
        // Output format: "profile_check: CIE94 DE 0.459 (patches 12, failed 0)"
        std::string output = run_result.stdout_out + run_result.stderr_out;

        // Look for "DE" value
        size_t de_pos = output.find("DE");
        if (de_pos != std::string::npos) {
            std::istringstream ss(output.substr(de_pos + 2));
            ss >> result.avg_delta_e;
        }

        // Look for "failed" count
        size_t fail_pos = output.find("failed");
        if (fail_pos != std::string::npos) {
            // Max DE could be extracted from failure details
        }

        result.passed = (result.avg_delta_e < 1.5);
    }

    return result;
}

}  // namespace workflow
}  // namespace jcolour