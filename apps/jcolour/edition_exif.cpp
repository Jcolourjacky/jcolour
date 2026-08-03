#include "edition_exif.h"

#include <QFile>
#include <QByteArray>
#include <tiffio.h>
#include <vector>

namespace jcolour {
namespace exif {

QString buildDescription(const EditionFields& f) {
    QString run = f.editionSize > 0 ? QString("%1/%2").arg(f.printNumber).arg(f.editionSize)
                                    : QString("%1 (open)").arg(f.printNumber);
    QString s = QString("Edition: %1 — Print %2")
                    .arg(f.title.isEmpty() ? QStringLiteral("Untitled") : f.title, run);
    if (!f.dateTime.isEmpty())  s += " · " + f.dateTime;
    if (!f.imageHash.isEmpty()) s += " · src " + f.imageHash.left(12);
    return s;
}

namespace {
// EXIF UserComment = 8-byte character-code prefix + text (here ASCII).
QByteArray userComment(const QString& text) {
    QByteArray b("ASCII\0\0\0", 8);
    b += text.toUtf8();
    return b;
}
}  // namespace

bool writeEditionTiff(const QString& srcPath, const QString& dstPath, const EditionFields& f) {
    TIFF* in = TIFFOpen(srcPath.toUtf8().constData(), "r");
    if (!in) return false;

    uint32_t w = 0, h = 0;
    uint16_t bps = 0, spp = 0, photometric = 0, planar = PLANARCONFIG_CONTIG;
    TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(in, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetFieldDefaulted(in, TIFFTAG_BITSPERSAMPLE, &bps);
    TIFFGetFieldDefaulted(in, TIFFTAG_SAMPLESPERPIXEL, &spp);
    TIFFGetFieldDefaulted(in, TIFFTAG_PHOTOMETRIC, &photometric);
    TIFFGetFieldDefaulted(in, TIFFTAG_PLANARCONFIG, &planar);
    if (w == 0 || h == 0 || planar != PLANARCONFIG_CONTIG) { TIFFClose(in); return false; }

    const tmsize_t scanline = TIFFScanlineSize(in);
    std::vector<uint8_t> raster(static_cast<size_t>(scanline) * h);
    for (uint32_t row = 0; row < h; ++row) {
        if (TIFFReadScanline(in, raster.data() + static_cast<size_t>(row) * scanline, row) < 0) {
            TIFFClose(in);
            return false;
        }
    }
    TIFFClose(in);

    const bool inPlace = (dstPath == srcPath);
    const QString outPath = inPlace ? dstPath + ".exiftmp" : dstPath;

    TIFF* out = TIFFOpen(outPath.toUtf8().constData(), "w");
    if (!out) return false;

    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, w);
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, h);
    TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, bps);
    TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, spp);
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, photometric);
    TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, 0));

    // Baseline provenance tags.
    const QByteArray desc = buildDescription(f).toUtf8();
    TIFFSetField(out, TIFFTAG_IMAGEDESCRIPTION, desc.constData());
    TIFFSetField(out, TIFFTAG_SOFTWARE, "jcolour");
    if (!f.artist.isEmpty())    TIFFSetField(out, TIFFTAG_ARTIST, f.artist.toUtf8().constData());
    if (!f.copyright.isEmpty()) TIFFSetField(out, TIFFTAG_COPYRIGHT, f.copyright.toUtf8().constData());
    if (!f.dateTime.isEmpty())  TIFFSetField(out, TIFFTAG_DATETIME, f.dateTime.toUtf8().constData());

    for (uint32_t row = 0; row < h; ++row) {
        if (TIFFWriteScanline(out, raster.data() + static_cast<size_t>(row) * scanline, row) < 0) {
            TIFFClose(out);
            QFile::remove(outPath);
            return false;
        }
    }

    // EXIF sub-IFD: UserComment + DateTimeOriginal.
    bool exifOk = false;
    if (TIFFWriteDirectory(out) != 0) {
        if (TIFFCreateEXIFDirectory(out) == 0) {
            const QByteArray uc = userComment(buildDescription(f));
            TIFFSetField(out, EXIFTAG_USERCOMMENT, static_cast<uint32_t>(uc.size()), uc.constData());
            if (!f.dateTime.isEmpty())
                TIFFSetField(out, EXIFTAG_DATETIMEORIGINAL, f.dateTime.toUtf8().constData());
            uint64_t exifOffset = 0;
            if (TIFFWriteCustomDirectory(out, &exifOffset) != 0) {
                TIFFSetDirectory(out, 0);
                TIFFSetField(out, TIFFTAG_EXIFIFD, exifOffset);
                exifOk = true;
            }
        }
    }
    Q_UNUSED(exifOk);
    TIFFClose(out);

    if (inPlace) {
        if (!QFile::remove(srcPath) || !QFile::rename(outPath, dstPath)) {
            QFile::remove(outPath);
            return false;
        }
    }
    return true;
}

}  // namespace exif
}  // namespace jcolour
