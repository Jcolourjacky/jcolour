#include "target_measure.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace jcolour {
namespace calibration {

double MeasuredPatch::chroma() const { return std::hypot(a, b); }
double MeasuredPatch::hue() const {
    double h = std::atan2(b, a) * 180.0 / 3.14159265358979323846;
    return h < 0 ? h + 360.0 : h;
}

std::vector<MeasuredPatch> parse_patch_list(const std::string& path) {
    std::vector<MeasuredPatch> out;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("LABEL", 0) == 0) continue;            // header
        std::stringstream ss(line);
        MeasuredPatch p;
        std::string tok;
        if (!std::getline(ss, p.label, '\t')) continue;
        bool ok = true;
        for (int i = 0; i < 7; ++i) {
            if (!std::getline(ss, tok, '\t')) { ok = false; break; }
            p.cmykogv[i] = std::stof(tok);
        }
        if (ok) out.push_back(p);
    }
    return out;
}

std::vector<CgatsRow> parse_cgats_lab(const std::string& path) {
    std::vector<CgatsRow> out;
    std::ifstream f(path);
    std::string line;
    std::vector<std::string> fmt;
    bool in_fmt = false, in_data = false;
    int iId = -1, iL = -1, iA = -1, iB = -1;
    while (std::getline(f, line)) {
        std::string t = line;
        // trim
        t.erase(0, t.find_first_not_of(" \t\r\n"));
        if (t.empty()) continue;
        if (t.rfind("BEGIN_DATA_FORMAT", 0) == 0) { in_fmt = true; continue; }
        if (t.rfind("END_DATA_FORMAT", 0) == 0) {
            in_fmt = false;
            for (int i = 0; i < (int)fmt.size(); ++i) {
                if (fmt[i] == "SAMPLE_ID" || fmt[i] == "SampleID") iId = i;
                else if (fmt[i] == "LAB_L") iL = i;
                else if (fmt[i] == "LAB_A") iA = i;
                else if (fmt[i] == "LAB_B") iB = i;
            }
            continue;
        }
        if (t.rfind("BEGIN_DATA", 0) == 0) { in_data = true; continue; }
        if (t.rfind("END_DATA", 0) == 0) { in_data = false; continue; }
        if (in_fmt) {
            std::stringstream ss(t); std::string w;
            while (ss >> w) fmt.push_back(w);
        } else if (in_data && iL >= 0 && iA >= 0 && iB >= 0) {
            std::stringstream ss(t); std::string w; std::vector<std::string> cols;
            while (ss >> w) cols.push_back(w);
            int need = std::max({iId, iL, iA, iB});
            if ((int)cols.size() <= need) continue;
            CgatsRow r;
            r.id = iId >= 0 ? cols[iId] : "";
            r.L = std::stod(cols[iL]); r.a = std::stod(cols[iA]); r.b = std::stod(cols[iB]);
            out.push_back(r);
        }
    }
    return out;
}

std::vector<MeasuredPatch> join_measurement(const std::string& patch_list_tsv,
                                            const std::string& cgats_ti3) {
    auto dev = parse_patch_list(patch_list_tsv);
    auto meas = parse_cgats_lab(cgats_ti3);
    if (dev.size() != meas.size() || dev.empty()) return {};
    for (size_t i = 0; i < dev.size(); ++i) {
        dev[i].L = meas[i].L; dev[i].a = meas[i].a; dev[i].b = meas[i].b;
    }
    return dev;
}

GamutSummary summarize_gamut(const std::vector<MeasuredPatch>& m) {
    GamutSummary g;
    constexpr int K = 3, O = 4, GR = 5, V = 6;
    auto cmy_only = [&](const MeasuredPatch& p) {
        return p.cmykogv[K] < 0.001f && p.cmykogv[O] < 0.001f &&
               p.cmykogv[GR] < 0.001f && p.cmykogv[V] < 0.001f;
    };
    // The full-tone corner for an extended ink = drives only `ch`, at its max.
    auto corner = [&](int ch) -> const MeasuredPatch* {
        const MeasuredPatch* best = nullptr;
        for (const auto& p : m) {
            bool pure = p.cmykogv[ch] > 0.001f;
            for (int i = 0; i < 7 && pure; ++i)
                if (i != ch && p.cmykogv[i] > 0.001f) pure = false;
            if (pure && (!best || p.cmykogv[ch] > best->cmykogv[ch])) best = &p;
        }
        return best;
    };
    auto dEab = [](const MeasuredPatch& p, const MeasuredPatch& q) {
        return std::sqrt((p.L-q.L)*(p.L-q.L) + (p.a-q.a)*(p.a-q.a) + (p.b-q.b)*(p.b-q.b));
    };
    // nearest CMY-only ΔEab to a corner = lower-bound distance beyond CMY hull.
    auto nearest_cmy = [&](const MeasuredPatch& corner_p) {
        double best = 1e9;
        for (const auto& p : m) if (cmy_only(p)) best = std::min(best, dEab(corner_p, p));
        return best == 1e9 ? 0.0 : best;
    };
    if (auto* c = corner(O))  { g.or_corner_C = c->chroma(); g.or_dist = nearest_cmy(*c); }
    if (auto* c = corner(GR)) { g.gr_corner_C = c->chroma(); g.gr_dist = nearest_cmy(*c); }
    if (auto* c = corner(V))  { g.v_corner_C  = c->chroma(); g.v_dist  = nearest_cmy(*c); }
    return g;
}

}  // namespace calibration
}  // namespace jcolour
