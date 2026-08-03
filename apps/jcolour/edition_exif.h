#pragma once

// edition_exif — embeds edition/print provenance into a printed TIFF's metadata.
// Writes baseline TIFF tags (ImageDescription / Artist / Copyright / Software /
// DateTime) plus an EXIF sub-IFD (UserComment + DateTimeOriginal) so the edition
// number, run size and source hash travel with the file. TIFF is the engine's
// 16-bit output format; libtiff is already linked.

#include <QString>

namespace jcolour {
namespace exif {

struct EditionFields {
    QString title;
    QString artist;
    QString copyright;
    QString imageHash;
    QString dateTime;     // "YYYY:MM:DD HH:MM:SS" (EXIF format)
    int     editionNumber = 0;
    int     editionSize   = 0;   // 0 = open edition
    int     printNumber   = 0;   // this print's index in the edition
};

// Human-readable provenance string ("Edition <title> — Print 3/25 · …").
QString buildDescription(const EditionFields& f);

// Read srcPath (TIFF), write dstPath (TIFF) with the edition metadata added.
// In-place when dstPath == srcPath (written via a temp file then renamed).
// Returns false if the source is not a readable TIFF or writing fails.
bool writeEditionTiff(const QString& srcPath, const QString& dstPath, const EditionFields& f);

}  // namespace exif
}  // namespace jcolour
