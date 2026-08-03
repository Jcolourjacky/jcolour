#pragma once

#include <QString>
#include <QImage>
#include <QDateTime>
#include "bw_mode_widget.h"

namespace jcolour {

enum class JobStatus {
    Spooled,
    Processing,
    Printing,
    Completed,
    Failed,
    Held
};

inline QString jobStatusName(JobStatus s) {
    switch (s) {
        case JobStatus::Spooled:    return "Spooled";
        case JobStatus::Processing: return "Processing";
        case JobStatus::Printing:   return "Printing";
        case JobStatus::Completed:  return "Completed";
        case JobStatus::Failed:     return "Failed";
        case JobStatus::Held:       return "Held";
    }
    return {};
}

struct Job {
    QString id;
    QString imagePath;
    QImage  thumbnail;          // 64x64 cached
    QString workflowName;       // matches WorkflowSidebar entry

    // Color
    QString iccProfilePath;
    QString colorSpace = "RGB";
    int     renderingIntent = 1; // 0=Perceptual, 1=Relative, 2=Saturation, 3=Absolute
    bool    blackPointCompensation = true;

    // B&W
    jcolour::ui::BWModeSettings bw {
        jcolour::ui::BWModeSettings::MultiChannel,
        jcolour::ui::BWModeSettings::PhotoBlack,
        0, 0, 0, 0, 0, 0
    };

    // Channel correction (EFI Farbanpassung mirror): C M Y K R G B
    int corrC = 0, corrM = 0, corrY = 0, corrK = 0;
    int corrR = 0, corrG = 0, corrB = 0;
    int brightness = 0;
    int contrast   = 0;
    int saturation = 0;
    int sharpen    = 0;

    // Layout
    double paperWidthMm  = 297;
    double paperHeightMm = 420;
    QString paperName    = "A3";
    double marginsMm[4]  = {10, 10, 10, 10};

    // Output
    QString printerName = "Epson SureColor P9500";
    int     copies = 1;
    bool    positioned = false;

    // Status
    JobStatus  status = JobStatus::Spooled;
    QDateTime  submittedAt;
    QString    errorMessage;
    qint64     fileSizeBytes = 0;
    int        pageCount = 1;
};

} // namespace jcolour
