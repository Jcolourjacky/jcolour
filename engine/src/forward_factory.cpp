#include "forward_factory.h"
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <cctype>
#include <cstdlib>

namespace jcolour {
namespace forward {

// ---- minimal JSON helpers ----

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Extract the raw value string for a given key from a flat JSON object.
// Handles string, number, array, and nested-object values.
static std::string json_get(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos)
        throw std::invalid_argument("JSON key not found: " + key);
    pos += needle.size();
    // skip whitespace and colon
    while (pos < json.size() && (std::isspace(json[pos]) || json[pos] == ':')) ++pos;
    if (pos >= json.size()) throw std::invalid_argument("JSON value missing for: " + key);

    char open = json[pos];
    if (open == '[' || open == '{') {
        // find matching close bracket
        char close = (open == '[') ? ']' : '}';
        int depth = 0;
        size_t start = pos;
        for (; pos < json.size(); ++pos) {
            if (json[pos] == open) ++depth;
            else if (json[pos] == close) { if (--depth == 0) { ++pos; break; } }
        }
        return json.substr(start, pos - start);
    }
    // number or string: read until comma or closing brace
    size_t start = pos;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != ']')
        ++pos;
    return trim(json.substr(start, pos - start));
}

// Parse a JSON array of doubles: [v0, v1, ...]
static std::vector<double> parse_double_array(const std::string& s) {
    std::vector<double> v;
    size_t pos = 0;
    while (pos < s.size()) {
        while (pos < s.size() && (s[pos] == '[' || s[pos] == ']' ||
               s[pos] == ',' || std::isspace(s[pos]))) ++pos;
        if (pos >= s.size()) break;
        char* end;
        double val = std::strtod(s.c_str() + pos, &end);
        if (end == s.c_str() + pos) break;
        v.push_back(val);
        pos = end - s.c_str();
    }
    return v;
}

spectral::Spectrum parse_spectrum(const std::string& json_array) {
    auto v = parse_double_array(json_array);
    if ((int)v.size() != spectral::NUM_CHANNELS)
        throw std::invalid_argument("spectrum must have 36 values");
    spectral::Spectrum s;
    for (int i = 0; i < spectral::NUM_CHANNELS; ++i) s[i] = v[i];
    return s;
}

std::string serialize_spectrum(const spectral::Spectrum& s) {
    std::ostringstream os;
    os << "[";
    for (int i = 0; i < spectral::NUM_CHANNELS; ++i) {
        if (i) os << ",";
        os << s[i];
    }
    os << "]";
    return os.str();
}

// ---- factories ----

std::unique_ptr<KMForwardModel> make_km(const std::string& json) {
    auto sub_str  = json_get(json, "substrate");
    auto inks_str = json_get(json, "inks");

    auto substrate = parse_spectrum(sub_str);
    auto model = std::make_unique<KMForwardModel>(substrate);

    // Parse array-of-arrays for inks
    // Find each inner array (start at 1 to skip the outer '[')
    size_t pos = 1;
    while (pos < inks_str.size()) {
        size_t start = inks_str.find('[', pos);
        if (start == std::string::npos) break;
        // find matching ]
        int depth = 0;
        size_t end = start;
        for (; end < inks_str.size(); ++end) {
            if (inks_str[end] == '[') ++depth;
            else if (inks_str[end] == ']') { if (--depth == 0) { ++end; break; } }
        }
        std::string ink_str = inks_str.substr(start, end - start);
        model->add_ink(parse_spectrum(ink_str));
        pos = end;
    }
    return model;
}

std::unique_ptr<CellularYNSNModel> make_cynsn(const std::string& json) {
    int n_inks    = std::stoi(json_get(json, "n_inks"));
    int divisions = std::stoi(json_get(json, "divisions"));
    double yn_n   = std::stod(json_get(json, "yn_n"));

    auto model = std::make_unique<CellularYNSNModel>(n_inks, divisions, yn_n);

    // Parse grid object: {"0000": [...], "0001": [...], ...}
    std::string grid_str = json_get(json, "grid");

    size_t pos = 0;
    while (pos < grid_str.size()) {
        // find key (quoted string)
        size_t qs = grid_str.find('"', pos);
        if (qs == std::string::npos) break;
        size_t qe = grid_str.find('"', qs + 1);
        if (qe == std::string::npos) break;
        std::string key = grid_str.substr(qs + 1, qe - qs - 1);
        pos = qe + 1;

        // find value array
        size_t vs = grid_str.find('[', pos);
        if (vs == std::string::npos) break;
        int depth = 0;
        size_t ve = vs;
        for (; ve < grid_str.size(); ++ve) {
            if (grid_str[ve] == '[') ++depth;
            else if (grid_str[ve] == ']') { if (--depth == 0) { ++ve; break; } }
        }
        auto spectrum = parse_spectrum(grid_str.substr(vs, ve - vs));

        // decode key: each character is one index digit
        std::vector<int> grid_pos;
        for (char c : key) grid_pos.push_back(c - '0');
        model->set_grid_point(grid_pos, spectrum);
        pos = ve;
    }
    return model;
}

std::string serialize_km(const spectral::Spectrum& substrate,
                          const std::vector<spectral::Spectrum>& inks) {
    std::ostringstream os;
    os << "{\"model\":\"km\",\"substrate\":" << serialize_spectrum(substrate)
       << ",\"inks\":[";
    for (int j = 0; j < (int)inks.size(); ++j) {
        if (j) os << ",";
        os << serialize_spectrum(inks[j]);
    }
    os << "]}";
    return os.str();
}

}  // namespace forward
}  // namespace jcolour
