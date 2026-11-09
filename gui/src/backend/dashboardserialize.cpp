#include "backend/dashboardstate.h"

#include <QFileInfo>
#include <QSet>

#include "backend/analyticsmodel.h"
#include "backend/dashboardmodel.h"
#include "resora/exportcommon.h"

namespace dashboard {
namespace {

struct Role
{
    const char *name;
    const AnalyticsImage AnalyticsData::*image;
};
const Role kRoles[] = {{"largest", &AnalyticsData::largest},
                       {"smallest", &AnalyticsData::smallest},
                       {"highest", &AnalyticsData::highest},
                       {"lowest", &AnalyticsData::lowest}};

QJsonValue numOrNull(double v) { return v < 0 ? QJsonValue(QJsonValue::Null) : QJsonValue(v); }

QJsonObject chart(const QString &id, const QString &kind, const QString &title, const QString &from,
                  const QList<ChartItem> &items, bool storage)
{
    QJsonArray cats, a, b, c;
    for (const ChartItem &i : items) {
        cats << i.label;
        if (storage) {
            a << i.value;
            b << numOrNull(i.percent);
            c << numOrNull(i.secondary);
        } else {
            a << double(i.count);
            b << numOrNull(i.percent);
        }
    }
    QJsonArray series;
    if (storage)
        series = {QJsonObject{{"label", "Bytes"}, {"values", a}},
                  QJsonObject{{"label", "Share of storage (%)"}, {"values", b}},
                  QJsonObject{{"label", "Share of images (%)"}, {"values", c}}};
    else
        series = {QJsonObject{{"label", "Images"}, {"values", a}},
                  QJsonObject{{"label", "Share of analyzed images (%)"}, {"values", b}}};
    return QJsonObject{{"id", id}, {"kind", kind}, {"title", title}, {"data_source", from},
                       {"categories", cats}, {"series", series}};
}

}  // namespace

QJsonArray chartsFor(const QJsonObject &analyticsData)
{
    const AnalyticsData d = AnalyticsModel::fromJson(analyticsData);
    return {chart("chart.categories", "bar", "Resolution distribution", "analytics.categories",
                  DashboardModel::categoryItems(d), false),
            chart("chart.orientation", "donut", "Orientation", "analytics.orientation",
                  DashboardModel::orientationItems(d), false),
            chart("chart.formats", "donut", "Image formats", "analytics.formats",
                  DashboardModel::formatItems(d), false),
            chart("chart.storage", "hbar", "Storage by format", "analytics.formats",
                  DashboardModel::storageItems(d), true)};
}

Plan plan(const State &s)
{
    Plan p;
    QStringList problems;
    const QJsonObject data = s.result.value("analytics").toObject();
    const AnalyticsData d = AnalyticsModel::fromJson(data);
    if (!d.valid)
        problems << "The dashboard has no analytics to export";
    if (s.viewIndex != 0 && s.viewIndex != 1)
        problems << QStringLiteral("Unsupported view index %1").arg(s.viewIndex);
    if (!problems.isEmpty()) {
        p.error = problems.join('\n');
        return p;
    }

    QJsonArray widgets;
    QSet<QString> seen;
    for (const Role &r : kRoles) {
        const AnalyticsImage &im = d.*(r.image);
        if (!im.valid)
            continue;
        const QString path = im.path;
        if (path.isEmpty() || !QFileInfo(path).isAbsolute()) {
            problems << QStringLiteral("Image for '%1' has no absolute path: %2").arg(r.name, path);
            continue;
        }
        int index = -1;
        for (int i = 0; i < p.images.size(); ++i)
            if (QString::compare(p.images[i].sourcePath, path, resora::detail::kPathCase) == 0)
                index = i;
        if (index < 0) {
            resora::ExportImage e;
            e.sourcePath = path;
            e.name = im.name;
            e.metadata = QJsonObject{{"format", im.format.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(im.format)},
                                     {"width", im.width > 0 ? QJsonValue(im.width) : QJsonValue(QJsonValue::Null)},
                                     {"height", im.height > 0 ? QJsonValue(im.height) : QJsonValue(QJsonValue::Null)},
                                     {"file_size", im.fileSize >= 0 ? QJsonValue(im.fileSize) : QJsonValue(QJsonValue::Null)},
                                     {"status", "ok"},
                                     {"category", im.resolution.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(im.resolution)}};
            p.images << e;
            index = p.images.size() - 1;
        }
        widgets << QJsonObject{{"id", QStringLiteral("extreme.") + r.name}, {"role", r.name},
                               {"image_id", resora::imageId(index)}};
    }
    if (!problems.isEmpty()) {
        p.error = problems.join('\n');
        return p;
    }

    p.layout = QJsonObject{{"active_view", s.viewIndex == 0 ? "dashboard" : "detailed"},
                           {"visible_sections", QJsonValue(QJsonValue::Null)}, {"widgets", widgets}};
    p.charts = chartsFor(data);
    p.analytics = data;
    QJsonObject ctx = s.result;  // every non-analytics key is kept verbatim (nothing silently dropped)
    ctx.remove("analytics");
    p.config = QJsonObject{{"recursive", ctx.value("recursive").isBool() ? ctx.value("recursive") : QJsonValue(QJsonValue::Null)},
                           {"report_options", QJsonValue(QJsonValue::Null)},
                           {"scan_filters", QJsonValue(QJsonValue::Null)},
                           {"result_context", ctx}};
    const QString kind = ctx.value("source").toString();
    if (kind == "folder" || kind == "files")
        p.source = QJsonObject{{"kind", kind},
                               {"root", kind == "folder" ? QJsonValue(ctx.value("folder").toString()) : QJsonValue(QJsonValue::Null)}};
    p.ok = true;
    return p;
}

resora::ExportResult exportBundled(const State &s, const QString &dest, const QString &appVersion,
                                   const resora::ExportProgressFn &progress)
{
    const Plan p = plan(s);
    if (!p.ok) {
        resora::ExportResult r;
        r.error = resora::ExportError::InvalidInput;
        r.message = p.error;
        return r;
    }
    resora::BundledExportInput in;
    in.appVersion = appVersion;
    in.source = p.source;
    in.layout = p.layout;
    in.charts = p.charts;
    in.analytics = p.analytics;
    in.config = p.config;
    in.images = p.images;
    return resora::exportBundled(in, dest, progress);
}

resora::LinkedExportResult exportLinked(const State &s, const QString &dest, const QString &appVersion,
                                        resora::RelativeBase base, const QString &baseDir,
                                        const resora::ExportProgressFn &progress)
{
    const Plan p = plan(s);
    if (!p.ok) {
        resora::LinkedExportResult r;
        r.error = resora::ExportError::InvalidInput;
        r.message = p.error;
        return r;
    }
    resora::LinkedExportInput in;
    in.appVersion = appVersion;
    in.source = p.source;
    in.layout = p.layout;
    in.charts = p.charts;
    in.analytics = p.analytics;
    in.config = p.config;
    in.relativeBase = base;
    in.baseDir = baseDir;
    for (const resora::ExportImage &e : p.images) {
        resora::LinkedImage l;
        l.sourcePath = e.sourcePath;
        l.name = e.name;
        l.metadata = e.metadata;
        in.images << l;
    }
    return resora::exportLinked(in, dest, progress);
}

}  // namespace dashboard
