#include <catch2/catch_test_macros.hpp>
#include "tiling.h"

using namespace jcolour::tiling;

TEST_CASE("[tiling] ImageBuffer basic operations", "[tiling]") {
    ImageBuffer img;
    img.resize(100, 100);

    REQUIRE(img.width == 100);
    REQUIRE(img.height == 100);
    REQUIRE(img.pixel_count() == 10000);
}

TEST_CASE("[tiling] ImageBuffer fill and access", "[tiling]") {
    ImageBuffer img;
    img.resize(10, 10);

    PixelRGBA blue{0, 0, 255, 255};
    img.fill(blue);

    const PixelRGBA& pixel = img.at(5, 5);
    REQUIRE(pixel.r == 0);
    REQUIRE(pixel.g == 0);
    REQUIRE(pixel.b == 255);
    REQUIRE(pixel.a == 255);
}

TEST_CASE("[tiling] TilingEngine tile_image", "[tiling]") {
    TilingEngine engine;
    TileConfig config;
    config.tile_width_mm = 50.0;
    config.tile_height_mm = 50.0;
    config.overlap_mm = 0.0;
    config.add_crop_marks = false;
    engine.set_config(config);

    ImageBuffer source;
    source.resize(200, 200);

    auto result = engine.tile_image(source, 200.0, 200.0);

    REQUIRE(result.total_tiles_x > 0);
    REQUIRE(result.total_tiles_y > 0);
    REQUIRE(result.tiles.size() > 0);
}

TEST_CASE("[tiling] TilingEngine extract_tile", "[tiling]") {
    TilingEngine engine;
    TileConfig config;
    config.tile_width_mm = 100.0;
    config.tile_height_mm = 100.0;
    engine.set_config(config);

    ImageBuffer source;
    source.resize(300, 300);

    for (int y = 0; y < 300; ++y) {
        for (int x = 0; x < 300; ++x) {
            source.at(x, y) = PixelRGBA{(uint8_t)x, (uint8_t)y, 0, 255};
        }
    }

    auto tile = engine.extract_tile(source, 0, 0);
    REQUIRE(tile.width > 0);
    REQUIRE(tile.height > 0);
}

TEST_CASE("[tiling] TilingEngine calculate_tile_regions", "[tiling]") {
    auto regions = TilingEngine::calculate_tile_regions(200.0, 200.0, 100.0, 100.0, 0.0);

    REQUIRE(regions.size() > 0);
}

TEST_CASE("[tiling] StepRepeatEngine create_repeat", "[tiling]") {
    StepRepeatEngine engine;
    StepRepeatConfig config;
    config.columns = 3;
    config.rows = 2;
    config.spacing_x_mm = 5.0;
    config.spacing_y_mm = 5.0;
    config.add_labels = false;
    engine.set_config(config);

    ImageBuffer source;
    source.resize(50, 50);  // Smaller for faster testing

    auto result = engine.create_repeat(source, 50.0, 50.0);

    REQUIRE(result.total_copies == 6);
    REQUIRE(result.combined_image.width > 50);
    REQUIRE(result.combined_image.height > 50);
}

TEST_CASE("[tiling] StepRepeatEngine generate_tile_label", "[tiling]") {
    auto label = StepRepeatEngine::generate_tile_label(0, "A");
    REQUIRE(label == "A1");

    label = StepRepeatEngine::generate_tile_label(25, "B");
    REQUIRE(label == "B26");
}

TEST_CASE("[tiling] LargeFormatSplitter split_for_printer", "[tiling]") {
    LargeFormatSplitter splitter;
    LargeFormatSplitter::SplitParams params;
    params.max_width_mm = 200.0;
    params.max_height_mm = 300.0;
    params.overlap_mm = 10.0;
    splitter.set_params(params);

    ImageBuffer source;
    source.resize(600, 400);

    auto result = splitter.split_for_printer(source, 600.0, 400.0, 200.0, 300.0);

    REQUIRE(result.total_panels > 0);
    REQUIRE(result.panels.size() > 0);
}

TEST_CASE("[tiling] LargeFormatSplitter calculate_seams", "[tiling]") {
    auto seams = LargeFormatSplitter::calculate_seams(600.0, 400.0, 200.0, 300.0, 10.0, false);

    REQUIRE(seams.size() > 0);
}

TEST_CASE("[tiling] PanelAssembler assemble_panels", "[tiling]") {
    std::vector<ImageBuffer> panels;
    std::vector<RectF> regions;

    ImageBuffer panel1;
    panel1.resize(100, 100);
    panel1.fill(PixelRGBA{255, 0, 0, 255});
    panels.push_back(panel1);
    regions.push_back({0, 0, 100, 100});

    ImageBuffer panel2;
    panel2.resize(100, 100);
    panel2.fill(PixelRGBA{0, 255, 0, 255});
    panels.push_back(panel2);
    regions.push_back({100, 0, 100, 100});

    auto assembled = PanelAssembler::assemble_panels(panels, regions, 10.0);

    REQUIRE(assembled.width >= 200);
    REQUIRE(assembled.height >= 100);
}

TEST_CASE("[tiling] RectF initialization", "[tiling]") {
    RectF rect{10.0, 20.0, 100.0, 200.0};
    REQUIRE(rect.x == 10.0);
    REQUIRE(rect.y == 20.0);
    REQUIRE(rect.width == 100.0);
    REQUIRE(rect.height == 200.0);
}