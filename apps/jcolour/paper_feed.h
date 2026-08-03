#pragma once

// PaperFeed — GMG-style 走紙長度（米尺）量測核心.
//
// Reproduces the GMG "EPSON x900 / 7500-9500 Print Quality Check" paper-feed
// method: print a chart whose control lines span exactly 280.00 mm, measure the
// printed length with a ruler, then derive the printer's paper-feed adjustment.
//
// GMG reference table (Measured length → Paper Feed Adjustment):
//   281.00→-0.4  280.75→-0.3  280.50→-0.2  280.25→-0.1  280.00→0.00
//   279.75→+0.1  279.50→+0.2  279.25→+0.3  279.00→+0.4   (0.25 mm = 0.1 %)
//
// Pure (Qt-string only) so it can be unit-tested without widgets.

#include <QString>

namespace jcolour {
namespace paperfeed {

// The GMG control lines are printed 280.00 mm apart.
constexpr double kReferenceMm = 280.0;
// GMG table slope: 0.25 mm deviation ↔ 0.1 % adjustment → 0.4 %/mm.
constexpr double kPercentPerMm = 0.4;

struct FeedResult {
    double measuredMm        = kReferenceMm;  // ruler reading
    double deviationMm       = 0.0;           // measured - 280.00
    double adjustPercent     = 0.0;           // Epson paper-feed adjustment (GMG table)
    double advancePerMeterMm = 0.0;           // jcolour mediaAdvanceMm (1 m correction)
    bool   onTarget          = true;          // within ±0.125 mm of 280
};

// Convert a ruler reading (mm) into the feed correction.
FeedResult compute(double measuredMm);

// One-line human hint, e.g. "實測 280.50mm → 走紙 +0.20%（每米 -1.8mm）".
QString describe(const FeedResult& r);

}  // namespace paperfeed
}  // namespace jcolour
