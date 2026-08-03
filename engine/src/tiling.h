#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace jcolour {
namespace tiling {

struct PixelRGBA {
    uint8_t r = 0, g = 0, b = 0, a = 255;
};

struct ImageBuffer {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;  // RGBA packed

    size_t pixel_count() const { return width * height; }
    size_t byte_size() const { return pixel_count() * sizeof(PixelRGBA); }

    PixelRGBA* data() { return reinterpret_cast<PixelRGBA*>(pixels.data()); }
    const PixelRGBA* data() const { return reinterpret_cast<const PixelRGBA*>(pixels.data()); }

    PixelRGBA& at(int x, int y) { return data()[y * width + x]; }
    const PixelRGBA& at(int x, int y) const { return data()[y * width + x]; }

    void resize(int w, int h) {
        width = w;
        height = h;
        pixels.resize(byte_size());
    }

    void fill(const PixelRGBA& color) {
        std::fill(data(), data() + pixel_count(), color);
    }
};

struct RectF {
    double x = 0, y = 0, width = 0, height = 0;
};

struct TileConfig {
    double tile_width_mm = 300.0;
    double tile_height_mm = 300.0;
    double overlap_mm = 0.0;
    int overlap_percent = 0;
    bool add_crop_marks = true;
    double crop_mark_length = 10.0;
    double crop_mark_offset = 3.0;
    int overlap_mode = 0;  // 0=cut, 1=blend
};

struct TilingResult {
    std::vector<ImageBuffer> tiles;
    std::vector<RectF> tile_regions;
    int total_tiles_x = 0;
    int total_tiles_y = 0;
    double original_width_mm = 0.0;
    double original_height_mm = 0.0;
};

struct StepRepeatConfig {
    int columns = 1;
    int rows = 1;
    double spacing_x_mm = 10.0;
    double spacing_y_mm = 10.0;
    bool add_labels = true;
    std::string label_text;
};

struct StepRepeatResult {
    ImageBuffer combined_image;
    int total_copies = 0;
    double total_width_mm = 0.0;
    double total_height_mm = 0.0;
};

class TilingEngine {
public:
    TilingEngine();
    ~TilingEngine();

    void set_config(const TileConfig& config);
    TileConfig get_config() const { return config_; }

    TilingResult tile_image(
        const ImageBuffer& source,
        double source_width_mm,
        double source_height_mm);

    ImageBuffer extract_tile(
        const ImageBuffer& source,
        int tile_x, int tile_y);

    static std::vector<RectF> calculate_tile_regions(
        double source_width, double source_height,
        double tile_width, double tile_height,
        double overlap_mm);

    static ImageBuffer add_crop_marks(
        const ImageBuffer& tile,
        double crop_mark_length,
        double crop_mark_offset,
        bool draw_corners[4]);

    static ImageBuffer blend_overlap(
        const ImageBuffer& left_tile,
        const ImageBuffer& right_tile,
        int overlap_pixels);

private:
    TileConfig config_;
};

class StepRepeatEngine {
public:
    StepRepeatEngine();
    ~StepRepeatEngine();

    void set_config(const StepRepeatConfig& config);
    StepRepeatConfig get_config() const { return config_; }

    StepRepeatResult create_repeat(
        const ImageBuffer& source,
        double source_width_mm,
        double source_height_mm);

    static ImageBuffer create_contact_sheet(
        const std::vector<ImageBuffer>& images,
        int columns,
        double spacing_mm,
        double label_height_mm = 0);

    static ImageBuffer create_grid(
        const ImageBuffer& tile,
        int columns,
        int rows,
        double spacing_pixels);

    static std::string generate_tile_label(int index, const std::string& prefix = "A");

    static std::string int_to_letter(int num);

private:
    StepRepeatConfig config_;
};

class LargeFormatSplitter {
public:
    LargeFormatSplitter();
    ~LargeFormatSplitter();

    struct SplitParams {
        double max_width_mm = 600.0;
        double max_height_mm = 900.0;
        double overlap_mm = 10.0;
        bool optimize_for_image = true;
        int seam_position = 0;
    };

    struct SplitResult {
        std::vector<ImageBuffer> panels;
        std::vector<RectF> source_regions;
        std::vector<std::string> panel_labels;
        int total_panels = 0;
        double overlap_mm = 0.0;
    };

    void set_params(const SplitParams& params);
    SplitParams get_params() const { return params_; }

    SplitResult split_for_printer(
        const ImageBuffer& source,
        double source_width_mm,
        double source_height_mm,
        double printer_max_width_mm,
        double printer_max_height_mm);

    static std::vector<RectF> calculate_seams(
        double total_width,
        double total_height,
        double max_panel_width,
        double max_panel_height,
        double overlap,
        bool vertical_only = false);

    static ImageBuffer add_seam_marks(
        const ImageBuffer& panel,
        bool is_vertical_seam,
        bool is_first,
        bool is_last);

private:
    SplitParams params_;
};

class PanelAssembler {
public:
    PanelAssembler();
    ~PanelAssembler();

    static ImageBuffer assemble_panels(
        const std::vector<ImageBuffer>& panels,
        const std::vector<RectF>& regions,
        double overlap_mm);

    static ImageBuffer blend_seam(
        const ImageBuffer& panel_a,
        const ImageBuffer& panel_b,
        int seam_position,
        int blend_width);
};

class RegistrationMarks {
public:
    static ImageBuffer add_registration_marks(
        const ImageBuffer& image,
        double mark_size_mm = 10.0,
        double margin_mm = 20.0);

    static ImageBuffer add_overlap_indicators(
        const ImageBuffer& image,
        double overlap_mm,
        const std::string& panel_id);
};

}
}