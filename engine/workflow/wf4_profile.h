#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <map>

namespace jcolour {
namespace workflow {

// ECI2002/IT8.7 target format for Argyll
struct TI1Data {
    std::string filename;
    std::vector<std::string> patch_names;
    std::vector<float> rgb_values;
};

struct TI3Data {
    std::string filename;
    std::vector<std::string> patch_names;
    std::vector<float> lab_values;  // [L1, a1, b1, L2, a2, b2, ...]
};

// Workflow 4 ICC profile
struct WF4Profile {
    std::string name;
    std::vector<uint8_t> icc_blob;
    double avg_delta_e = 0.0;
    double max_delta_e = 0.0;
};

// Profile verification result
struct ProfileCheckResult {
    double avg_delta_e = 0.0;
    double max_delta_e = 0.0;
    int patches_tested = 0;
    bool passed = false;
};

// Argyll generation options
struct ArgyllTargetOptions {
    int patch_count = 1500;
    int lut_size = 33;
    bool include_extended_inks = true;
};

// Argyll profile generation options
struct ArgyllProfileOptions {
    std::string profile_name = "jcolour";
    std::string manufacturer = "jcolour";
    std::string device = "P9580";
    char quality = 'h';  // h=high, m=medium, l=low
    int lut_size = 33;
    bool perceptual_table = true;
    bool relcol_table = true;
    bool saturation_table = true;
};

// Generate TI1 test chart file using Argyll targen
TI1Data generate_ti1_file(const std::string& output_path, int num_patches = 1500);

// Parse TI3 measurement file
TI3Data parse_ti3_file(const std::string& input_path);

// Write TI3 file for Argyll colprof
bool write_ti3_file(const TI3Data& data, const std::string& output_path);

// Generate ICC profile using Argyll
WF4Profile generate_wf4_icc_profile(const TI3Data& measurements,
                                     const std::string& profile_name,
                                     const std::string& work_dir);

// Generate ICC profile using Argyll with options
WF4Profile generate_wf4_icc_profile(const TI3Data& measurements,
                                     const std::string& profile_name,
                                     const std::string& work_dir,
                                     const ArgyllProfileOptions& opts);

// Generate TI1 using Argyll targen
TI1Data generate_wf4_target(const std::string& work_dir,
                            const ArgyllTargetOptions& opts = {});

// Profile verification result
ProfileCheckResult verify_wf4_profile(const WF4Profile& profile,
                                      const TI3Data& reference);

// Verify profile using Argyll profcheck
ProfileCheckResult verify_wf4_profile(const std::string& icc_path,
                                       const std::string& ti3_path);

}  // namespace workflow
}  // namespace jcolour