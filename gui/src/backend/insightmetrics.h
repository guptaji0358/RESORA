#pragma once
// Numbers and statements behind the Image Comparison visuals (Qt Core only, unit-testable).
// Derived from the same two `properties` entries as Comparison; nothing is re-read from disk.
// Missing or invalid values (absent, zero, negative) are flagged, never plotted as real data.
// Verdict statements describe pixel count / storage / shape only and never judge image quality.
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

struct Comparison;

enum class Side { None, A, B, Tie };

struct SideFacts
{
    QString name;
    bool hasSize = false;   // width/height present AND both > 0
    qint64 w = 0, h = 0;
    double mp = 0;
    bool hasBytes = false;  // file size present and >= 0 (0 is a real, if odd, value)
    qint64 bytes = 0;
    bool hasAspect = false; // aspect ratio present and > 0
    double aspect = 0;
    QString aspectLabel;    // "16:9", or a decimal "1.78:1" when no common name exists

    qint64 pixels() const { return hasSize ? w * h : 0; }
};

struct MetaRow
{
    QString label, a, b;
    bool aAvailable = true, bAvailable = true;
    bool differs = false;
};

struct InsightMetrics
{
    SideFacts a, b;

    // Resolution
    bool pixelsComparable = false;
    Side morePixels = Side::None;
    double pixelRatio = 1;     // larger / smaller pixel count (>= 1)
    double mpDifference = 0;   // absolute, megapixels
    double pixelPercent = 0;   // larger vs smaller, e.g. 300 = "300% more pixels"

    // File size
    bool sizesComparable = false;
    Side largerFile = Side::None;
    qint64 byteDifference = 0;
    bool bytePercentValid = false;  // false when the smaller file is 0 bytes
    double bytePercent = 0;         // larger vs smaller

    // Dimensions: B relative to A, only when both sizes are valid.
    double widthRatio = 0, heightRatio = 0;

    // Aspect ratio
    bool aspectComparable = false;
    bool aspectSame = false;        // within 0.5 % (relative)
    double aspectRelDiff = 0;       // |a-b| / smaller, as a fraction

    QStringList verdict;            // concise, data-derived statements (Quick Verdict)
    QList<MetaRow> meta;            // Format, Color mode, DPI/PPI, Color profile, EXIF, Orientation
};

namespace InsightMetricsBuilder {

constexpr double kAspectSameTolerance = 0.005;  // 0.5 % relative: tiny differences are not shown as differences

InsightMetrics build(const QJsonObject &leftProps, const QJsonObject &rightProps, const Comparison &c);

// "16:9" when the file reports a common name, otherwise a decimal ratio "1.33:1"; empty when invalid.
QString aspectText(const QJsonObject &props);

// "4.00×" style multiplier; "1,920 × 1,080" style size.
QString multiplier(double ratio);
QString sizeText(qint64 w, qint64 h);

}  // namespace InsightMetricsBuilder
