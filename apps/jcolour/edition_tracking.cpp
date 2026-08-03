#include "edition_tracking.h"
#include "edition_exif.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QDir>
#include <QDebug>

namespace jcolour {

EditionManager& EditionManager::instance() {
    static EditionManager instance;
    return instance;
}

EditionManager::EditionManager(QObject* parent)
    : QObject(parent)
    , nextEditionNumber_(1)
{
    loadFromStorage();
}

int EditionManager::createEdition(const QString& name, const QString& description,
                                   int maxCount, bool isLimited) {
    Edition edition;
    edition.number = nextEditionNumber_++;
    edition.name = name;
    edition.description = description;
    edition.createdAt = QDateTime::currentDateTime();
    edition.printedCount = 0;
    edition.maxCount = maxCount;
    edition.isLimited = isLimited;
    edition.imageHash = "";

    editions_[edition.number] = edition;
    saveToStorage();

    emit editionCreated(edition.number);
    return edition.number;
}

Edition EditionManager::getEdition(int number) const {
    return editions_.value(number);
}

QVector<Edition> EditionManager::listEditions() const {
    QVector<Edition> result;
    for (const auto& edition : editions_) {
        result.append(edition);
    }
    // Sort by number descending (newest first)
    std::sort(result.begin(), result.end(), [](const Edition& a, const Edition& b) {
        return a.number > b.number;
    });
    return result;
}

bool EditionManager::updateEdition(int number, const QString& name, const QString& description) {
    if (!editions_.contains(number)) return false;

    editions_[number].name = name;
    editions_[number].description = description;
    saveToStorage();

    emit editionUpdated(number);
    return true;
}

bool EditionManager::deleteEdition(int number) {
    if (!editions_.contains(number)) return false;
    editions_.remove(number);
    saveToStorage();
    return true;
}

bool EditionManager::recordPrint(int editionNumber, const QString& imagePath) {
    if (!editions_.contains(editionNumber)) return false;

    Edition& edition = editions_[editionNumber];
    edition.printedCount++;

    // Compute hash if not already set
    if (edition.imageHash.isEmpty() && !imagePath.isEmpty()) {
        edition.imageHash = computeImageHash(imagePath);
    }

    saveToStorage();

    emit editionPrinted(editionNumber, edition.printedCount);

    // Check if limited edition is now complete
    if (edition.isLimited && edition.printedCount >= edition.maxCount) {
        emit editionCompleted(editionNumber);
    }

    return true;
}

bool EditionManager::recordPrintWithExif(int editionNumber, const QString& srcPath,
                                         const QString& dstPath, const QString& artist,
                                         const QString& copyright) {
    if (!editions_.contains(editionNumber)) return false;
    if (!recordPrint(editionNumber, srcPath)) return false;

    const Edition& e = editions_[editionNumber];
    jcolour::exif::EditionFields f;
    f.title         = e.name;
    f.artist        = artist;
    f.copyright     = copyright;
    f.imageHash     = e.imageHash;
    f.dateTime      = QDateTime::currentDateTime().toString("yyyy:MM:dd HH:mm:ss");
    f.editionNumber = e.number;
    f.editionSize   = e.isLimited ? e.maxCount : 0;
    f.printNumber   = e.printedCount;   // recordPrint already incremented
    return jcolour::exif::writeEditionTiff(srcPath, dstPath, f);
}

int EditionManager::getPrintedCount(int editionNumber) const {
    if (!editions_.contains(editionNumber)) return 0;
    return editions_[editionNumber].printedCount;
}

bool EditionManager::isEditionComplete(int editionNumber) const {
    if (!editions_.contains(editionNumber)) return false;
    const Edition& edition = editions_[editionNumber];
    if (!edition.isLimited) return false;
    return edition.printedCount >= edition.maxCount;
}

EditionStatistics EditionManager::getStatistics() const {
    EditionStatistics stats;
    stats.totalEditions = editions_.size();
    stats.totalPrints = 0;
    stats.limitedEditionCount = 0;
    stats.openEditionCount = 0;
    stats.lastPrintDate = QDateTime();

    for (const auto& edition : editions_) {
        stats.totalPrints += edition.printedCount;
        if (edition.isLimited) {
            stats.limitedEditionCount++;
        } else {
            stats.openEditionCount++;
        }
        if (!edition.createdAt.isNull() && (stats.lastPrintDate.isNull() ||
            edition.createdAt > stats.lastPrintDate)) {
            // Note: in production would track actual print dates
        }
    }

    return stats;
}

QString EditionManager::generateEditionReport(int editionNumber) const {
    if (!editions_.contains(editionNumber)) return QString();

    const Edition& edition = editions_[editionNumber];
    QString report;

    report += QString("Edition #%1\n").arg(editionNumber);
    report += QString("══════════════════════════════════\n");
    report += QString("Name: %1\n").arg(edition.name);
    report += QString("Description: %2\n").arg(edition.description);
    report += QString("Created: %1\n").arg(edition.createdAt.toString(Qt::ISODate));
    report += QString("Type: %1\n").arg(edition.isLimited ? "Limited" : "Open");

    if (edition.isLimited) {
        report += QString("Prints: %1 / %2\n").arg(edition.printedCount).arg(edition.maxCount);
        double pct = (edition.maxCount > 0)
            ? (edition.printedCount * 100.0 / edition.maxCount)
            : 0;
        report += QString("Progress: %1%\n").arg(pct, 0, 'f', 1);
    } else {
        report += QString("Total Prints: %1\n").arg(edition.printedCount);
    }

    if (!edition.imageHash.isEmpty()) {
        report += QString("Image Hash: %1\n").arg(edition.imageHash);
    }

    return report;
}

QString EditionManager::exportEditionsToJSON() const {
    QJsonArray array;
    for (const auto& edition : editions_) {
        QJsonObject obj;
        obj["number"] = edition.number;
        obj["name"] = edition.name;
        obj["description"] = edition.description;
        obj["createdAt"] = edition.createdAt.toString(Qt::ISODate);
        obj["printedCount"] = edition.printedCount;
        obj["maxCount"] = edition.maxCount;
        obj["isLimited"] = edition.isLimited;
        obj["imageHash"] = edition.imageHash;
        array.append(obj);
    }

    QJsonDocument doc(array);
    return doc.toJson(QJsonDocument::Indented);
}

bool EditionManager::importEditionsFromJSON(const QString& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) return false;

    QJsonArray array = doc.array();
    for (const auto& val : array) {
        QJsonObject obj = val.toObject();
        Edition edition;
        edition.number = obj["number"].toInt();
        edition.name = obj["name"].toString();
        edition.description = obj["description"].toString();
        edition.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
        edition.printedCount = obj["printedCount"].toInt();
        edition.maxCount = obj["maxCount"].toInt();
        edition.isLimited = obj["isLimited"].toBool();
        edition.imageHash = obj["imageHash"].toString();

        editions_[edition.number] = edition;
        if (edition.number >= nextEditionNumber_) {
            nextEditionNumber_ = edition.number + 1;
        }
    }

    saveToStorage();
    return true;
}

QString EditionManager::computeImageHash(const QString& path) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QString();

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return QString(hash.result().toHex());
}

void EditionManager::saveToStorage() {
    QSettings settings("jcolour", "editions");
    settings.setValue("nextNumber", nextEditionNumber_);

    QJsonArray array;
    for (const auto& edition : editions_) {
        QJsonObject obj;
        obj["number"] = edition.number;
        obj["name"] = edition.name;
        obj["description"] = edition.description;
        obj["createdAt"] = edition.createdAt.toString(Qt::ISODate);
        obj["printedCount"] = edition.printedCount;
        obj["maxCount"] = edition.maxCount;
        obj["isLimited"] = edition.isLimited;
        obj["imageHash"] = edition.imageHash;
        array.append(obj);
    }
    settings.setValue("editions", QString(QJsonDocument(array).toJson()));
}

void EditionManager::loadFromStorage() {
    QSettings settings("jcolour", "editions");
    nextEditionNumber_ = settings.value("nextNumber", 1).toInt();

    QString json = settings.value("editions").toString();
    if (json.isEmpty()) return;

    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) return;

    QJsonArray array = doc.array();
    for (const auto& val : array) {
        QJsonObject obj = val.toObject();
        Edition edition;
        edition.number = obj["number"].toInt();
        edition.name = obj["name"].toString();
        edition.description = obj["description"].toString();
        edition.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
        edition.printedCount = obj["printedCount"].toInt();
        edition.maxCount = obj["maxCount"].toInt();
        edition.isLimited = obj["isLimited"].toBool();
        edition.imageHash = obj["imageHash"].toString();

        editions_[edition.number] = edition;
    }
}

} // namespace jcolour