#include "paper_feed.h"

#include <cmath>

namespace jcolour {
namespace paperfeed {

FeedResult compute(double measuredMm) {
    FeedResult r;
    r.measuredMm  = measuredMm;
    r.deviationMm = measuredMm - kReferenceMm;
    // GMG table: printed too long (dev>0) → fed too much → negative adjustment.
    r.adjustPercent     = -r.deviationMm * kPercentPerMm;
    r.advancePerMeterMm = -r.deviationMm / kReferenceMm * 1000.0;
    // half a table step (0.125 mm) counts as on-target.
    r.onTarget = std::abs(r.deviationMm) < 0.125;
    return r;
}

QString describe(const FeedResult& r) {
    if (r.onTarget)
        return QStringLiteral("實測 %1mm ≈ 280mm，走紙正確（補償 0.00%）")
            .arg(r.measuredMm, 0, 'f', 2);
    return QStringLiteral("實測 %1mm → 走紙補償 %2%3%（每米 %4%5mm）")
        .arg(r.measuredMm, 0, 'f', 2)
        .arg(r.adjustPercent >= 0 ? "+" : "")
        .arg(r.adjustPercent, 0, 'f', 2)
        .arg(r.advancePerMeterMm >= 0 ? "+" : "")
        .arg(r.advancePerMeterMm, 0, 'f', 1);
}

}  // namespace paperfeed
}  // namespace jcolour
