#include <catch2/catch_test_macros.hpp>
#include "argyll.h"
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <sstream>

using namespace jcolour::argyll;

TEST_CASE("find_tool: unknown tool returns empty string", "[argyll]") {
    REQUIRE(find_tool("__no_such_argyll_tool__").empty());
}

TEST_CASE("find_tool: /bin/echo is found via PATH", "[argyll]") {
    // /bin/echo is always present on macOS; tests that PATH walk works
    std::string p = find_tool("echo");
    // May be in /bin or /usr/bin — just check it's non-empty and executable
    REQUIRE(!p.empty());
}

TEST_CASE("run: captures stdout from /bin/echo", "[argyll]") {
    std::string echo = find_tool("echo");
    REQUIRE(!echo.empty());

    auto r = run(echo, {"hello", "world"});
    REQUIRE(r.exit_code == 0);
    REQUIRE(r.stdout_out.find("hello world") != std::string::npos);
}

TEST_CASE("run: captures stderr via shell redirect", "[argyll]") {
    // Use /bin/sh -c to write to stderr
    std::string sh = find_tool("sh");
    REQUIRE(!sh.empty());

    auto r = run(sh, {"-c", "echo errline >&2"});
    REQUIRE(r.exit_code == 0);
    REQUIRE(r.stderr_out.find("errline") != std::string::npos);
}

TEST_CASE("run: non-zero exit code is captured", "[argyll]") {
    std::string sh = find_tool("sh");
    REQUIRE(!sh.empty());

    auto r = run(sh, {"-c", "exit 42"});
    REQUIRE(r.exit_code == 42);
}

TEST_CASE("run: timeout kills long-running process", "[argyll]") {
    std::string sleep_bin = find_tool("sleep");
    REQUIRE(!sleep_bin.empty());

    auto r = run(sleep_bin, {"10"}, 1);
    REQUIRE(r.timed_out == true);
}

TEST_CASE("version: returns string or empty (not installed is ok)", "[argyll]") {
    // Just verify it doesn't crash and returns a string
    std::string v = version();
    // If Argyll is installed, version should be non-empty; otherwise empty is fine
    (void)v;
    SUCCEED();
}

TEST_CASE("targen: returns exit_code -2 when not installed", "[argyll]") {
    std::string path = find_tool("targen");
    if (!path.empty()) {
        SKIP("targen is installed — skipping not-found test");
    }
    auto r = targen("/tmp/jcolour_test_demo", {"-d", "5", "-G", "-g", "11", "-f", "50"});
    REQUIRE(r.exit_code == -2);
    REQUIRE(r.stderr_out.find("not found") != std::string::npos);
}

TEST_CASE("colprof: returns exit_code -2 when not installed", "[argyll]") {
    std::string path = find_tool("colprof");
    if (!path.empty()) {
        SKIP("colprof is installed — skipping not-found test");
    }
    auto r = colprof("/tmp/jcolour_test_profile");
    REQUIRE(r.exit_code == -2);
}

TEST_CASE("profcheck: returns exit_code -2 when not installed", "[argyll]") {
    std::string path = find_tool("profcheck");
    if (!path.empty()) {
        SKIP("profcheck is installed — skipping not-found test");
    }
    auto r = profcheck("/tmp/test.icc", "/tmp/test.ti3");
    REQUIRE(r.exit_code == -2);
}

TEST_CASE("targen: produces ti1 file when installed", "[argyll][integration]") {
    std::string path = find_tool("targen");
    if (path.empty()) {
        SKIP("targen not installed");
    }

    std::string base = "/tmp/jcolour_a18_test";
    // Remove any leftover files
    std::remove((base + ".ti1").c_str());

    auto r = targen(base, {"-d", "5", "-G", "-g", "11", "-f", "50"});
    REQUIRE(r.exit_code == 0);

    std::ifstream ti1(base + ".ti1");
    REQUIRE(ti1.good());

    std::remove((base + ".ti1").c_str());
}

// ---- A19: ICC Profile E2E Tests -------------------------------------------

// Helper: write a CTI3 fixture file with spectral data (36 bands)
// Each data row must have exactly 43 fields: SAMPLE_ID, CMY_C, CMY_M, CMY_Y,
// 36 spectral values (SPEC_380...SPEC_730), XYZ_X, XYZ_Y, XYZ_Z
// Note: colprof takes output_base (without extension), appends .ti3 internally
static bool write_test_ti3(const std::string& base_path) {
    // colprof appends .ti3 to base_path, so base_path should be like "/tmp/test_base"
    std::string ti3_path = base_path + ".ti3";
    std::ofstream f(ti3_path);
    if (!f) return false;

    f << "CTI3\n\n"
      << "DESCRIPTOR \"jcolour A19 E2E test\"\n"
      << "ORIGINATOR \"jcolour test\"\n"
      << "CREATED \"Mon May  4 02:00:00 2026\"\n"
      << "DEVICE_CLASS \"OUTPUT\"\n"
      << "TOTAL_INK_LIMIT \"360\"\n"
      << "COLOR_REP \"CMY_XYZ\"\n"
      << "SPECTRAL_BANDS 36\n"
      << "SPECTRAL_START_NM 380.0\n"
      << "SPECTRAL_END_NM 730.0\n\n"
      << "NUMBER_OF_FIELDS 43\n"
      << "BEGIN_DATA_FORMAT\n"
      << "SAMPLE_ID CMY_C CMY_M CMY_Y\n"
      << "SPEC_380 SPEC_390 SPEC_400 SPEC_410 SPEC_420 SPEC_430 SPEC_440 SPEC_450\n"
      << "SPEC_460 SPEC_470 SPEC_480 SPEC_490 SPEC_500 SPEC_510 SPEC_520 SPEC_530\n"
      << "SPEC_540 SPEC_550 SPEC_560 SPEC_570 SPEC_580 SPEC_590 SPEC_600 SPEC_610\n"
      << "SPEC_620 SPEC_630 SPEC_640 SPEC_650 SPEC_660 SPEC_670 SPEC_680 SPEC_690\n"
      << "SPEC_700 SPEC_710 SPEC_720 SPEC_730\n"
      << "XYZ_X XYZ_Y XYZ_Z\n"
      << "END_DATA_FORMAT\n\n"
      << "NUMBER_OF_SETS 5\n"
      << "BEGIN_DATA\n";

    // Generate 5 rows with exactly 43 fields each
    // Format: SAMPLE_ID(1) + CMY(3) + 36 spectral + XYZ(3) = 43
    const std::string rows[] = {
        // Row 1: Light gray (10% each CMY)
        "1 10 10 10 "
        "0.100 0.105 0.110 0.115 0.120 0.125 0.130 0.135 0.140 0.145 0.150 0.155 "
        "0.160 0.165 0.170 0.175 0.180 0.185 0.190 0.195 0.200 0.205 0.210 0.215 "
        "0.220 0.225 0.230 0.235 0.240 0.245 0.250 0.255 0.260 0.265 0.270 0.275 "
        "66.0 66.4 52.2",
        // Row 2: Pure magenta
        "2 0 100 0 "
        "0.150 0.155 0.160 0.165 0.170 0.175 0.180 0.185 0.190 0.200 0.210 0.220 "
        "0.230 0.240 0.250 0.260 0.270 0.280 0.290 0.300 0.310 0.320 0.330 0.340 "
        "0.350 0.360 0.370 0.380 0.390 0.400 0.410 0.420 0.430 0.440 0.450 0.460 "
        "38.0 19.0 20.0",
        // Row 3: Mixed color (C+M)
        "3 48 49 0 "
        "0.070 0.075 0.080 0.085 0.090 0.095 0.100 0.105 0.110 0.115 0.120 0.125 "
        "0.130 0.135 0.140 0.145 0.150 0.155 0.160 0.165 0.170 0.175 0.180 0.185 "
        "0.190 0.195 0.200 0.205 0.210 0.215 0.220 0.225 0.230 0.235 0.240 0.245 "
        "16.8 13.8 23.1",
        // Row 4: Medium gray (20% each CMY)
        "4 20 20 20 "
        "0.110 0.115 0.120 0.125 0.130 0.135 0.140 0.145 0.150 0.160 0.170 0.180 "
        "0.190 0.200 0.210 0.220 0.230 0.240 0.250 0.260 0.270 0.280 0.290 0.300 "
        "0.310 0.320 0.330 0.340 0.350 0.360 0.370 0.380 0.390 0.400 0.410 0.420 "
        "44.7 43.3 32.4",
        // Row 5: White paper (no ink)
        "5 0 0 0 "
        "0.800 0.805 0.810 0.815 0.820 0.825 0.830 0.835 0.840 0.845 0.850 0.855 "
        "0.860 0.865 0.870 0.875 0.880 0.885 0.890 0.895 0.900 0.905 0.910 0.915 "
        "0.920 0.925 0.930 0.935 0.940 0.945 0.950 0.955 0.960 0.965 0.970 0.975 "
        "96.4 100.0 82.5"
    };

    for (int i = 0; i < 5; i++) {
        // Verify row has 43 fields before writing
        std::istringstream iss(rows[i]);
        int count = 0;
        std::string tok;
        while (iss >> tok) count++;
        if (count != 43) {
            return false;
        }
        f << rows[i] << "\n";
    }

    f << "END_DATA\n";
    return f.good();
}

TEST_CASE("colprof: E2E generates valid ICC profile from ti3", "[argyll][integration][a19]") {
    std::string colprof_path = find_tool("colprof");
    if (colprof_path.empty()) {
        SKIP("colprof not installed");
    }

    const std::string base_path = "/tmp/jcolour_a19_e2e";
    const std::string ti3_path = base_path + ".ti3";
    const std::string icc_path = base_path + ".icc";

    std::remove(ti3_path.c_str());
    std::remove(icc_path.c_str());

    REQUIRE(write_test_ti3(base_path));

    auto r = colprof(base_path, {"-A", "jcolour", "-M", "TestPrinter", "-D", "A19 E2E Test", "-qh"});
    INFO("colprof stdout: " << r.stdout_out);
    INFO("colprof stderr: " << r.stderr_out);
    REQUIRE(r.exit_code == 0);

    // Verify ICC file was created (> 100KB)
    std::ifstream icc(icc_path, std::ios::binary);
    REQUIRE(icc.good());
    icc.seekg(0, std::ios::end);
    auto size = icc.tellg();
    INFO("ICC file size: " << size << " bytes");
    REQUIRE(size > 100000);

    // Verify ICC file header
    // ICC profile format: [4 bytes size][4 bytes signature][rest]
    icc.seekg(4, std::ios::beg);  // Skip size, signature starts at offset 4
    char sig[4];
    icc.read(sig, 4);
    // Argyll creates profiles with "argl" signature
    REQUIRE(sig[0] == 'a');
    REQUIRE(sig[1] == 'r');
    REQUIRE(sig[2] == 'g');
    REQUIRE(sig[3] == 'l');

    std::remove(ti3_path.c_str());
    std::remove(icc_path.c_str());
}

TEST_CASE("colprof: profile check shows avg ΔE < 2", "[argyll][integration][a19]") {
    std::string colprof_path = find_tool("colprof");
    if (colprof_path.empty()) {
        SKIP("colprof not installed");
    }

    const std::string base_path = "/tmp/jcolour_a19_deltae";
    const std::string ti3_path = base_path + ".ti3";

    std::remove(ti3_path.c_str());
    std::remove((base_path + ".icc").c_str());

    REQUIRE(write_test_ti3(base_path));

    // colprof outputs: "avg err = X.XXXXX, RMS = Y.YYYYY"
    // Note: output_base must come LAST in arg list (colprof appends .ti3 and writes .icc)
    // Use -v to see the error metrics; without -v colprof is silent
    auto r = colprof(base_path, {"-v", "-A", "jcolour", "-M", "Test", "-D", "DeltaE Test", "-qh"});

    double avg_err = -1.0;
    auto& output = r.stdout_out;  // -v outputs to stdout
    // Parse "avg err = X.XXXXX" from output like "peak err = 0.029, avg err = 0.010, RMS = ..."
    auto pos = output.find("avg err");
    if (pos != std::string::npos) {
        // Find the "=" after "avg err"
        size_t eq_pos = output.find("=", pos);
        if (eq_pos != std::string::npos) {
            const char* start = output.c_str() + eq_pos + 1;
            avg_err = std::atof(start);
        }
    }

    INFO("Average error: " << avg_err);
    REQUIRE(r.exit_code == 0);
    REQUIRE(avg_err >= 0);
    REQUIRE(avg_err < 2.0);  // Threshold: ΔE_avg < 2

    std::remove(ti3_path.c_str());
    std::remove((base_path + ".icc").c_str());
}
