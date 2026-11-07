#include "dashboardmodel.h"

#include <QObject>

#include "ui/uiformat.h"

namespace {
QString pct(double p) { return AnalyticsModel::percentText(p); }
QString images(qint64 n) { return n == 1 ? QObject::tr("1 image") : QObject::tr("%1 images").arg(UiFormat::count(n)); }

ChartItem countItem(const AnalyticsShare &s, const QString &name)
{
    ChartItem c;
    c.label = name;
    c.count = s.count;
    c.percent = s.percent;
    c.value = double(s.count);
    c.valueText = UiFormat::count(s.count);
    c.tooltip = QObject::tr("%1: %2 (%3 of analyzed images)").arg(name, images(s.count), pct(s.percent));
    c.detail = QObject::tr("%1 — %2, %3 of the analyzed images.").arg(name, images(s.count), pct(s.percent));
    return c;
}

QString cap(const QString &s) { return s.isEmpty() ? s : s.left(1).toUpper() + s.mid(1); }
}  // namespace

namespace DashboardModel {

QList<ChartItem> categoryItems(const AnalyticsData &d)
{
    QList<ChartItem> out;
    if (d.analyzed <= 0)
        return out;
    for (const AnalyticsShare &s : d.categories)
        out << countItem(s, s.name);
    return out;
}

QList<ChartItem> orientationItems(const AnalyticsData &d)
{
    QList<ChartItem> out;
    if (d.analyzed <= 0)
        return out;
    for (const AnalyticsShare &s : d.orientation)
        out << countItem(s, cap(s.name));
    return out;
}

QList<ChartItem> formatItems(const AnalyticsData &d)
{
    QList<ChartItem> out;
    if (d.analyzed <= 0)
        return out;
    for (const AnalyticsShare &s : d.formats) {
        ChartItem c = countItem(s, s.name);
        if (d.sizeKnown > 0)
            c.detail += QObject::tr(" Together %1, %2 of storage.").arg(AnalyticsModel::bytesText(double(s.bytes)), pct(s.bytesPercent));
        out << c;
    }
    return out;
}

QList<ChartItem> storageItems(const AnalyticsData &d)
{
    QList<ChartItem> out;
    if (d.analyzed <= 0 || d.sizeKnown <= 0)
        return out;
    for (const AnalyticsShare &s : d.formats) {
        ChartItem c;
        c.label = s.name;
        c.count = s.count;
        c.percent = s.bytesPercent;
        c.value = double(s.bytes);
        c.secondary = s.percent;
        c.valueText = QStringLiteral("%1  ·  %2").arg(AnalyticsModel::bytesText(double(s.bytes)), pct(s.bytesPercent));
        c.tooltip = QObject::tr("%1: %2, %3 of storage; %4 (%5 of images)")
                        .arg(s.name, AnalyticsModel::bytesText(double(s.bytes)), pct(s.bytesPercent), images(s.count), pct(s.percent));
        c.detail = QObject::tr("%1 — %2 on disk (%3 of storage) across %4 (%5 of images).")
                       .arg(s.name, AnalyticsModel::bytesText(double(s.bytes)), pct(s.bytesPercent), images(s.count), pct(s.percent));
        out << c;
    }
    return out;
}

QStringList insights(const AnalyticsData &d)
{
    QStringList out;
    if (d.analyzed <= 0) {
        out << QObject::tr("No image could be analyzed, so there is nothing to summarize yet.");
        if (d.failed > 0)
            out << QObject::tr("%1 failed to analyze.").arg(images(d.failed));
        return out;
    }
    auto top = [](const QList<AnalyticsShare> &l) {
        const AnalyticsShare *best = nullptr;
        for (const AnalyticsShare &s : l)
            if (s.count > 0 && (!best || s.count > best->count))
                best = &s;
        return best;
    };
    if (const AnalyticsShare *c = top(d.categories))
        out << QObject::tr("Most common resolution class: %1 (%2 of analyzed images).").arg(c->name, pct(c->percent));
    if (const AnalyticsShare *o = top(d.orientation))
        out << QObject::tr("%1 images lead the collection (%2).").arg(cap(o->name), pct(o->percent));
    if (d.commonCount > 0)
        out << QObject::tr("The most frequent exact size is %1 (%2).").arg(AnalyticsModel::sizeText(d.commonWidth, d.commonHeight), images(d.commonCount));
    if (d.averageMegapixels >= 0)
        out << QObject::tr("The average image is %1 (%2 × %3 px).")
                   .arg(AnalyticsModel::megapixelsText(d.averageMegapixels))
                   .arg(d.averageWidth, 0, 'f', 0)
                   .arg(d.averageHeight, 0, 'f', 0);
    if (d.sizeKnown > 0) {
        const AnalyticsShare *heavy = nullptr;
        for (const AnalyticsShare &s : d.formats)
            if (s.bytesPercent >= 0 && (!heavy || s.bytesPercent > heavy->bytesPercent))
                heavy = &s;
        if (heavy)
            out << QObject::tr("%1 files take %2 of storage while making up %3 of images.").arg(heavy->name, pct(heavy->bytesPercent), pct(heavy->percent));
    }
    if (d.sizeUnknown > 0)
        out << QObject::tr("%1 have no known file size and are left out of every storage figure.").arg(images(d.sizeUnknown));
    if (d.failed > 0)
        out << QObject::tr("%1 of %2 failed to analyze and are excluded from every chart.").arg(UiFormat::count(d.failed), images(d.total));
    if (d.duplicatesIgnored > 0)
        out << QObject::tr("%1 repeated path(s) were counted once.").arg(UiFormat::count(d.duplicatesIgnored));
    return out;
}

double totalValue(const QList<ChartItem> &items)
{
    double t = 0;
    for (const ChartItem &i : items)
        t += i.value;
    return t;
}

qint64 niceAxisMax(qint64 max)
{
    if (max <= 1)
        return 1;
    qint64 base = 1;
    while (base * 10 < max)
        base *= 10;
    for (qint64 m : {1, 2, 5, 10}) {
        if (base * m >= max)
            return base * m;
    }
    return base * 10;
}

}  // namespace DashboardModel
