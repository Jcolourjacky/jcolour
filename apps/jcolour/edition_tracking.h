#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVector>
#include <QMap>

namespace jcolour {

struct Edition {
    int number;
    QString name;
    QString description;
    QDateTime createdAt;
    int printedCount;
    int maxCount; // 0 = unlimited
    bool isLimited;
    QString imageHash; // SHA-256 of source image
};

struct EditionStatistics {
    int totalEditions;
    int totalPrints;
    int limitedEditionCount;
    int openEditionCount;
    QDateTime lastPrintDate;
};

class EditionManager : public QObject {
    Q_OBJECT
public:
    static EditionManager& instance();

    // Edition CRUD
    int createEdition(const QString& name, const QString& description,
                      int maxCount = 0, bool isLimited = false);
    Edition getEdition(int number) const;
    QVector<Edition> listEditions() const;
    bool updateEdition(int number, const QString& name, const QString& description);
    bool deleteEdition(int number);

    // Printing tracking
    bool recordPrint(int editionNumber, const QString& imagePath);
    // Record a print and embed edition provenance into the output TIFF's metadata
    // (baseline + EXIF). dstPath may equal srcPath for in-place tagging.
    bool recordPrintWithExif(int editionNumber, const QString& srcPath, const QString& dstPath,
                             const QString& artist = QString(),
                             const QString& copyright = QString());
    int getPrintedCount(int editionNumber) const;
    bool isEditionComplete(int editionNumber) const;

    // Statistics
    EditionStatistics getStatistics() const;
    QString generateEditionReport(int editionNumber) const;

    // Export
    QString exportEditionsToJSON() const;
    bool importEditionsFromJSON(const QString& json);

signals:
    void editionCreated(int number);
    void editionUpdated(int number);
    void editionPrinted(int number, int currentCount);
    void editionCompleted(int number);

private:
    EditionManager(QObject* parent = nullptr);
    ~EditionManager() = default;

    QString computeImageHash(const QString& path) const;
    void saveToStorage();
    void loadFromStorage();

    QMap<int, Edition> editions_;
    int nextEditionNumber_;
};

} // namespace jcolour