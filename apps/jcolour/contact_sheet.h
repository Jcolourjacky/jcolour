#pragma once

#include <QWidget>
#include <QString>
#include <QVector>
#include <QImage>
#include <QSize>
#include <QSpinBox>
#include <QLabel>

namespace jcolour {

struct ContactSheetConfig {
    int columns = 4;
    int rows = 5;
    double cellWidthMm = 50;
    double cellHeightMm = 50;
    double marginMm = 10;
    double spacingMm = 5;
    bool showLabels = true;
    bool showImageNumbers = true;
    QString paperSize; // "A4", "Letter", etc.
    int fontSize = 10;
    QString fontFamily = "Arial";
};

struct ContactSheetImage {
    QString path;
    QString label;
    int number;
};

class ContactSheetGenerator {
public:
    static QImage generate(const QVector<ContactSheetImage>& images,
                           const ContactSheetConfig& config);
    static QImage generateFromPaths(const QStringList& paths,
                                    const ContactSheetConfig& config);
};

class ContactSheetDialog : public QWidget {
    Q_OBJECT
public:
    explicit ContactSheetDialog(QWidget* parent = nullptr);
    ~ContactSheetDialog() override;

    void setImages(const QStringList& paths);
    void setConfig(const ContactSheetConfig& config);
    ContactSheetConfig getConfig() const;

    QImage generatePreview() const;
    QString saveToFile(const QString& path) const;

signals:
    void configChanged();

private slots:
    void onColumnsChanged(int val);
    void onRowsChanged(int val);
    void onFontChanged();
    void onBrowseOutput();

private:
    void updatePreview();
    void loadThumbnails();

    QStringList imagePaths_;
    ContactSheetConfig config_;
    QVector<QImage> thumbnails_;
    QLabel* previewLabel_;
    QSpinBox* columnsSpin_;
    QSpinBox* rowsSpin_;
    QSpinBox* fontSizeSpin_;
};

} // namespace jcolour