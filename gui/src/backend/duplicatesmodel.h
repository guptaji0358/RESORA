#pragma once
// Parses the engine's `duplicates` result (docs/DUPLICATES.md) into plain structs and builds the
// request params. Qt Core only. Nothing is computed here beyond reading values: the engine owns the
// savings arithmetic. Exact and visually similar groups stay in separate lists on purpose.
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

struct DupFile
{
    QString path, name, folder, format, resolution, category;
    int width = 0, height = 0;
    qint64 size = -1;
    double similarity = -1;  // -1 = not applicable (exact duplicates)
    bool anchor = false;
    int identicalCopies = 0;
};

struct DupGroup
{
    QString id;
    bool exact = true;
    QString category;       // similar groups: weakest member's category
    double lowest = -1;     // similar groups: lowest similarity in the group
    qint64 totalBytes = 0, savingsBytes = 0;  // savings only for exact groups
    QList<DupFile> files;
};

struct DupStats
{
    qint64 total = 0, empty = 0, exactGroups = 0, exactFiles = 0, exactExtra = 0, exactBytes = 0, exactSavings = 0;
    qint64 similarGroups = 0, similarFiles = 0, similarBytes = 0, skipped = 0, cacheHits = 0, flat = 0;
};

struct DupResult
{
    bool valid = false;
    DupStats stats;
    QList<DupGroup> exact, similar;
    QStringList skipped;  // "path — reason"
    bool skippedTruncated = false;
    QString cacheWarning;
    double threshold = 90;
    bool similarEnabled = true;
};

struct DupRequest
{
    QStringList folders, exclude, files;  // files = an imported collection
    bool recursive = true, similar = true;
    double threshold = 90;
    QString cachePath;

    QByteArray paramsJson() const;  // written to the params file
};

namespace DuplicatesModel {
DupResult fromJson(const QJsonObject &data);
QString bytesText(qint64 bytes);
QString similarityText(double percent);  // 96.9 -> "96.9%"
QString groupTitle(const DupGroup &g);   // "3 identical files" / "2 visually similar images"
}  // namespace DuplicatesModel
