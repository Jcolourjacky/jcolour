#pragma once

#include "spectral.h"

namespace jcolour {

struct DeltaEResult {
    double de00 = 0.0;
    double de94 = 0.0;
    double de76 = 0.0;
    bool valid = false;
    const char* error = nullptr;
};

class ColorPipeline {
public:
    struct Config {
        bool use_36_channel = true;
        bool preserve_colorimetric_accuracy = true;
        int rendering_intent = 1;
        bool use_bpc = true;
        double adaptation_state = 1.0;
    };

    ColorPipeline();  // default config
    ColorPipeline(const Config& config);
    ~ColorPipeline();

    void set_source_profile(const char* profile_path);
    void set_destination_profile(const char* profile_path);

    std::vector<uint16_t> transform_tiff(const std::vector<uint16_t>& input,
                                         int width, int height, int channels);

    DeltaEResult measure_accuracy(const std::vector<uint16_t>& input,
                                  int width, int height,
                                  const char* reference_tiff);

    struct Stats {
        double max_de00;
        double avg_de00;
        double gray_avg_de00;
        double skin_avg_de00;
        double saturated_avg_de00;
        double pct_within_1_0;
    };
    Stats get_stats() const { return stats_; }

private:
    struct Impl;
    Impl* pImpl_;
    Config config_;
    Stats stats_{};
};

}