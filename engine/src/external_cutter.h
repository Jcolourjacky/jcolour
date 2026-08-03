#pragma once

#include <QString>
#include <QVector>
#include <QRectF>
#include <cstdint>

namespace jcolour {

enum class CutterModel {
    SureCut_SF,
    SureCut_S2,
    SureCut_S3,
    FC9000,
    FC8600,
    Generic
};

enum class CutContourMode {
    Standard,      // Standard 3mm border
    Oversized,     // Oversized by 5mm
    Exact,         // No border, exact cut
    InsideContour, // Cut inside image border
    OutsideContour // Cut outside image border
};

// NOTE: OPOS and ARMS are registration-mark *sensing* systems, not plotter
// command languages — they let the cutter optically read printed marks and align
// the cut to the print. The actual on-wire dialects are GP-GL / HP-GL / SCPL.
// The OPOS/ARMS entries here select the mark-registration workflow; the byte
// stream is still emitted in the model's plotter language. (See CUTTER-ASSESSMENT.md.)
enum class CutLanguage {
    OPOS,          // Graphtec Optical POSitioning System (mark sensing)
    ARMS,          // Roland Automatic Mark Registration System (mark sensing)
    SCPL,          // Summa Cutter Plotter Language
    GPGL,          // Graphtec Plotter Language (GP-GL)
    HPGL           // HP-GL/2
};

struct CutJob {
    QString job_id;
    QString media_name;
    int copies = 1;
    bool cut_after_print = true;
    CutLanguage language = CutLanguage::OPOS;
};

struct CutPath {
    QVector<QPointF> points;
    double speed_mm_s = 50.0;
    double pressure_grams = 100.0;
    bool is_closed = true;
};

struct CutContourResult {
    QVector<CutPath> paths;
    QRectF bounding_box;
    double total_length_mm = 0.0;
};

struct CutterCapabilities {
    bool supports_oversized_cut = false;
    bool supports_multi_copy_cut = false;
    bool supports_piece_cut = false;
    bool supports_rotation = false;
    double max_speed_mm_s = 100.0;
    double min_speed_mm_s = 10.0;
    int max_copies = 1;
};

class ExternalCutter {
public:
    ExternalCutter();
    ~ExternalCutter();

    static QString model_name(CutterModel model);
    static CutterCapabilities capabilities(CutterModel model);

    bool connect(const QString& port, int baud_rate = 9600);
    void disconnect();

    bool is_connected() const { return connected_; }
    CutterModel model() const { return model_; }

    bool send_cut_data(const QByteArray& data);
    bool send_cut_contour(const CutContourResult& contour);
    bool execute_cut();

    bool set_speed(double mm_s);
    bool set_pressure(double grams);
    bool set_mode(CutContourMode mode);

    QString last_error() const { return error_message_; }

private:
    bool send_command(const QByteArray& cmd);
    QByteArray build_opos_command(const QString& cmd);
    QByteArray build_arms_command(const QString& cmd);
    QByteArray build_scpl_command(const QString& cmd);

    bool parse_response(const QByteArray& response);

    CutterModel model_ = CutterModel::SureCut_SF;
    QString port_name_;
    int baud_rate_ = 9600;
    bool connected_ = false;
    QString error_message_;

    CutContourMode mode_ = CutContourMode::Standard;
    double speed_ = 50.0;
    double pressure_ = 100.0;
};

class CutContourGenerator {
public:
    static CutContourResult generate_from_image(
        int image_width_px, int image_height_px,
        double dpi,
        CutContourMode mode = CutContourMode::Standard,
        double border_mm = 3.0);

    static CutContourResult generate_rectangle(
        int image_width_px, int image_height_px,
        double dpi,
        double border_mm = 3.0);

    static CutContourResult generate_bleed_cut(
        const QVector<QPointF>& original_contour,
        double bleed_mm);

    static CutContourResult offset_contour(
        const QVector<QPointF>& contour,
        double offset_mm);

    static CutContourResult merge_contours(
        const QVector<CutContourResult>& contours,
        double max_gap_mm = 5.0);

    static double calculate_total_length(const QVector<QPointF>& path);
    static QRectF bounding_box(const QVector<QPointF>& points);
};

class CutMarkRenderer {
public:
    static QByteArray render_registration_marks(
        const QRectF& page_bounds,
        double mark_length_mm = 10.0,
        double mark_offset_mm = 5.0);

    static QByteArray render_crop_marks(
        const QRectF& image_bounds,
        const QRectF& page_bounds,
        double overlap_mm = 0.0);

    static QByteArray render_barcode(
        const QString& job_id,
        const QPointF& position,
        double width_mm = 30.0,
        double height_mm = 10.0);
};

class OPOSCommandBuilder {
public:
    static QByteArray initialize();
    static QByteArray set_media_type(const QString& media);
    static QByteArray cut(double length_mm);
    static QByteArray cut_with_eject(double length_mm);
    static QByteArray partial_cut(double depth_percent = 70);
    static QByteArray set_speed(double mm_s);
    static QByteArray set_pressure(double grams);
    static QByteArray feed_and_cut(double feed_mm);
    static QByteArray back_feed(double mm);
    static QByteArray set_autofeed(bool enabled);
    static QByteArray query_status();
    static QByteArray raw_bytes(const QByteArray& data);
};

class ARMSCommandBuilder {
public:
    static QByteArray begin_job(const QString& job_id);
    static QByteArray set_media_width(double width_mm);
    static QByteArray set_media_length(double length_mm);
    static QByteArray add_cut_path(const CutPath& path);
    static QByteArray end_job();
    static QByteArray execute();
};

class SCPLCommandBuilder {
public:
    static QByteArray initialize();
    static QByteArray set_page_size(double width_mm, double height_mm);
    static QByteArray move_to(double x_mm, double y_mm);
    static QByteArray line_to(double x_mm, double y_mm);
    static QByteArray arc(double cx_mm, double cy_mm, double radius_mm, bool clockwise);
    static QByteArray set_speed(double mm_s);
    static QByteArray set_pressure(double grams);
    static QByteArray cut();
    static QByteArray end_plot();
};

}