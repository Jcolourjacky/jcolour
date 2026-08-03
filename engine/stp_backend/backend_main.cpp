#include "stp_backend.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

// jcolour-stp-backend CLI.
//
//   jcolour-stp-backend [manifest]
//
// Reads a key=value job manifest (from the file argument, or stdin if omitted
// or "-"), drives Gutenprint, and writes the ESC/P2 byte stream to the
// manifest's `output` (default stdout). All diagnostics go to stderr.
//
// This is the process boundary that keeps GPL (libgutenprint) out of the
// closed jcolour engine: the engine spawns this executable and pipes a
// manifest in.

namespace {

void print_usage(std::ostream& os) {
    os << "usage: jcolour-stp-backend [MANIFEST]\n"
       << "  MANIFEST  key=value job manifest file; \"-\" or omitted = stdin\n"
       << "\n"
       << "manifest keys: model input output width height dpi media pagesize\n"
       << "               resolution quality ink_limit\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string manifest_path = "-";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            print_usage(std::cout);
            return 0;
        }
        manifest_path = a;
    }

    jcolour::stp::StpJobRequest req;
    std::string err;
    bool ok = false;
    if (manifest_path == "-") {
        ok = jcolour::stp::parse_manifest(std::cin, req, err);
    } else {
        std::ifstream in(manifest_path);
        if (!in) {
            std::cerr << "stp-backend: cannot open manifest \"" << manifest_path
                      << "\"\n";
            return 1;
        }
        ok = jcolour::stp::parse_manifest(in, req, err);
    }
    if (!ok) {
        std::cerr << err << "\n";
        return 1;
    }

    return jcolour::stp::run_stp_backend(req, std::cerr);
}
