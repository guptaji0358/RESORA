#include "comparison.h"

#include <QLocale>
#include <cmath>

namespace {
QString n(double v, int d = 0)
{
    return QLocale(QLocale::English).toString(v, 'f', d);
}

bool has(const QJsonObject &o, const char *k)
{
    const QJsonValue v = o.value(QLatin1String(k));
    return !v.isNull() && !v.isUndefined();
}

const PropertyRow *findRow(const ImageProperties &p, const QString &section, const QString &label)
{
    for (const PropertySection &s : p.sections)
        if (s.title == section)
            for (const PropertyRow &r : s.rows)
                if (r.label == label)
                    return &r;
    return nullptr;
}

void addRow(Comparison &c, const QString &section, const QString &label, const PropertyRow *l, const PropertyRow *r)
{
    if (!l && !r)
        return;
    CompareRow row;
    row.section = section;
    row.label = label;
    row.leftAvailable = l && l->available;
    row.rightAvailable = r && r->available;
    row.left = l ? l->value : PropertiesModel::unavailableText();
    row.right = r ? r->value : PropertiesModel::unavailableText();
    if (!row.leftAvailable)
        row.left = PropertiesModel::unavailableText();
    if (!row.rightAvailable)
        row.right = PropertiesModel::unavailableText();
    row.differs = (row.leftAvailable || row.rightAvailable) && (row.leftAvailable != row.rightAvailable || row.left != row.right);
    c.rows << row;
}

struct Facts
{
    qint64 w = 0, h = 0, bytes = 0;
    double mp = 0, aspect = 0;
    bool haveSize = false, haveBytes = false, haveAspect = false;
};

Facts facts(const QJsonObject &o)
{
    Facts f;
    const QJsonObject d = o.value(QLatin1String("dimensions")).toObject();
    f.haveSize = has(d, "width") && has(d, "height");
    if (f.haveSize) {
        f.w = d.value(QLatin1String("width")).toInteger();
        f.h = d.value(QLatin1String("height")).toInteger();
        f.mp = has(o, "megapixels") ? o.value(QLatin1String("megapixels")).toDouble() : double(f.w) * f.h / 1e6;
    }
    f.haveBytes = has(o, "file_size");
    f.bytes = o.value(QLatin1String("file_size")).toInteger();
    f.haveAspect = has(o, "aspect_ratio");
    f.aspect = o.value(QLatin1String("aspect_ratio")).toDouble();
    return f;
}

QString signedInt(qint64 v)
{
    return (v > 0 ? QStringLiteral("+") : QString()) + QLocale(QLocale::English).toString(v);
}

void addInsights(Comparison &c, const QJsonObject &lo, const QJsonObject &ro)
{
    const Facts a = facts(lo), b = facts(ro);
    const QString ln = c.left.name, rn = c.right.name;
    if (a.haveSize && b.haveSize) {
        const qint64 pa = a.w * a.h, pb = b.w * b.h;
        if (pa == pb)
            c.insights << QObject::tr("Both images have the same pixel count (%1 px).").arg(n(double(pa)));
        else {
            const bool leftBigger = pa > pb;
            const double big = double(qMax(pa, pb)), small = double(qMin(pa, pb));
            c.insights << QObject::tr("%1 has the greater pixel count: %2 vs %3 px (%4× the pixels, %5 MP difference).")
                              .arg(leftBigger ? ln : rn, n(big), n(small), n(big / small, 2), n(std::abs(a.mp - b.mp), 2));
        }
        c.insights << QObject::tr("Width difference: %1 px. Height difference: %2 px (right minus left).")
                          .arg(signedInt(b.w - a.w), signedInt(b.h - a.h));
    }
    if (a.haveBytes && b.haveBytes) {
        if (a.bytes == b.bytes)
            c.insights << QObject::tr("Both files are the same size (%1).").arg(PropertiesModel::formatBytes(a.bytes));
        else {
            const bool leftBigger = a.bytes > b.bytes;
            c.insights << QObject::tr("%1 has the larger file: %2 bigger%3.")
                              .arg(leftBigger ? ln : rn, PropertiesModel::formatBytes(std::abs(a.bytes - b.bytes)),
                                   [&] {
                                       const QString p = ComparisonModel::percentChange(double(qMin(a.bytes, b.bytes)),
                                                                                         double(qMax(a.bytes, b.bytes)));
                                       return p.isEmpty() ? QString() : QObject::tr(" (%1)").arg(p);
                                   }());
        }
    }
    if (a.haveAspect && b.haveAspect) {
        const double diff = std::abs(a.aspect - b.aspect);
        if (diff < 0.005)
            c.insights << QObject::tr("The aspect ratios match (%1).").arg(c.left.sections.isEmpty() ? QString() : lo.value(QLatin1String("aspect_label")).toString());
        else
            c.insights << QObject::tr("Aspect ratios differ: %1 vs %2 (difference %3). Images are shown at their own proportions, never stretched.")
                              .arg(lo.value(QLatin1String("aspect_label")).toString(), ro.value(QLatin1String("aspect_label")).toString(), n(diff, 3));
    }
    c.insights << QObject::tr("Higher resolution is not the same as better quality: pixel count says nothing about sharpness, "
                              "noise, compression or colour. Judge quality by inspecting the images.");
}
}  // namespace

int Comparison::differenceCount() const
{
    int count = 0;
    for (const CompareRow &r : rows)
        count += r.differs ? 1 : 0;
    return count;
}

namespace ComparisonModel {

QString percentChange(double a, double b)
{
    if (a <= 0)
        return QString();
    const double p = (b - a) / a * 100.0;
    return (p >= 0 ? QStringLiteral("+") : QString()) + n(p, 1) + QLatin1Char('%');
}

Comparison compare(const QJsonObject &lo, const QJsonObject &ro)
{
    Comparison c;
    c.left = PropertiesModel::fromJson(lo);
    c.right = PropertiesModel::fromJson(ro);
    c.complete = c.left.ok() && c.right.ok();
    if (!c.complete)
        return c;

    struct Spec { const char *section; const char *label; };
    // Titles/labels must match PropertiesModel (shared source of truth).
    const QList<QPair<QString, QString>> specs = {
        {QObject::tr("DIMENSIONS"), QObject::tr("Displayed size")}, {QObject::tr("DIMENSIONS"), QObject::tr("Megapixels")},
        {QObject::tr("DIMENSIONS"), QObject::tr("Aspect ratio")},   {QObject::tr("DIMENSIONS"), QObject::tr("Resolution class")},
        {QObject::tr("FILE"), QObject::tr("File size")},            {QObject::tr("FILE"), QObject::tr("Format")},
        {QObject::tr("COLOR"), QObject::tr("Color mode")},          {QObject::tr("COLOR"), QObject::tr("Color profile")},
        {QObject::tr("DENSITY"), QObject::tr("DPI / PPI")},         {QObject::tr("ORIENTATION"), QObject::tr("Layout")},
        {QObject::tr("ORIENTATION"), QObject::tr("EXIF orientation")}};
    for (const auto &s : specs)
        addRow(c, s.first, s.second, findRow(c.left, s.first, s.second), findRow(c.right, s.first, s.second));

    // EXIF: union of the fields either image carries, in first-seen order.
    const QString exif = QObject::tr("EXIF");
    QStringList labels;
    for (const ImageProperties *p : {&c.left, &c.right})
        for (const PropertySection &s : p->sections)
            if (s.title == exif)
                for (const PropertyRow &r : s.rows)
                    if (r.available && !labels.contains(r.label))
                        labels << r.label;
    if (labels.isEmpty())
        addRow(c, exif, QObject::tr("EXIF metadata"), findRow(c.left, exif, QObject::tr("EXIF metadata")),
               findRow(c.right, exif, QObject::tr("EXIF metadata")));
    for (const QString &label : labels)
        addRow(c, exif, label, findRow(c.left, exif, label), findRow(c.right, exif, label));

    addInsights(c, lo, ro);
    return c;
}

}  // namespace ComparisonModel
