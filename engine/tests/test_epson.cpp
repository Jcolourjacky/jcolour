#include <catch2/catch_test_macros.hpp>
#include "epson.h"

using namespace jcolour::epson;

TEST_CASE("Printer: construction does not throw", "[epson]") {
    REQUIRE_NOTHROW(Printer("192.168.1.100"));
    REQUIRE_NOTHROW(Printer("192.168.1.100", 9100));
}

TEST_CASE("Printer: is_connected defaults false", "[epson]") {
    Printer p("192.168.1.100");
    REQUIRE(p.is_connected() == false);
}

TEST_CASE("Printer: connect against unreachable port returns false fast", "[epson]") {
    // 127.0.0.1:1 is reserved/unused → kernel responds with RST immediately.
    Printer p("127.0.0.1", 1);
    REQUIRE(p.connect() == false);
    REQUIRE(p.is_connected() == false);
}

TEST_CASE("Printer: disconnect when not connected does not throw", "[epson]") {
    Printer p("127.0.0.1", 1);
    REQUIRE_NOTHROW(p.disconnect());
}

TEST_CASE("Printer: query_status against unreachable host returns error message", "[epson]") {
    Printer p("127.0.0.1", 1);
    auto s = p.query_status();
    REQUIRE(s.is_ready == false);
    REQUIRE(!s.error_message.empty());
    REQUIRE(s.error_message != "ok");
}

TEST_CASE("Printer: read_cartridges returns empty (not implemented)", "[epson]") {
    Printer p("127.0.0.1", 1);
    auto carts = p.read_cartridges();
    REQUIRE(carts.empty());
}

TEST_CASE("Printer: read_cartridges(batch_id) returns empty (not implemented)", "[epson]") {
    Printer p("127.0.0.1", 1);
    auto carts = p.read_cartridges("BATCH-001");
    REQUIRE(carts.empty());
}

TEST_CASE("Printer: nozzle_check fails gracefully on unreachable host", "[epson]") {
    Printer p("127.0.0.1", 1);
    auto nozzles = p.nozzle_check();
    REQUIRE(nozzles.empty());
}

TEST_CASE("Printer: clean_head fails gracefully on unreachable host", "[epson]") {
    Printer p("127.0.0.1", 1);
    REQUIRE(p.clean_head() == false);
    REQUIRE(p.clean_head(CleanLevel::Heavy) == false);
}

TEST_CASE("Printer: set_black_overcoat fails gracefully on unreachable host", "[epson]") {
    Printer p("127.0.0.1", 1);
    REQUIRE(p.set_black_overcoat(true) == false);
    REQUIRE(p.set_black_overcoat(false) == false);
}

TEST_CASE("Printer: send_raw_data on unreachable host returns false", "[epson]") {
    Printer p("127.0.0.1", 1);
    std::vector<uint8_t> data = {0x1B, 0x40};
    REQUIRE(p.send_raw_data(data) == false);
}

TEST_CASE("Printer: send_raw_data with empty buffer returns false", "[epson]") {
    Printer p("127.0.0.1", 1);
    REQUIRE(p.send_raw_data({}) == false);
}

TEST_CASE("InkCartridge: default values", "[epson]") {
    InkCartridge c;
    REQUIRE(c.slot == -1);
    REQUIRE(c.level_pct == -1);
    REQUIRE(c.batch_id.empty());
}

TEST_CASE("NozzleStatus: default values", "[epson]") {
    NozzleStatus n;
    REQUIRE(n.has_clog == false);
    REQUIRE(n.missing_nozzles == 0);
}

TEST_CASE("PrinterStatus: default values", "[epson]") {
    PrinterStatus s;
    REQUIRE(s.is_ready == false);
    REQUIRE(s.media_width_mm == 0);
    REQUIRE(s.cartridges.empty());
    REQUIRE(s.nozzles.empty());
}

TEST_CASE("discover_printers: returns vector (empty when no printers)", "[epson][discovery]") {
    // Without a real printer on the network, expect empty or near-empty result
    auto printers = discover_printers();
    // Just verify it returns without crashing; the actual list depends on network
    (void)printers;
}

TEST_CASE("DiscoveredPrinter: default values", "[epson][discovery]") {
    DiscoveredPrinter p;
    REQUIRE(p.name.empty());
    REQUIRE(p.host.empty());
    REQUIRE(p.port == 9100);
}
