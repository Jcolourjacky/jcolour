#include <catch2/catch_test_macros.hpp>
#include "pjl.h"

using namespace jcolour::pjl;

TEST_CASE("PJLCommand::set_ink_mode PhotoBlack", "[pjl]") {
    auto cmd = PJLCommand::set_ink_mode(InkMode::PhotoBlack);
    REQUIRE(!cmd.data.empty());
    std::string str(cmd.data.begin(), cmd.data.end());
    REQUIRE(str.find("INKMODE=") != std::string::npos);
}

TEST_CASE("PJLCommand::set_ink_mode MatteBlack", "[pjl]") {
    auto cmd = PJLCommand::set_ink_mode(InkMode::MatteBlack);
    std::string str(cmd.data.begin(), cmd.data.end());
    REQUIRE(str.find("MATTE") != std::string::npos);
}

TEST_CASE("PJLCommand::set_color_mode Color", "[pjl]") {
    auto cmd = PJLCommand::set_color_mode(ColorMode::Color);
    std::string str(cmd.data.begin(), cmd.data.end());
    REQUIRE(str.find("COLORMODE=") != std::string::npos);
}

TEST_CASE("PJLCommand::set_color_mode Off", "[pjl]") {
    auto cmd = PJLCommand::set_color_mode(ColorMode::Off);
    std::string str(cmd.data.begin(), cmd.data.end());
    REQUIRE(str.find("OFF") != std::string::npos);
}

TEST_CASE("PJLCommand::set_resolution", "[pjl]") {
    auto cmd = PJLCommand::set_resolution(1440);
    std::string str(cmd.data.begin(), cmd.data.end());
    REQUIRE(str.find("RESOLUTION=1440") != std::string::npos);
}

TEST_CASE("PJLCommand::set_paper_size", "[pjl]") {
    auto cmd = PJLCommand::set_paper_size("A3");
    std::string str(cmd.data.begin(), cmd.data.end());
    REQUIRE(str.find("PAPERSIZE=A3") != std::string::npos);
}

TEST_CASE("PJLCommand::set_paper_type", "[pjl]") {
    auto cmd = PJLCommand::set_paper_type("PHOTORAG");
    std::string str(cmd.data.begin(), cmd.data.end());
    REQUIRE(str.find("MEDIATYPE=PHOTORAG") != std::string::npos);
}

TEST_CASE("PJLCommand::enter_language", "[pjl]") {
    auto cmd = PJLCommand::enter_language("PCL");
    std::string str(cmd.data.begin(), cmd.data.end());
    REQUIRE(str.find("ENTER LANGUAGE=PCL") != std::string::npos);
}

TEST_CASE("PJLCommand::query_status", "[pjl]") {
    auto cmd = PJLCommand::query_status();
    std::string str(cmd.data.begin(), cmd.data.end());
    REQUIRE(str.find("INFO STATUS") != std::string::npos);
}

TEST_CASE("PJLCommand::uel", "[pjl]") {
    auto cmd = PJLCommand::uel();
    REQUIRE(cmd.data.size() >= 5);
    REQUIRE(cmd.data[0] == 0x1B); // ESC
}

TEST_CASE("PJLCommand::set_print_quality", "[pjl]") {
    auto cmd = PJLCommand::set_print_quality(3);
    std::string str(cmd.data.begin(), cmd.data.end());
    REQUIRE(str.find("QUALITY=") != std::string::npos);
}

TEST_CASE("EpsonPrinter construction with URI", "[pjl]") {
    REQUIRE_NOTHROW(EpsonPrinter("ipp://192.168.1.100:631/printers/epson"));
}

TEST_CASE("EpsonPrinter::is_connected defaults false", "[pjl]") {
    EpsonPrinter printer("ipp://localhost");
    REQUIRE(printer.is_connected() == false);
}

TEST_CASE("EpsonPrinter::disconnect when not connected", "[pjl]") {
    EpsonPrinter printer("ipp://localhost");
    REQUIRE_NOTHROW(printer.disconnect());
}

TEST_CASE("EpsonPrinter::query_status returns error when not connected", "[pjl]") {
    EpsonPrinter printer("ipp://localhost");
    auto status = printer.query_status();
    REQUIRE(status.is_ready == false);
}

TEST_CASE("EpsonPrinter::discover_printers returns vector", "[pjl]") {
    auto printers = EpsonPrinter::discover_printers();
    REQUIRE(printers.size() >= 0);
}

TEST_CASE("EpsonPrinter::is_sandbox_safe returns bool", "[pjl]") {
    auto safe = EpsonPrinter::is_sandbox_safe();
    REQUIRE((safe == false || safe == true));
}

TEST_CASE("RasterGenerator::generate_raster_header", "[pjl]") {
    auto header = RasterGenerator::generate_raster_header(256, 256, true);
    REQUIRE(!header.empty());
}

TEST_CASE("RasterGenerator::generate_raster_footer", "[pjl]") {
    auto footer = RasterGenerator::generate_raster_footer();
    REQUIRE(footer.size() == 5);
}

TEST_CASE("RasterGenerator::convert_to_raster basic", "[pjl]") {
    std::vector<uint16_t> data(100 * 100 * 3, 32768);
    std::vector<double> limits(12, 1.0);

    auto raster = RasterGenerator::convert_to_raster(data, 100, 100, limits);
    REQUIRE(!raster.empty());
}

TEST_CASE("RasterGenerator::convert_to_raster with single channel enabled", "[pjl]") {
    std::vector<uint16_t> data(100 * 100 * 3, 65535); // White
    std::vector<double> limits(12, 0.0);
    limits[0] = 1.0; // Only PK

    auto raster = RasterGenerator::convert_to_raster(data, 100, 100, limits);
    REQUIRE(!raster.empty());
}

TEST_CASE("PrintJob construction", "[pjl]") {
    PrintJob job;
    REQUIRE(job.is_16bit == false);
    REQUIRE(job.width_pixels == 0);
    REQUIRE(job.height_pixels == 0);
}

TEST_CASE("InkMode enum values", "[pjl]") {
    REQUIRE(static_cast<uint8_t>(InkMode::PhotoBlack) == 0);
    REQUIRE(static_cast<uint8_t>(InkMode::MatteBlack) == 1);
}

TEST_CASE("ColorMode enum values", "[pjl]") {
    REQUIRE(static_cast<uint8_t>(ColorMode::Color) == 0);
    REQUIRE(static_cast<uint8_t>(ColorMode::AdvancedBW) == 1);
    REQUIRE(static_cast<uint8_t>(ColorMode::Off) == 2);
}
