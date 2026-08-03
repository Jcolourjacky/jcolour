#include "cynsn.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace jcolour {
namespace forward {

std::vector<double> demichel(const std::vector<double>& coverages) {
    int n = static_cast<int>(coverages.size());
    int num_primaries = 1 << n;
    std::vector<double> probs(num_primaries);

    for (int k = 0; k < num_primaries; ++k) {
        double p = 1.0;
        for (int j = 0; j < n; ++j) {
            double c = std::max(0.0, std::min(1.0, coverages[j]));
            p *= (k >> j & 1) ? c : (1.0 - c);
        }
        probs[k] = p;
    }
    return probs;
}

YNSNModel::YNSNModel(double n) : n_(n) {
    if (n <= 0.0) throw std::invalid_argument("Yule-Nielsen n must be > 0");
}

void YNSNModel::add_primary(const spectral::Spectrum& primary) {
    primaries_.push_back(primary);

    // Infer n_inks_ from number of primaries: 2^n_inks == primaries_.size()
    int sz = static_cast<int>(primaries_.size());
    // sz must be a power of 2
    if ((sz & (sz - 1)) == 0) {
        n_inks_ = 0;
        while ((1 << n_inks_) < sz) ++n_inks_;
    }
    // If not a power of 2 yet, n_inks_ stays at the last valid value;
    // predict() will throw if called with an incomplete primary set.
}

spectral::Spectrum YNSNModel::predict(const std::vector<double>& inks) const {
    if (static_cast<int>(inks.size()) != n_inks_) {
        throw std::invalid_argument("ink count mismatch");
    }
    int expected = 1 << n_inks_;
    if (static_cast<int>(primaries_.size()) != expected) {
        throw std::invalid_argument("incomplete primaries: need 2^N");
    }

    auto probs = demichel(inks);

    spectral::Spectrum result = {};
    double inv_n = 1.0 / n_;
    for (int i = 0; i < spectral::NUM_CHANNELS; ++i) {
        double sum = 0.0;
        for (int k = 0; k < expected; ++k) {
            double r = std::max(0.0, primaries_[k][i]);
            sum += probs[k] * std::pow(r, inv_n);
        }
        result[i] = std::pow(sum, n_);
    }
    return result;
}

// ---- CellularYNSNModel ----

CellularYNSNModel::CellularYNSNModel(int n_inks, int divisions, double yn_n)
    : n_inks_(n_inks), divisions_(divisions), yn_n_(yn_n)
{
    if (n_inks < 1) throw std::invalid_argument("n_inks must be >= 1");
    if (divisions < 1) throw std::invalid_argument("divisions must be >= 1");
    if (yn_n <= 0.0) throw std::invalid_argument("yn_n must be > 0");

    int grid_size = 1;
    for (int j = 0; j < n_inks; ++j) grid_size *= (divisions + 1);
    spectral::Spectrum zero = {};
    grid_.assign(grid_size, zero);
}

int CellularYNSNModel::grid_index(const std::vector<int>& pos) const {
    int idx = 0, stride = 1;
    for (int j = 0; j < n_inks_; ++j) {
        idx += pos[j] * stride;
        stride *= (divisions_ + 1);
    }
    return idx;
}

void CellularYNSNModel::set_grid_point(const std::vector<int>& pos,
                                        const spectral::Spectrum& spectrum) {
    if (static_cast<int>(pos.size()) != n_inks_)
        throw std::invalid_argument("pos size mismatch");
    for (int j = 0; j < n_inks_; ++j)
        if (pos[j] < 0 || pos[j] > divisions_)
            throw std::invalid_argument("grid pos out of range");
    grid_[grid_index(pos)] = spectrum;
}

spectral::Spectrum CellularYNSNModel::predict(const std::vector<double>& inks) const {
    if (static_cast<int>(inks.size()) != n_inks_)
        throw std::invalid_argument("ink count mismatch");

    std::vector<int> cell(n_inks_);
    std::vector<double> t(n_inks_);
    for (int j = 0; j < n_inks_; ++j) {
        double c = std::max(0.0, std::min(1.0, inks[j]));
        cell[j] = std::min(static_cast<int>(c * divisions_), divisions_ - 1);
        t[j] = c * divisions_ - cell[j];
    }

    int n_corners = 1 << n_inks_;
    auto probs = demichel(t);

    spectral::Spectrum result = {};
    double inv_n = 1.0 / yn_n_;
    for (int i = 0; i < spectral::NUM_CHANNELS; ++i) {
        double sum = 0.0;
        for (int k = 0; k < n_corners; ++k) {
            std::vector<int> corner(n_inks_);
            for (int j = 0; j < n_inks_; ++j)
                corner[j] = cell[j] + ((k >> j) & 1);
            double r = std::max(0.0, grid_[grid_index(corner)][i]);
            sum += probs[k] * std::pow(r, inv_n);
        }
        result[i] = std::pow(sum, yn_n_);
    }
    return result;
}

}  // namespace forward
}  // namespace jcolour
