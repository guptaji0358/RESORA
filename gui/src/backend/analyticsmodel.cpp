#include "backend/analyticsmodel.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QLocale>

#include "ui/uiformat.h"

namespace {
double num(const QJsonObject &o, const char *key)  // null / missing -> -1 ("not available")
{
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isDouble() ? v.toDouble() : -1.0;
}

qint64 whole(const QJsonObject &o, const char *key, qint64 fallback = 0)
{
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isDouble() ? v.toInteger(fallback) : fallback;  // toInteger keeps exact 64-bit values
}

QString str(const QJsonObject &o, const char *key)
{
    return o.value(QLatin1String(key)).toString();
}

AnalyticsImage image(const QJsonValue &value)
{
    AnalyticsImage i;
    if (!value.isObject())
        return i;
    const QJsonObject o = value.toObject();
    i.valid = true;
    i.path = str(o, "path");
    i.name = str(o, "name");
    i.format = str(o, "format");
    i.resolution = str(o, "resolution");
    i.width = int(whole(o, "width"));
    i.height = int(whole(o, "height"));
    i.megapixels = num(o, "megapixels");
    i.fileSize = whole(o, "file_size", -1);
    i.tied = whole(o, "tied", 1);
    return i;
}

QList<AnalyticsShare> shares(const QJsonValue &value)
{
    QList<AnalyticsShare> out;
    for (const QJsonValue &v : value.toArray()) {
        const QJsonObject o = v.toObject();
        out.append({str(o, "name"), whole(o, "count"), num(o, "percent"), whole(o, "bytes"), num(o, "bytes_percent")});
    }
    return out;
}

QList<QPair<QString, qint64>> counts(const QJsonObject &o)
{
    QList<QPair<QString, qint64>> out;
    for (auto it = o.begin(); it != o.end(); ++it)
        out.append({it.key(), it.value().toInteger()});
    return out;
}
}  // namespace

namespace AnalyticsModel {

AnalyticsData fromJson(const QJsonObject &a)
{
    AnalyticsData d;
    if (a.isEmpty() || !a.value(QLatin1String("collection")).isObject())
        return d;
    d.valid = true;
    const QJsonObject c = a.value(QLatin1String("collection")).toObject();
    const QJsonObject r = a.value(QLatin1String("resolution")).toObject();
    d.total = whole(c, "total");
    d.analyzed = whole(c, "analyzed");
    d.failed = whole(c, "failed");
    d.duplicatesIgnored = whole(c, "duplicates_ignored");
    d.totalBytes = whole(c, "total_bytes");
    d.failedBytes = whole(c, "failed_bytes");
    d.sizeKnown = whole(c, "size_known");
    d.sizeUnknown = whole(c, "size_unknown");
    d.averageBytes = num(c, "average_bytes");
    d.largest = image(c.value(QLatin1String("largest")));
    d.smallest = image(c.value(QLatin1String("smallest")));
    d.failedByStatus = counts(c.value(QLatin1String("failed_by_status")).toObject());
    d.averageWidth = num(r, "average_width");
    d.averageHeight = num(r, "average_height");
    d.averageMegapixels = num(r, "average_megapixels");
    d.totalMegapixels = num(r, "total_megapixels");
    d.highest = image(r.value(QLatin1String("highest")));
    d.lowest = image(r.value(QLatin1String("lowest")));
    const QJsonValue common = r.value(QLatin1String("most_common"));
    if (common.isObject()) {
        const QJsonObject m = common.toObject();
        d.commonWidth = int(whole(m, "width"));
        d.commonHeight = int(whole(m, "height"));
        d.commonCount = whole(m, "count");
        d.commonTied = whole(m, "tied", 1);
        d.commonPercent = num(m, "percent");
    }
    d.categories = shares(a.value(QLatin1String("categories")));
    d.orientation = shares(a.value(QLatin1String("orientation")));
    d.formats = shares(a.value(QLatin1String("formats")));
    d.otherFormats = counts(a.value(QLatin1String("other_formats")).toObject());
    for (const QJsonValue &v : a.value(QLatin1String("failures")).toArray()) {
        const QJsonObject o = v.toObject();
        d.failures.append({str(o, "path"), str(o, "status"), str(o, "error")});
    }
    d.failuresTruncated = a.value(QLatin1String("failures_truncated")).toBool();
    return d;
}

QString notAvailable()
{
    return QObject::tr("Not available");
}

QString bytesText(double bytes)
{
    if (bytes < 0)
        return notAvailable();
    if (bytes < 1024)
        return QObject::tr("%1 B").arg(QLocale(QLocale::English).toString(bytes, 'f', bytes == int(bytes) ? 0 : 1));
    double v = bytes;
    for (const char *unit : {"KB", "MB", "GB", "TB"}) {
        v /= 1024.0;
        if (v < 1024.0 || unit[0] == 'T')
            return QStringLiteral("%1 %2").arg(QLocale(QLocale::English).toString(v, 'f', 2), QLatin1String(unit));
    }
    return notAvailable();  // unreachable
}

QString percentText(double percent)
{
    return percent < 0 ? QStringLiteral("—") : QStringLiteral("%1%").arg(percent, 0, 'f', 1);
}

QString megapixelsText(double mp)
{
    return mp < 0 ? notAvailable() : QStringLiteral("%1 MP").arg(QLocale(QLocale::English).toString(mp, 'f', 2));
}

QString sizeText(int width, int height)
{
    return QStringLiteral("%1 × %2").arg(UiFormat::count(width), UiFormat::count(height));
}

QString imageSummary(const AnalyticsImage &i)
{
    if (!i.valid)
        return notAvailable();
    QString text = QStringLiteral("%1 · %2").arg(sizeText(i.width, i.height), megapixelsText(i.megapixels));
    if (!i.resolution.isEmpty())
        text += QStringLiteral(" · ") + i.resolution;
    return text;
}

QString statusText(const QString &status)
{
    if (status == QLatin1String("invalid_metadata"))
        return QObject::tr("Invalid dimensions");
    if (status.isEmpty())
        return QObject::tr("Unknown");
    return status.left(1).toUpper() + status.mid(1);
}

}  // namespace AnalyticsModel
