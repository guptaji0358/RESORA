#pragma once
// Parses the engine's `analytics` object (docs/ANALYTICS.md) into plain structs and formats numbers
// for display. Qt Core only (no widgets), so it is unit-testable. No statistic is computed here: the
// engine owns every definition; this layer only reads values and says "not available" for null,
// never 0.
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>

struct AnalyticsImage
{
    bool valid = false;  // false when the engine returned null (no image qualifies)
    QString path, name, format, resolution;
    int width = 0, height = 0;
    double megapixels = 0;
    qint64 fileSize = -1;  // -1 = unknown
    qint64 tied = 1;       // images sharing the same pixel count (highest/lowest only)
};

struct AnalyticsShare
{
    QString name;
    qint64 count = 0;
    double percent = -1;  // -1 = not available (empty denominator)
    qint64 bytes = 0;
    double bytesPercent = -1;
};

struct AnalyticsFailure
{
    QString path, status, error;
};

struct AnalyticsData
{
    bool valid = false;
    qint64 total = 0, analyzed = 0, failed = 0, duplicatesIgnored = 0;
    qint64 totalBytes = 0, failedBytes = 0, sizeKnown = 0, sizeUnknown = 0;
    double averageBytes = -1;                   // -1 = not available (all doubles below)
    double averageWidth = -1, averageHeight = -1, averageMegapixels = -1, totalMegapixels = -1;
    AnalyticsImage largest, smallest, highest, lowest;
    int commonWidth = 0, commonHeight = 0;  // most frequent exact size; 0x0 = none
    qint64 commonCount = 0, commonTied = 0;  // images with it; sizes sharing that top count
    double commonPercent = -1;
    QList<AnalyticsShare> categories, orientation, formats;
    QList<QPair<QString, qint64>> failedByStatus;
    QList<AnalyticsFailure> failures;
    bool failuresTruncated = false;
    QList<QPair<QString, qint64>> otherFormats;
};

namespace AnalyticsModel {

// `analytics` is the object under the result's "analytics" key. An empty object gives valid == false.
AnalyticsData fromJson(const QJsonObject &analytics);

QString notAvailable();                 // "Not available"
QString bytesText(double bytes);        // 1536 -> "1.50 KB", -1 -> notAvailable()
QString percentText(double percent);    // 12.345 -> "12.3%", -1 -> "—"
QString megapixelsText(double mp);      // 2.0736 -> "2.07 MP", -1 -> notAvailable()
QString sizeText(int width, int height);// "1,920 × 1,080"
QString imageSummary(const AnalyticsImage &image);  // "3,840 × 2,160 · 8.29 MP · UHD 4K"
QString statusText(const QString &status);          // "corrupted" -> "Corrupted", "invalid_metadata" -> "Invalid dimensions"

}  // namespace AnalyticsModel
