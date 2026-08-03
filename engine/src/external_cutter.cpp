#include "external_cutter.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>
#include <cmath>
#include <QtMath>

namespace jcolour {

struct ExternalCutter::Impl {
    QSerialPort* port = nullptr;
};

ExternalCutter::ExternalCutter() : pImpl_(new Impl) {}

ExternalCutter::~ExternalCutter() {
    disconnect();
}

QString ExternalCutter::model_name(CutterModel model) {
    switch (model) {
        case CutterModel::SureCut_SF: return "SureCut SF Series";
        case CutterModel::SureCut_S2: return "SureCut S2";
        case CutterModel::SureCut_S3: return "SureCut S3";
        case CutterModel::FC9000: return "Graphtec FC9000";
        case CutterModel::FC8600: return "Graphtec FC8600";
        case CutterModel::Generic: return "Generic Serial Cutter";
        default: return "Unknown";
    }
}

CutterCapabilities ExternalCutter::capabilities(CutterModel model) {
    CutterCapabilities caps;

    switch (model) {
        case CutterModel::SureCut_SF:
            caps.supports_oversized_cut = true;
            caps.supports_multi_copy_cut = true;
            caps.supports_piece_cut = true;
            caps.supports_rotation = true;
            caps.max_speed_mm_s = 100.0;
            caps.min_speed_mm_s = 10.0;
            caps.max_copies = 99;
            break;

        case CutterModel::SureCut_S2:
            caps.supports_oversized_cut = true;
            caps.supports_multi_copy_cut = true;
            caps.supports_piece_cut = true;
            caps.supports_rotation = true;
            caps.max_speed_mm_s = 120.0;
            caps.min_speed_mm_s = 5.0;
            caps.max_copies = 999;
            break;

        case CutterModel::SureCut_S3:
            caps.supports_oversized_cut = true;
            caps.supports_multi_copy_cut = true;
            caps.supports_piece_cut = true;
            caps.supports_rotation = true;
            caps.max_speed_mm_s = 150.0;
            caps.min_speed_mm_s = 5.0;
            caps.max_copies = 9999;
            break;

        case CutterModel::FC9000:
            caps.supports_oversized_cut = true;
            caps.supports_multi_copy_cut = false;
            caps.supports_piece_cut = false;
            caps.supports_rotation = false;
            caps.max_speed_mm_s = 150.0;
            caps.min_speed_mm_s = 10.0;
            caps.max_copies = 1;
            break;

        case CutterModel::FC8600:
            caps.supports_oversized_cut = true;
            caps.supports_multi_copy_cut = false;
            caps.supports_piece_cut = false;
            caps.supports_rotation = false;
            caps.max_speed_mm_s = 120.0;
            caps.min_speed_mm_s = 10.0;
            caps.max_copies = 1;
            break;

        default:
            caps.max_speed_mm_s = 50.0;
            caps.min_speed_mm_s = 10.0;
            break;
    }

    return caps;
}

bool ExternalCutter::connect(const QString& port, int baud_rate) {
    if (connected_) {
        disconnect();
    }

    port_name_ = port;
    baud_rate_ = baud_rate;

    pImpl_->port = new QSerialPort(port);
    pImpl_->port->setBaudRate(baud_rate);
    pImpl_->port->setDataBits(QSerialPort::Data8);
    pImpl_->port->setParity(QSerialPort::NoParity);
    pImpl_->port->setStopBits(QSerialPort::OneStop);
    pImpl_->port->setFlowControl(QSerialPort::NoFlowControl);

    if (!pImpl_->port->open(QIODevice::ReadWrite)) {
        error_message_ = QString("Failed to open %1: %2")
            .arg(port)
            .arg(pImpl_->port->errorString());
        delete pImpl_->port;
        pImpl_->port = nullptr;
        return false;
    }

    connected_ = true;

    // Send initialization
    QByteArray init_data;
    switch (model_) {
        case CutterModel::SureCut_SF:
        case CutterModel::SureCut_S2:
        case CutterModel::SureCut_S3:
            init_data = OPOSCommandBuilder::initialize();
            break;
        case CutterModel::FC9000:
        case CutterModel::FC8600:
            init_data = SCPLCommandBuilder::initialize();
            break;
        default:
            init_data = OPOSCommandBuilder::initialize();
    }

    if (!send_command(init_data)) {
        error_message_ = "Failed to initialize cutter";
        disconnect();
        return false;
    }

    return true;
}

void ExternalCutter::disconnect() {
    if (pImpl_->port) {
        if (pImpl_->port->isOpen()) {
            pImpl_->port->close();
        }
        delete pImpl_->port;
        pImpl_->port = nullptr;
    }
    connected_ = false;
}

bool ExternalCutter::send_command(const QByteArray& cmd) {
    if (!connected_ || !pImpl_->port) {
        error_message_ = "Not connected";
        return false;
    }

    pImpl_->port->clear();
    pImpl_->port->write(cmd);

    if (!pImpl_->port->waitForBytesWritten(1000)) {
        error_message_ = "Write timeout";
        return false;
    }

    if (!pImpl_->port->waitForReadyRead(1000)) {
        return true; // Some commands don't have response
    }

    QByteArray response = pImpl_->port->readAll();
    return parse_response(response);
}

bool ExternalCutter::send_cut_data(const QByteArray& data) {
    if (!connected_) {
        error_message_ = "Not connected";
        return false;
    }

    return send_command(data);
}

bool ExternalCutter::send_cut_contour(const CutContourResult& contour) {
    QByteArray data;

    switch (model_) {
        case CutterModel::SureCut_SF:
        case CutterModel::SureCut_S2:
        case CutterModel::SureCut_S3:
            data = OPOSCommandBuilder::initialize();
            send_command(data);
            for (const auto& path : contour.paths) {
                for (int i = 0; i < path.points.size(); ++i) {
                    if (i == 0) {
                        data = build_opos_command(
                            QString("MD%1,%2").arg(path.points[i].x()).arg(path.points[i].y()));
                    } else {
                        data = build_opos_command(
                            QString("D%1,%2").arg(path.points[i].x()).arg(path.points[i].y()));
                    }
                    send_command(data);
                }
                if (path.is_closed) {
                    send_command(build_opos_command("D CLOSE"));
                }
            }
            break;

        case CutterModel::FC9000:
        case CutterModel::FC8600:
            data = SCPLCommandBuilder::initialize();
            send_command(data);
            for (const auto& path : contour.paths) {
                if (!path.points.isEmpty()) {
                    data = SCPLCommandBuilder::move_to(path.points[0].x(), path.points[0].y());
                    send_command(data);
                    for (int i = 1; i < path.points.size(); ++i) {
                        data = SCPLCommandBuilder::line_to(path.points[i].x(), path.points[i].y());
                        send_command(data);
                    }
                }
            }
            send_command(SCPLCommandBuilder::end_plot());
            break;

        default:
            data = build_opos_command("MD0,0");
            send_command(data);
            for (const auto& path : contour.paths) {
                for (int i = 0; i < path.points.size(); ++i) {
                    data = build_opos_command(
                        QString("D%1,%2").arg(path.points[i].x()).arg(path.points[i].y()));
                    send_command(data);
                }
            }
            break;
    }

    return true;
}

bool ExternalCutter::execute_cut() {
    QByteArray cmd;

    switch (model_) {
        case CutterModel::SureCut_SF:
        case CutterModel::SureCut_S2:
        case CutterModel::SureCut_S3:
            cmd = OPOSCommandBuilder::cut(0);
            break;
        case CutterModel::FC9000:
        case CutterModel::FC8600:
            cmd = SCPLCommandBuilder::cut();
            break;
        default:
            cmd = build_opos_command("cut");
    }

    return send_command(cmd);
}

bool ExternalCutter::set_speed(double mm_s) {
    speed_ = mm_s;
    QByteArray cmd;

    switch (model_) {
        case CutterModel::SureCut_SF:
        case CutterModel::SureCut_S2:
        case CutterModel::SureCut_S3:
            cmd = OPOSCommandBuilder::set_speed(mm_s);
            break;
        case CutterModel::FC9000:
        case CutterModel::FC8600:
            cmd = SCPLCommandBuilder::set_speed(mm_s);
            break;
        default:
            cmd = build_opos_command(QString("VS%1").arg(mm_s));
    }

    return send_command(cmd);
}

bool ExternalCutter::set_pressure(double grams) {
    pressure_ = grams;
    QByteArray cmd;

    switch (model_) {
        case CutterModel::SureCut_SF:
        case CutterModel::SureCut_S2:
        case CutterModel::SureCut_S3:
            cmd = OPOSCommandBuilder::set_pressure(grams);
            break;
        default:
            cmd = build_opos_command(QString("FP%1").arg(grams));
    }

    return send_command(cmd);
}

bool ExternalCutter::set_mode(CutContourMode mode) {
    mode_ = mode;
    return true;
}

QByteArray ExternalCutter::build_opos_command(const QString& cmd) {
    QString full_cmd = cmd;
    if (!full_cmd.endsWith('\r') && !full_cmd.endsWith('\n')) {
        full_cmd += '\r';
    }
    return full_cmd.toLatin1();
}

QByteArray ExternalCutter::build_arms_command(const QString& cmd) {
    return cmd.toLatin1();
}

QByteArray ExternalCutter::build_scpl_command(const QString& cmd) {
    return cmd.toLatin1() + '\n';
}

bool ExternalCutter::parse_response(const QByteArray& response) {
    if (response.isEmpty()) {
        return true;
    }

    QString resp = QString::fromLatin1(response).trimmed();

    if (resp.contains("ERROR") || resp.contains("NAK")) {
        error_message_ = "Cutter error: " + resp;
        return false;
    }

    return true;
}

// CutContourGenerator implementation
CutContourResult CutContourGenerator::generate_from_image(
    int image_width_px, int image_height_px,
    double dpi,
    CutContourMode mode,
    double border_mm) {

    CutContourResult result;

    switch (mode) {
        case CutContourMode::InsideContour:
            result = generate_rectangle(image_width_px, image_height_px, dpi, 0);
            break;
        case CutContourMode::OutsideContour:
            result = generate_rectangle(image_width_px, image_height_px, dpi, 0);
            result = offset_contour(result.paths.isEmpty() ? QVector<QPointF>() : result.paths[0].points, border_mm);
            break;
        default:
            result = generate_rectangle(image_width_px, image_height_px, dpi, border_mm);
    }

    return result;
}

CutContourResult CutContourGenerator::generate_rectangle(
    int image_width_px, int image_height_px,
    double dpi,
    double border_mm) {

    CutContourResult result;

    double width_mm = image_width_px / dpi * 25.4;
    double height_mm = image_height_px / dpi * 25.4;

    double left = -border_mm;
    double top = -border_mm;
    double right = width_mm + border_mm;
    double bottom = height_mm + border_mm;

    QVector<QPointF> rect_points;
    rect_points.append(QPointF(left, top));
    rect_points.append(QPointF(right, top));
    rect_points.append(QPointF(right, bottom));
    rect_points.append(QPointF(left, bottom));
    rect_points.append(QPointF(left, top)); // Close

    CutPath path;
    path.points = rect_points;
    path.is_closed = true;
    path.speed_mm_s = 50.0;
    path.pressure_grams = 100.0;

    result.paths.append(path);
    result.bounding_box = QRectF(left, top, right - left, bottom - top);
    result.total_length_mm = calculate_total_length(rect_points);

    return result;
}

CutContourResult CutContourGenerator::generate_bleed_cut(
    const QVector<QPointF>& original_contour,
    double bleed_mm) {

    CutContourResult result;

    if (original_contour.isEmpty()) {
        return result;
    }

    QVector<QPointF> offset_points;
    for (const auto& pt : original_contour) {
        // Simple offset - in production use proper contour offsetting
        QPointF offset_pt(pt.x() - bleed_mm, pt.y() - bleed_mm);
        offset_points.append(offset_pt);
    }

    CutPath path;
    path.points = offset_points;
    path.is_closed = true;

    result.paths.append(path);
    result.bounding_box = bounding_box(offset_points);
    result.total_length_mm = calculate_total_length(offset_points);

    return result;
}

CutContourResult CutContourGenerator::offset_contour(
    const QVector<QPointF>& contour,
    double offset_mm) {

    CutContourResult result;

    if (contour.isEmpty()) {
        return result;
    }

    QVector<QPointF> offset_points;
    for (const auto& pt : contour) {
        offset_points.append(QPointF(pt.x() + offset_mm, pt.y() + offset_mm));
    }

    CutPath path;
    path.points = offset_points;
    path.is_closed = true;

    result.paths.append(path);
    result.bounding_box = bounding_box(offset_points);
    result.total_length_mm = calculate_total_length(offset_points);

    return result;
}

CutContourResult CutContourGenerator::merge_contours(
    const QVector<CutContourResult>& contours,
    double max_gap_mm) {

    CutContourResult result;

    QVector<QPointF> all_points;
    for (const auto& contour : contours) {
        for (const auto& path : contour.paths) {
            all_points.append(path.points);
        }
    }

    if (all_points.isEmpty()) {
        return result;
    }

    // Sort by proximity and merge
    QVector<QPointF> merged;
    QVector<bool> used(all_points.size(), false);

    for (int i = 0; i < all_points.size(); ++i) {
        if (used[i]) continue;

        QVector<QPointF> current_chain;
        current_chain.append(all_points[i]);
        used[i] = true;

        for (int j = 0; j < all_points.size(); ++j) {
            if (used[j]) continue;

            QPointF last = current_chain.last();
            QPointF next = all_points[j];
            double dist = std::sqrt(std::pow(next.x() - last.x(), 2) +
                                   std::pow(next.y() - last.y(), 2));

            if (dist < max_gap_mm) {
                current_chain.append(next);
                used[j] = true;
            }
        }

        if (!current_chain.isEmpty()) {
            CutPath path;
            path.points = current_chain;
            path.is_closed = true;
            result.paths.append(path);
        }
    }

    for (const auto& path : result.paths) {
        result.total_length_mm += calculate_total_length(path.points);
    }

    return result;
}

double CutContourGenerator::calculate_total_length(const QVector<QPointF>& path) {
    double length = 0.0;
    for (int i = 1; i < path.size(); ++i) {
        length += std::sqrt(std::pow(path[i].x() - path[i-1].x(), 2) +
                          std::pow(path[i].y() - path[i-1].y(), 2));
    }
    return length;
}

QRectF CutContourGenerator::bounding_box(const QVector<QPointF>& points) {
    if (points.isEmpty()) {
        return QRectF();
    }

    double min_x = points[0].x();
    double min_y = points[0].y();
    double max_x = points[0].x();
    double max_y = points[0].y();

    for (const auto& pt : points) {
        min_x = std::min(min_x, pt.x());
        min_y = std::min(min_y, pt.y());
        max_x = std::max(max_x, pt.x());
        max_y = std::max(max_y, pt.y());
    }

    return QRectF(min_x, min_y, max_x - min_x, max_y - min_y);
}

// CutMarkRenderer implementation
QByteArray CutMarkRenderer::render_registration_marks(
    const QRectF& page_bounds,
    double mark_length_mm,
    double mark_offset_mm) {

    QByteArray data;

    // Four corner marks
    QVector<QPointF> corners = {
        QPointF(page_bounds.left() + mark_offset_mm, page_bounds.top() + mark_offset_mm),
        QPointF(page_bounds.right() - mark_offset_mm, page_bounds.top() + mark_offset_mm),
        QPointF(page_bounds.left() + mark_offset_mm, page_bounds.bottom() - mark_offset_mm),
        QPointF(page_bounds.right() - mark_offset_mm, page_bounds.bottom() - mark_offset_mm)
    };

    // Crosshair marks at each corner
    for (const auto& corner : corners) {
        QString mark = QString("M%1,%2\n").arg(corner.x()).arg(corner.y());
        mark += QString("D%1,%2\n").arg(corner.x() + mark_length_mm).arg(corner.y());
        mark += QString("M%1,%2\n").arg(corner.x()).arg(corner.y());
        mark += QString("D%1,%2\n").arg(corner.x()).arg(corner.y() + mark_length_mm);
        data += mark.toLatin1();
    }

    return data;
}

QByteArray CutMarkRenderer::render_crop_marks(
    const QRectF& image_bounds,
    const QRectF& page_bounds,
    double overlap_mm) {

    QByteArray data;
    double mark_len = 5.0;

    // Top-left
    data += QString("M%1,%2\n").arg(image_bounds.left()).arg(image_bounds.top()).toLatin1();
    data += QString("D%1,%2\n").arg(image_bounds.left() - mark_len).arg(image_bounds.top()).toLatin1();
    data += QString("M%1,%2\n").arg(image_bounds.left()).arg(image_bounds.top()).toLatin1();
    data += QString("D%1,%2\n").arg(image_bounds.left()).arg(image_bounds.top() - mark_len).toLatin1();

    // Top-right
    data += QString("M%1,%2\n").arg(image_bounds.right()).arg(image_bounds.top()).toLatin1();
    data += QString("D%1,%2\n").arg(image_bounds.right() + mark_len).arg(image_bounds.top()).toLatin1();
    data += QString("M%1,%2\n").arg(image_bounds.right()).arg(image_bounds.top()).toLatin1();
    data += QString("D%1,%2\n").arg(image_bounds.right()).arg(image_bounds.top() - mark_len).toLatin1();

    // Bottom-left
    data += QString("M%1,%2\n").arg(image_bounds.left()).arg(image_bounds.bottom()).toLatin1();
    data += QString("D%1,%2\n").arg(image_bounds.left() - mark_len).arg(image_bounds.bottom()).toLatin1();
    data += QString("M%1,%2\n").arg(image_bounds.left()).arg(image_bounds.bottom()).toLatin1();
    data += QString("D%1,%2\n").arg(image_bounds.left()).arg(image_bounds.bottom() + mark_len).toLatin1();

    // Bottom-right
    data += QString("M%1,%2\n").arg(image_bounds.right()).arg(image_bounds.bottom()).toLatin1();
    data += QString("D%1,%2\n").arg(image_bounds.right() + mark_len).arg(image_bounds.bottom()).toLatin1();
    data += QString("M%1,%2\n").arg(image_bounds.right()).arg(image_bounds.bottom()).toLatin1();
    data += QString("D%1,%2\n").arg(image_bounds.right()).arg(image_bounds.bottom() + mark_len).toLatin1();

    return data;
}

QByteArray CutMarkRenderer::render_barcode(
    const QString& job_id,
    const QPointF& position,
    double width_mm,
    double height_mm) {

    // Code 39 barcode encoding
    QByteArray data;

    data += QString("M%1,%2\n").arg(position.x()).arg(position.y()).toLatin1();

    // Simplified barcode - draw as rectangles
    double bar_width = width_mm / 36.0; // Code 39 uses 36 bars for alphanumerics
    for (int i = 0; i < job_id.size() && i < 10; ++i) {
        QChar c = job_id[i];
        int char_idx;
        if (c.isDigit()) {
            char_idx = c.digitValue();
        } else if (c.isLetter()) {
            char_idx = 10 + (c.toUpper().unicode() - 'A');
        } else {
            char_idx = 38; // Space
        }

        // Each character is encoded as 5 bars
        static const int patterns[][6] = {
            {1,0,1,0,1,0}, // 0
            {0,1,0,0,1,0}, // 1
            {1,0,0,0,0,1}, // 2
            {0,1,0,0,0,1}, // 3
            {1,0,1,0,0,0}, // 4
            {0,1,1,0,0,0}, // 5
            {1,0,0,0,1,0}, // 6
            {0,1,0,0,1,0}, // 7
            {0,0,1,0,0,1}, // 8
            {1,0,0,0,0,1}, // 9
        };

        double x_pos = position.x() + i * (bar_width * 10);
        for (int bar = 0; bar < 5; ++bar) {
            double bar_h = patterns[char_idx % 10][bar] ? height_mm : height_mm * 0.6;
            data += QString("D%1,%2\n").arg(x_pos + bar * bar_width * 2).arg(position.y() + (height_mm - bar_h)).toLatin1();
        }
    }

    return data;
}

// OPOSCommandBuilder implementation
QByteArray OPOSCommandBuilder::initialize() {
    return QByteArray("INIT\r");
}

QByteArray OPOSCommandBuilder::set_media_type(const QString& media) {
    return QString("MT%1\r").arg(media).toLatin1();
}

QByteArray OPOSCommandBuilder::cut(double length_mm) {
    if (length_mm > 0) {
        return QString("C%1\r").arg(length_mm).toLatin1();
    }
    return QByteArray("C\r");
}

QByteArray OPOSCommandBuilder::cut_with_eject(double length_mm) {
    return QString("E%1\r").arg(length_mm).toLatin1();
}

QByteArray OPOSCommandBuilder::partial_cut(double depth_percent) {
    return QString("PC%1\r").arg(depth_percent).toLatin1();
}

QByteArray OPOSCommandBuilder::set_speed(double mm_s) {
    return QString("VS%1\r").arg(mm_s).toLatin1();
}

QByteArray OPOSCommandBuilder::set_pressure(double grams) {
    return QString("FP%1\r").arg(grams).toLatin1();
}

QByteArray OPOSCommandBuilder::feed_and_cut(double feed_mm) {
    return QString("FC%1\r").arg(feed_mm).toLatin1();
}

QByteArray OPOSCommandBuilder::back_feed(double mm) {
    return QString("BK%1\r").arg(mm).toLatin1();
}

QByteArray OPOSCommandBuilder::set_autofeed(bool enabled) {
    return QString("AF%1\r").arg(enabled ? 1 : 0).toLatin1();
}

QByteArray OPOSCommandBuilder::query_status() {
    return QByteArray("?\r");
}

QByteArray OPOSCommandBuilder::raw_bytes(const QByteArray& data) {
    return data;
}

// ARMSCommandBuilder implementation
QByteArray ARMSCommandBuilder::begin_job(const QString& job_id) {
    return QString("%%!RIP_ARS_1.0\n%%JobID:%1\n").arg(job_id).toLatin1();
}

QByteArray ARMSCommandBuilder::set_media_width(double width_mm) {
    return QString("%%MediaWidth:%1\n").arg(width_mm).toLatin1();
}

QByteArray ARMSCommandBuilder::set_media_length(double length_mm) {
    return QString("%%MediaLength:%1\n").arg(length_mm).toLatin1();
}

QByteArray ARMSCommandBuilder::add_cut_path(const CutPath& path) {
    QByteArray data;
    for (int i = 0; i < path.points.size(); ++i) {
        if (i == 0) {
            data += QString("%%MoveTo:%1,%2\n").arg(path.points[i].x()).arg(path.points[i].y()).toLatin1();
        } else {
            data += QString("%%LineTo:%1,%2\n").arg(path.points[i].x()).arg(path.points[i].y()).toLatin1();
        }
    }
    data += "%%ClosePath\n";
    return data;
}

QByteArray ARMSCommandBuilder::end_job() {
    return QByteArray("%%EOF\n");
}

QByteArray ARMSCommandBuilder::execute() {
    return QByteArray("%%ExecuteCut\n");
}

// SCPLCommandBuilder implementation
QByteArray SCPLCommandBuilder::initialize() {
    return QByteArray("!R1\n"); // Reset
}

QByteArray SCPLCommandBuilder::set_page_size(double width_mm, double height_mm) {
    return QString("!S1,%1,%2\n").arg(width_mm).arg(height_mm).toLatin1();
}

QByteArray SCPLCommandBuilder::move_to(double x_mm, double y_mm) {
    return QString("PU;PA%1,%2;\n").arg(x_mm).arg(y_mm).toLatin1();
}

QByteArray SCPLCommandBuilder::line_to(double x_mm, double y_mm) {
    return QString("PD;PA%1,%2;\n").arg(x_mm).arg(y_mm).toLatin1();
}

QByteArray SCPLCommandBuilder::arc(double cx_mm, double cy_mm, double radius_mm, bool clockwise) {
    QString cmd = QString("PD;PA%1,%2;\n").arg(cx_mm + radius_mm).arg(cy_mm).toLatin1();
    cmd += QString("CI%1,%2;\n").arg(radius_mm).arg(clockwise ? 1 : 0).toLatin1();
    return cmd.toLatin1();
}

QByteArray SCPLCommandBuilder::set_speed(double mm_s) {
    return QString("!V1,%1\n").arg(mm_s).toLatin1();
}

QByteArray SCPLCommandBuilder::set_pressure(double grams) {
    return QString("!C1,%1\n").arg(grams).toLatin1();
}

QByteArray SCPLCommandBuilder::cut() {
    return QByteArray("!D1;\n");
}

QByteArray SCPLCommandBuilder::end_plot() {
    return QByteArray("!D0;\nEO;\n");
}

}
