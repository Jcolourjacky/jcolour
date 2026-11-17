#pragma once

// CIELAB(D50) → 屏幕 sRGB 的 QColor（色块示意 / 靶预览用；LCMS 相对比色）。

#include "../../../engine/src/spectral.h"

#include <QColor>
#include <lcms2.h>

#include <algorithm>

namespace jcolour::ui {

inline QColor labToQColor(const jcolour::spectral::Lab& c) {
    static cmsHTRANSFORM xf = [] {
        cmsHPROFILE lab = cmsCreateLab4Profile(nullptr);
        cmsHPROFILE srgb = cmsCreate_sRGBProfile();
        cmsHTRANSFORM t = cmsCreateTransform(lab, TYPE_Lab_DBL, srgb, TYPE_RGB_8,
                                             INTENT_RELATIVE_COLORIMETRIC, 0);
        cmsCloseProfile(lab);
        cmsCloseProfile(srgb);
        return t;
    }();
    cmsCIELab in{c.L, c.a, c.b};
    unsigned char rgb[3] = {0, 0, 0};
    if (xf) cmsDoTransform(xf, &in, rgb, 1);
    return QColor(rgb[0], rgb[1], rgb[2]);
}

}  // namespace jcolour::ui
