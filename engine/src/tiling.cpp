#include "tiling.h"
#include <cmath>
#include <algorithm>
#include <sstream>

namespace jcolour {
namespace tiling {

static constexpr double MM_TO_INCH = 1.0 / 25.4;

static int mm_to_pixels(double mm, double dpi) {
    return static_cast<int>(mm * MM_TO_INCH * dpi);
}

static PixelRGBA blend_pixels(const PixelRGBA& a, const PixelRGBA& b, double alpha) {
    PixelRGBA result;
    result.r = static_cast<uint8_t>(a.r * alpha + b.r * (1.0 - alpha));
    result.g = static_cast<uint8_t>(a.g * alpha + b.g * (1.0 - alpha));
    result.b = static_cast<uint8_t>(a.b * alpha + b.b * (1.0 - alpha));
    result.a = 255;
    return result;
}

static void draw_horizontal_line(PixelRGBA* pixels, int width, int height, int x, int y, int len, const PixelRGBA& color) {
    for (int i = 0; i < len; ++i) {
        if (x + i >= 0 && x + i < width && y >= 0 && y < height) {
            pixels[y * width + (x + i)] = color;
        }
    }
}

static void draw_vertical_line(PixelRGBA* pixels, int width, int height, int x, int y, int len, const PixelRGBA& color) {
    for (int i = 0; i < len; ++i) {
        if (x >= 0 && x < width && y + i >= 0 && y + i < height) {
            pixels[(y + i) * width + x] = color;
        }
    }
}

TilingEngine::TilingEngine() {}
TilingEngine::~TilingEngine() {}

void TilingEngine::set_config(const TileConfig& config) {
    config_ = config;
}

TilingResult TilingEngine::tile_image(
    const ImageBuffer& source,
    double source_width_mm,
    double source_height_mm) {

    TilingResult result;
    result.original_width_mm = source_width_mm;
    result.original_height_mm = source_height_mm;

    double dpi_x = source.width / (source_width_mm * MM_TO_INCH);
    double dpi_y = source.height / (source_height_mm * MM_TO_INCH);

    int tile_width_px = mm_to_pixels(config_.tile_width_mm, dpi_x);
    int tile_height_px = mm_to_pixels(config_.tile_height_mm, dpi_y);
    int overlap_px = mm_to_pixels(config_.overlap_mm, dpi_x);

    int step_x = tile_width_px - overlap_px;
    int step_y = tile_height_px - overlap_px;

    // Handle edge case where tile is smaller than overlap
    if (step_x <= 0) step_x = 1;
    if (step_y <= 0) step_y = 1;

    result.total_tiles_x = static_cast<int>(
        std::ceil((source.width - overlap_px) / static_cast<double>(step_x)));
    result.total_tiles_y = static_cast<int>(
        std::ceil((source.height - overlap_px) / static_cast<double>(step_y)));

    for (int ty = 0; ty < result.total_tiles_y; ++ty) {
        for (int tx = 0; tx < result.total_tiles_x; ++tx) {
            ImageBuffer tile = extract_tile(source, tx, ty);

            if (config_.add_crop_marks) {
                bool corners[4] = {
                    tx == 0 && ty == 0,
                    tx == result.total_tiles_x - 1 && ty == 0,
                    tx == 0 && ty == result.total_tiles_y - 1,
                    tx == result.total_tiles_x - 1 && ty == result.total_tiles_y - 1
                };
                tile = add_crop_marks(tile, config_.crop_mark_length,
                                      config_.crop_mark_offset, corners);
            }

            result.tiles.push_back(tile);

            double region_x = tx * step_x;
            double region_y = ty * step_y;
            result.tile_regions.push_back({region_x, region_y,
                                           static_cast<double>(tile.width),
                                           static_cast<double>(tile.height)});
        }
    }

    return result;
}

ImageBuffer TilingEngine::extract_tile(
    const ImageBuffer& source,
    int tile_x, int tile_y) {

    double dpi = source.width / (config_.tile_width_mm * MM_TO_INCH);
    int tile_width_px = mm_to_pixels(config_.tile_width_mm, dpi);
    int tile_height_px = mm_to_pixels(config_.tile_height_mm, dpi);
    int overlap_px = mm_to_pixels(config_.overlap_mm, dpi);

    int step_x = tile_width_px - overlap_px;
    int step_y = tile_height_px - overlap_px;

    int x = tile_x * step_x;
    int y = tile_y * step_y;

    int right = std::min(x + tile_width_px, source.width);
    int bottom = std::min(y + tile_height_px, source.height);

    int crop_w = right - x;
    int crop_h = bottom - y;

    ImageBuffer tile;
    tile.resize(crop_w > 0 ? crop_w : 1, crop_h > 0 ? crop_h : 1);

    for (int row = 0; row < tile.height; ++row) {
        for (int col = 0; col < tile.width; ++col) {
            int src_x = x + col;
            int src_y = y + row;
            if (src_x >= 0 && src_x < source.width && src_y >= 0 && src_y < source.height) {
                tile.at(col, row) = source.at(src_x, src_y);
            }
        }
    }

    return tile;
}

std::vector<RectF> TilingEngine::calculate_tile_regions(
    double source_width, double source_height,
    double tile_width, double tile_height,
    double overlap_mm) {

    std::vector<RectF> regions;
    double step_x = tile_width - overlap_mm;
    double step_y = tile_height - overlap_mm;

    for (double y = 0; y < source_height; y += step_y) {
        for (double x = 0; x < source_width; x += step_x) {
            double w = std::min(tile_width, source_width - x);
            double h = std::min(tile_height, source_height - y);
            regions.push_back({x, y, w, h});
        }
    }

    return regions;
}

ImageBuffer TilingEngine::add_crop_marks(
    const ImageBuffer& tile,
    double crop_mark_length,
    double crop_mark_offset,
    bool draw_corners[4]) {

    ImageBuffer marked = tile;

    double dpi = 300;
    int mark_len = mm_to_pixels(crop_mark_length, dpi);
    int offset = mm_to_pixels(crop_mark_offset, dpi);

    PixelRGBA black{0, 0, 0, 255};

    // Top-left corner
    if (draw_corners[0] || (!draw_corners[1] && !draw_corners[2])) {
        draw_horizontal_line(marked.data(), marked.width, marked.height,
                             0, offset, mark_len, black);
        draw_vertical_line(marked.data(), marked.width, marked.height,
                           offset, 0, mark_len, black);
    }

    // Top-right corner
    if (draw_corners[1] || (!draw_corners[0] && !draw_corners[3])) {
        int x = marked.width - offset;
        draw_vertical_line(marked.data(), marked.width, marked.height,
                           x, 0, mark_len, black);
        draw_horizontal_line(marked.data(), marked.width, marked.height,
                             marked.width - mark_len, offset, mark_len, black);
    }

    // Bottom-left corner
    if (draw_corners[2] || (!draw_corners[0] && !draw_corners[3])) {
        int y = marked.height - offset;
        draw_horizontal_line(marked.data(), marked.width, marked.height,
                             0, y, mark_len, black);
        draw_vertical_line(marked.data(), marked.width, marked.height,
                           offset, marked.height - mark_len, mark_len, black);
    }

    // Bottom-right corner
    if (draw_corners[3] || (!draw_corners[1] && !draw_corners[2])) {
        int x = marked.width - offset;
        int y = marked.height - offset;
        draw_vertical_line(marked.data(), marked.width, marked.height,
                           x, marked.height - mark_len, mark_len, black);
        draw_horizontal_line(marked.data(), marked.width, marked.height,
                             marked.width - mark_len, y, mark_len, black);
    }

    return marked;
}

ImageBuffer TilingEngine::blend_overlap(
    const ImageBuffer& left_tile,
    const ImageBuffer& right_tile,
    int overlap_pixels) {

    if (overlap_pixels <= 0) return right_tile;

    ImageBuffer blended = right_tile;

    for (int y = 0; y < blended.height; ++y) {
        for (int x = 0; x < overlap_pixels && x < blended.width; ++x) {
            int left_x = left_tile.width - overlap_pixels + x;
            if (left_x >= 0 && left_x < left_tile.width && y < left_tile.height) {
                double alpha = x / static_cast<double>(overlap_pixels);
                blended.at(x, y) = blend_pixels(left_tile.at(left_x, y),
                                                right_tile.at(x, y), alpha);
            }
        }
    }

    return blended;
}

// StepRepeatEngine implementation
StepRepeatEngine::StepRepeatEngine() {}
StepRepeatEngine::~StepRepeatEngine() {}

void StepRepeatEngine::set_config(const StepRepeatConfig& config) {
    config_ = config;
}

StepRepeatResult StepRepeatEngine::create_repeat(
    const ImageBuffer& source,
    double source_width_mm,
    double source_height_mm) {

    StepRepeatResult result;
    result.total_copies = config_.columns * config_.rows;

    result.total_width_mm = source_width_mm * config_.columns +
                            config_.spacing_x_mm * (config_.columns - 1);
    result.total_height_mm = source_height_mm * config_.rows +
                             config_.spacing_y_mm * (config_.rows - 1);

    double dpi = source.width / (source_width_mm * MM_TO_INCH);

    int width_px = static_cast<int>(result.total_width_mm * MM_TO_INCH * dpi);
    int height_px = static_cast<int>(result.total_height_mm * MM_TO_INCH * dpi);

    result.combined_image.resize(width_px, height_px);
    result.combined_image.fill(PixelRGBA{255, 255, 255, 255});

    PixelRGBA* pixels = result.combined_image.data();

    for (int row = 0; row < config_.rows; ++row) {
        for (int col = 0; col < config_.columns; ++col) {
            int x = static_cast<int>((source_width_mm * col +
                                      config_.spacing_x_mm * col) * MM_TO_INCH * dpi);
            int y = static_cast<int>((source_height_mm * row +
                                      config_.spacing_y_mm * row) * MM_TO_INCH * dpi);

            for (int sy = 0; sy < source.height; ++sy) {
                for (int sx = 0; sx < source.width; ++sx) {
                    int dest_x = x + sx;
                    int dest_y = y + sy;
                    if (dest_x < result.combined_image.width &&
                        dest_y < result.combined_image.height) {
                        pixels[dest_y * result.combined_image.width + dest_x] =
                            source.at(sx, sy);
                    }
                }
            }
        }
    }

    return result;
}

ImageBuffer StepRepeatEngine::create_contact_sheet(
    const std::vector<ImageBuffer>& images,
    int columns,
    double spacing_mm,
    double label_height_mm) {

    if (images.empty()) return ImageBuffer{};

    double dpi = 72;
    int spacing_px = mm_to_pixels(spacing_mm, dpi);
    int label_px = mm_to_pixels(label_height_mm, dpi);

    int img_width = images[0].width;
    int img_height = images[0].height;

    int rows = static_cast<int>((images.size() + columns - 1) / columns);

    int total_width = columns * img_width + (columns + 1) * spacing_px;
    int total_height = rows * (img_height + label_px) + (rows + 1) * spacing_px;

    ImageBuffer sheet;
    sheet.resize(total_width, total_height);
    sheet.fill(PixelRGBA{255, 255, 255, 255});

    for (size_t i = 0; i < images.size(); ++i) {
        int col = static_cast<int>(i) % columns;
        int row = static_cast<int>(i) / columns;

        int x = spacing_px + col * (img_width + spacing_px);
        int y = spacing_px + row * (img_height + label_px + spacing_px);

        for (int sy = 0; sy < images[i].height; ++sy) {
            for (int sx = 0; sx < images[i].width; ++sx) {
                int dest_x = x + sx;
                int dest_y = y + sy;
                if (dest_x < sheet.width && dest_y < sheet.height) {
                    sheet.at(dest_x, dest_y) = images[i].at(sx, sy);
                }
            }
        }
    }

    return sheet;
}

ImageBuffer StepRepeatEngine::create_grid(
    const ImageBuffer& tile,
    int columns,
    int rows,
    double spacing_pixels) {

    int spacing = static_cast<int>(spacing_pixels);
    int tile_width = tile.width;
    int tile_height = tile.height;

    int total_width = columns * tile_width + (columns + 1) * spacing;
    int total_height = rows * tile_height + (rows + 1) * spacing;

    ImageBuffer grid;
    grid.resize(total_width, total_height);
    grid.fill(PixelRGBA{255, 255, 255, 255});

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < columns; ++c) {
            int x = spacing + c * (tile_width + spacing);
            int y = spacing + r * (tile_height + spacing);

            for (int sy = 0; sy < tile.height; ++sy) {
                for (int sx = 0; sx < tile.width; ++sx) {
                    int dest_x = x + sx;
                    int dest_y = y + sy;
                    if (dest_x < grid.width && dest_y < grid.height) {
                        grid.at(dest_x, dest_y) = tile.at(sx, sy);
                    }
                }
            }
        }
    }

    return grid;
}

std::string StepRepeatEngine::generate_tile_label(int index, const std::string& prefix) {
    std::ostringstream oss;
    oss << prefix << (index + 1);
    return oss.str();
}

std::string StepRepeatEngine::int_to_letter(int num) {
    std::string result;
    while (num >= 0) {
        result = static_cast<char>('A' + (num % 26)) + result;
        num = num / 26 - 1;
        if (num < 0) break;
    }
    return result;
}

// LargeFormatSplitter implementation
LargeFormatSplitter::LargeFormatSplitter() {}
LargeFormatSplitter::~LargeFormatSplitter() {}

void LargeFormatSplitter::set_params(const SplitParams& params) {
    params_ = params;
}

LargeFormatSplitter::SplitResult LargeFormatSplitter::split_for_printer(
    const ImageBuffer& source,
    double source_width_mm,
    double source_height_mm,
    double printer_max_width_mm,
    double printer_max_height_mm) {

    SplitResult result;
    result.overlap_mm = params_.overlap_mm;

    double dpi = source.width / (source_width_mm * MM_TO_INCH);

    int printer_width_px = mm_to_pixels(printer_max_width_mm, dpi);
    int printer_height_px = mm_to_pixels(printer_max_height_mm, dpi);
    int overlap_px = mm_to_pixels(params_.overlap_mm, dpi);

    auto seams = calculate_seams(
        source.width, source.height,
        printer_width_px, printer_height_px,
        overlap_px, false);

    for (size_t i = 0; i < seams.size(); ++i) {
        const auto& rect = seams[i];

        ImageBuffer panel;
        int w = static_cast<int>(rect.width);
        int h = static_cast<int>(rect.height);
        panel.resize(w > 0 ? w : 1, h > 0 ? h : 1);

        for (int py = 0; py < panel.height; ++py) {
            for (int px = 0; px < panel.width; ++px) {
                int src_x = static_cast<int>(rect.x) + px;
                int src_y = static_cast<int>(rect.y) + py;
                if (src_x >= 0 && src_x < source.width &&
                    src_y >= 0 && src_y < source.height) {
                    panel.at(px, py) = source.at(src_x, src_y);
                }
            }
        }

        bool is_first = (i == 0);
        bool is_last = (i == seams.size() - 1);
        bool is_vertical = rect.width < rect.height;

        panel = add_seam_marks(panel, is_vertical, is_first, is_last);

        result.panels.push_back(panel);
        result.source_regions.push_back(rect);
        result.panel_labels.push_back(std::to_string(static_cast<int>(i) + 1));
    }

    result.total_panels = static_cast<int>(result.panels.size());
    return result;
}

std::vector<RectF> LargeFormatSplitter::calculate_seams(
    double total_width,
    double total_height,
    double max_panel_width,
    double max_panel_height,
    double overlap,
    bool vertical_only) {

    std::vector<RectF> panels;

    if (vertical_only) {
        double x = 0;
        while (x < total_width) {
            double w = std::min(max_panel_width, total_width - x);
            panels.push_back({x, 0, w, total_height});
            x += w - overlap;
        }
    } else {
        double y = 0;
        while (y < total_height) {
            double h = std::min(max_panel_height, total_height - y);
            double x = 0;
            while (x < total_width) {
                double w = std::min(max_panel_width, total_width - x);
                panels.push_back({x, y, w, h});
                x += w - overlap;
            }
            y += h - overlap;
        }
    }

    return panels;
}

ImageBuffer LargeFormatSplitter::add_seam_marks(
    const ImageBuffer& panel,
    bool is_vertical_seam,
    bool is_first,
    bool is_last) {

    ImageBuffer marked = panel;

    int mark_len = 30;
    PixelRGBA red{255, 0, 0, 255};

    if (is_vertical_seam) {
        if (!is_first) {
            int x = 10;
            draw_vertical_line(marked.data(), marked.width, marked.height,
                               x, mark_len, mark_len, red);
            draw_horizontal_line(marked.data(), marked.width, marked.height,
                                 x, 2 * mark_len, mark_len, red);
        }
        if (!is_last) {
            int x = panel.width - 10;
            draw_vertical_line(marked.data(), marked.width, marked.height,
                               x, mark_len, mark_len, red);
            draw_horizontal_line(marked.data(), marked.width, marked.height,
                                 panel.width - mark_len, 2 * mark_len, mark_len, red);
        }
    } else {
        if (!is_first) {
            int y = 10;
            draw_horizontal_line(marked.data(), marked.width, marked.height,
                                 mark_len, y, mark_len, red);
            draw_vertical_line(marked.data(), marked.width, marked.height,
                               2 * mark_len, y, mark_len, red);
        }
        if (!is_last) {
            int y = panel.height - 10;
            draw_horizontal_line(marked.data(), marked.width, marked.height,
                                 mark_len, y, mark_len, red);
            draw_vertical_line(marked.data(), marked.width, marked.height,
                               2 * mark_len, y - mark_len, mark_len, red);
        }
    }

    return marked;
}

PanelAssembler::PanelAssembler() {}
PanelAssembler::~PanelAssembler() {}

ImageBuffer PanelAssembler::assemble_panels(
    const std::vector<ImageBuffer>& panels,
    const std::vector<RectF>& regions,
    double overlap_mm) {

    if (panels.empty() || regions.empty()) return ImageBuffer{};

    double max_x = 0, max_y = 0;
    for (const auto& r : regions) {
        max_x = std::max(max_x, r.x + r.width);
        max_y = std::max(max_y, r.y + r.height);
    }

    ImageBuffer assembled;
    assembled.resize(static_cast<int>(max_x), static_cast<int>(max_y));
    assembled.fill(PixelRGBA{255, 255, 255, 255});

    for (size_t i = 0; i < panels.size() && i < regions.size(); ++i) {
        int offset_x = static_cast<int>(regions[i].x);
        int offset_y = static_cast<int>(regions[i].y);

        for (int y = 0; y < panels[i].height; ++y) {
            for (int x = 0; x < panels[i].width; ++x) {
                int dest_x = offset_x + x;
                int dest_y = offset_y + y;
                if (dest_x < assembled.width && dest_y < assembled.height) {
                    assembled.at(dest_x, dest_y) = panels[i].at(x, y);
                }
            }
        }
    }

    return assembled;
}

ImageBuffer PanelAssembler::blend_seam(
    const ImageBuffer& panel_a,
    const ImageBuffer& panel_b,
    int seam_position,
    int blend_width) {

    ImageBuffer result = panel_b;

    for (int y = 0; y < result.height; ++y) {
        for (int x = seam_position - blend_width; x < seam_position; ++x) {
            if (x >= 0 && x < result.width && x < panel_a.width) {
                double alpha = (x - (seam_position - blend_width)) / static_cast<double>(blend_width);
                result.at(x, y) = blend_pixels(panel_a.at(x, y), panel_b.at(x, y), alpha);
            }
        }
    }

    return result;
}

}
}