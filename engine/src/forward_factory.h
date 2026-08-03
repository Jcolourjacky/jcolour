#pragma once

// ForwardModel factory + JSON config serialization.
//
// JSON format (minimal, no external deps):
//   {"model":"km",    "substrate":[...36 values...], "inks":[[...],[...]]}
//   {"model":"cynsn", "n_inks":4, "divisions":2, "yn_n":2.5,
//    "grid":{"000...":[ ...36... ], ...}}
//
// The "grid" key maps grid-position strings (e.g. "0102") to spectra.

#include "km.h"
#include "cynsn.h"
#include <memory>
#include <string>
#include <stdexcept>

namespace jcolour {
namespace forward {

// Parse a JSON array of 36 doubles into a Spectrum.
// Expects: [v0, v1, ..., v35]
spectral::Spectrum parse_spectrum(const std::string& json_array);

// Serialize a Spectrum to a JSON array string.
std::string serialize_spectrum(const spectral::Spectrum& s);

// Build a KMForwardModel from a JSON config string.
// Required keys: "substrate" (36-element array), "inks" (array of arrays).
std::unique_ptr<KMForwardModel> make_km(const std::string& json);

// Build a CellularYNSNModel from a JSON config string.
// Required keys: "n_inks", "divisions", "yn_n", "grid".
// "grid" is an object mapping position strings to 36-element arrays.
// Position string: concatenated decimal indices, e.g. "0102" for pos={0,1,0,2}.
std::unique_ptr<CellularYNSNModel> make_cynsn(const std::string& json);

// Serialize a KMForwardModel to JSON.
// Requires access to substrate and ink spectra — caller provides them.
std::string serialize_km(const spectral::Spectrum& substrate,
                          const std::vector<spectral::Spectrum>& inks);

}  // namespace forward
}  // namespace jcolour
