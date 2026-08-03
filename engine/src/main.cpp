#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <getopt.h>
#include <chrono>
#include "color_pipeline.h"
#include "tiff_io.h"

namespace {
struct Options {
    std::string input_path;
    std::string output_path;
    std::string source_profile;
    std::string dest_profile;
    std::string reference_path;
    int rendering_intent = 1;
    bool use_bpc = true;
    bool verbose = false;
    bool show_stats = false;
};

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [OPTIONS]\n"
              << "  -i, --input FILE       Input TIFF file\n"
              << "  -o, --output FILE      Output TIFF file\n"
              << "  -s, --source FILE     Source ICC profile\n"
              << "  -d, --dest FILE       Destination ICC profile\n"
              << "  -r, --reference FILE  Reference TIFF for accuracy measurement\n"
              << "  -I, --intent INT      Rendering intent (0=Perceptual, 1=Relative, 2=Saturation, 3=Absolute)\n"
              << "  --no-bpc               Disable black point compensation\n"
              << "  -v, --verbose         Verbose output\n"
              << "  --stats                Show per-region accuracy stats\n"
              << "  -h, --help             Show this help\n";
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    constexpr struct option long_opts[] = {
        {"input", required_argument, nullptr, 'i'},
        {"output", required_argument, nullptr, 'o'},
        {"source", required_argument, nullptr, 's'},
        {"dest", required_argument, nullptr, 'd'},
        {"reference", required_argument, nullptr, 'r'},
        {"intent", required_argument, nullptr, 'I'},
        {"no-bpc", no_argument, nullptr, 1},
        {"verbose", no_argument, nullptr, 'v'},
        {"stats", no_argument, nullptr, 2},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    Options opts;
    int opt;
    while ((opt = getopt_long(argc, argv, "i:o:s:d:r:I:vh", long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'i': opts.input_path = optarg; break;
            case 'o': opts.output_path = optarg; break;
            case 's': opts.source_profile = optarg; break;
            case 'd': opts.dest_profile = optarg; break;
            case 'r': opts.reference_path = optarg; break;
            case 'I': opts.rendering_intent = std::atoi(optarg); break;
            case 1: opts.use_bpc = false; break;
            case 'v': opts.verbose = true; break;
            case 2: opts.show_stats = true; break;
            case 'h': print_usage(argv[0]); return 0;
            default: print_usage(argv[0]); return 1;
        }
    }

    if (opts.input_path.empty() || opts.output_path.empty()) {
        std::cerr << "Error: --input and --output are required\n";
        return 1;
    }

    if (opts.verbose) {
        std::cout << "jcolour-convert v0.1.0\n"
                  << "Input: " << opts.input_path << "\n"
                  << "Output: " << opts.output_path << "\n"
                  << "Source profile: " << (opts.source_profile.empty() ? "(none)" : opts.source_profile) << "\n"
                  << "Dest profile: " << (opts.dest_profile.empty() ? "(none)" : opts.dest_profile) << "\n"
                  << "Intent: " << opts.rendering_intent << "\n"
                  << "BPC: " << (opts.use_bpc ? "on" : "off") << "\n";
    }

    auto pipeline = jcolour::ColorPipeline(
        jcolour::ColorPipeline::Config{
            .use_36_channel = true,
            .preserve_colorimetric_accuracy = true,
            .rendering_intent = opts.rendering_intent,
            .use_bpc = opts.use_bpc
        }
    );

    if (!opts.source_profile.empty()) {
        pipeline.set_source_profile(opts.source_profile.c_str());
    }
    if (!opts.dest_profile.empty()) {
        pipeline.set_destination_profile(opts.dest_profile.c_str());
    }

    if (opts.verbose) {
        std::cout << "Reading input TIFF...\n";
    }
    auto image = jcolour::io::TIFFReader::read(opts.input_path);
    if (image.data.empty()) {
        std::cerr << "Error: Failed to read input TIFF\n";
        return 1;
    }

    if (opts.verbose) {
        std::cout << "Image: " << image.width << "x" << image.height
                  << ", " << image.bits_per_sample << "-bit, "
                  << image.samples_per_pixel << " channels\n"
                  << "Transforming...\n";
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto output = pipeline.transform_tiff(
        image.data, image.width, image.height, image.samples_per_pixel);
    auto end = std::chrono::high_resolution_clock::now();

    if (opts.verbose) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Transform completed in " << duration.count() << " ms\n";
    }

    if (opts.verbose) {
        std::cout << "Writing output TIFF...\n";
    }
    jcolour::io::TIFFImage out_image = image;
    out_image.data = std::move(output);
    jcolour::io::TIFFReader::write(opts.output_path, out_image);

    if (!opts.reference_path.empty()) {
        if (opts.verbose) {
            std::cout << "Measuring accuracy against reference...\n";
        }
        auto result = pipeline.measure_accuracy(
            out_image.data, out_image.width, out_image.height,
            opts.reference_path.c_str());

        auto stats = pipeline.get_stats();

        std::cout << "\n=== Accuracy Report ===\n"
                  << "Max ΔE2000: " << stats.max_de00 << "\n"
                  << "Avg ΔE2000: " << stats.avg_de00 << "\n"
                  << "Within 0.5: " << (stats.gray_avg_de00 <= 0.5 ? "PASS" : "FAIL") << "\n";

        if (stats.max_de00 > 3.0) {
            std::cerr << "\n!!! RED LINE: Max ΔE2000 > 3.0 - fundamental pipeline issue detected\n";
            return 1;
        }
    }

    if (opts.verbose) {
        std::cout << "Done.\n";
    }

    return 0;
}
