#include "stp_backend.h"

#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace jcolour {
namespace stp {

namespace {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool parse_int(const std::string& v, int& out) {
    try {
        size_t pos = 0;
        const long n = std::stol(v, &pos);
        if (pos != v.size()) return false;
        out = static_cast<int>(n);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parse_double(const std::string& v, double& out) {
    try {
        size_t pos = 0;
        out = std::stod(v, &pos);
        return pos == v.size();
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace

bool parse_manifest(std::istream& in, StpJobRequest& out, std::string& err) {
    std::string line;
    int lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;

        const auto eq = t.find('=');
        if (eq == std::string::npos) {
            err = "manifest line " + std::to_string(lineno) +
                  ": missing '=' in \"" + t + "\"";
            return false;
        }
        const std::string key = trim(t.substr(0, eq));
        const std::string val = trim(t.substr(eq + 1));

        auto fail_num = [&](const char* k) {
            err = "manifest line " + std::to_string(lineno) + ": " + k +
                  " is not a valid number: \"" + val + "\"";
        };

        if (key == "model") {
            out.model = val;
        } else if (key == "input") {
            out.input = val;
        } else if (key == "output") {
            out.output = val;
        } else if (key == "media") {
            out.media = val;
        } else if (key == "pagesize") {
            out.pagesize = val;
        } else if (key == "resolution") {
            out.resolution = val;
        } else if (key == "width") {
            if (!parse_int(val, out.width_px)) { fail_num("width"); return false; }
        } else if (key == "height") {
            if (!parse_int(val, out.height_px)) { fail_num("height"); return false; }
        } else if (key == "dpi") {
            if (!parse_int(val, out.dpi)) { fail_num("dpi"); return false; }
        } else if (key == "quality") {
            if (!parse_int(val, out.quality)) { fail_num("quality"); return false; }
        } else if (key == "ink_limit") {
            if (!parse_double(val, out.ink_limit)) { fail_num("ink_limit"); return false; }
        }
        // Unknown keys are ignored on purpose (forward-compat).
    }
    return true;
}

void write_manifest(std::ostream& out, const StpJobRequest& req) {
    out << "# jcolour-stp-backend job manifest\n"
        << "model=" << req.model << "\n"
        << "input=" << req.input << "\n"
        << "output=" << req.output << "\n"
        << "width=" << req.width_px << "\n"
        << "height=" << req.height_px << "\n"
        << "dpi=" << req.dpi << "\n"
        << "media=" << req.media << "\n"
        << "pagesize=" << req.pagesize << "\n"
        << "resolution=" << req.resolution << "\n"
        << "quality=" << req.quality << "\n"
        << "ink_limit=" << req.ink_limit << "\n";
}

}  // namespace stp
}  // namespace jcolour
