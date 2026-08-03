#pragma once

// ogv_meter — PIPELINE §8.5 "Or/Gr/V 调用率" estimator.
//
// Pre-print estimate of how much Orange / Green / Violet the self-managed
// separation will recruit for an image: the headline differentiator vs Esko /
// ColorLogic (image-INdependent). It answers "is the extended gamut actually
// being used for THIS image?" → the three meters in the inspector.
//
// Method (estimator, not ground truth): per-pixel Lab → hue+chroma. CMYK is
// weak in the orange / green / violet hue bands; chroma above a hue-dependent
// CMYK ceiling in those bands implies the separation must fire that extra ink,
// proportional to the excess. Ground truth = actual coverage in the rendered
// 12-ch raster (post-§7) — this meter is the cheap pre-flight advisor.
//
// Lab-direct (no LCMS / ICC dependency) so it is trivially unit-testable; the
// §7/UI caller converts ProPhoto→Lab (e.g. via GamutAdvisor's pass) and calls
// estimate_ogv_usage().

#include <cstddef>
#include <string>

namespace jcolour {
namespace gamut {

struct OGVUsage {
    // Mean recruited coverage over the whole image (0..1) — drives the meters.
    float or_pct = 0, gr_pct = 0, v_pct = 0;
    // Fraction of pixels actually recruiting each extra ink (0..1).
    float or_ratio = 0, gr_ratio = 0, v_ratio = 0;
    // Fraction of pixels recruiting ANY of O/G/V — "extended gamut在用吗".
    float extension_ratio = 0;
    std::string summary;     // per §8.5 thresholds (<2% 基本未用 / >30% 充分调用)
};

// lab: interleaved L,a,b float triples; n = pixel count.
OGVUsage estimate_ogv_usage(const float* lab, std::size_t n);

}  // namespace gamut
}  // namespace jcolour
