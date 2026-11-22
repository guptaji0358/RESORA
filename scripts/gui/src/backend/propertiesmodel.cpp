#include "backend/propertiesmodel.h"

#include <QLocale>

namespace {
PropertyRow row(const QString &label, const QString &value)
{
    return {label, value, true};
}

// A row whose value may be absent: `text` empty => "Not available" (muted).
PropertyRow maybe(const QString &label, const QString &text)
{
    if (text.isEmpty())
        return {label, PropertiesModel::unavailableText(), false};
    return {label, text, true};
}

QString str(const QJsonObject &o, const char *key)
{
    return o.value(QLatin1String(key)).toString();
}

QString num(double v, int decimals = 0)
{
    return QLocale(QLocale::English).toString(v, 'f', decimals);
}

bool has(const QJsonObject &o, const char *key)
{
    return !o.value(QLatin1String(key)).isNull() && !o.value(QLatin1String(key)).isUndefined();
}

PropertySection fileSection(const QJsonObject &o)
{
    PropertySection s{QObject::tr("FILE"), {}};
    s.rows << row(QObject::tr("Name"), str(o, "name"));
    s.rows << row(QObject::tr("Location"), str(o, "path"));
    s.rows << maybe(QObject::tr("Format"), str(o, "format"));
    s.rows << maybe(QObject::tr("File size"),
                    has(o, "file_size") ? PropertiesModel::formatBytes(o.value(QLatin1String("file_size")).toInteger()) : QString());
    return s;
}

PropertySection dimensionsSection(const QJsonObject &o, const QJsonObject &d)
{
    PropertySection s{QObject::tr("DIMENSIONS"), {}};
    const bool haveDims = has(d, "width") && has(d, "height");
    s.rows << maybe(QObject::tr("Displayed size"),
                    haveDims ? PropertiesModel::formatDimensions(d.value(QLatin1String("width")).toInt(),
                                                                 d.value(QLatin1String("height")).toInt())
                             : QString());
    if (haveDims && d.value(QLatin1String("exif_rotated")).toBool())
        s.rows << row(QObject::tr("Stored size"),
                      PropertiesModel::formatDimensions(d.value(QLatin1String("stored_width")).toInt(),
                                                        d.value(QLatin1String("stored_height")).toInt()));
    s.rows << maybe(QObject::tr("Megapixels"),
                    has(o, "megapixels") ? num(o.value(QLatin1String("megapixels")).toDouble(), 2) + QStringLiteral(" MP") : QString());
    s.rows << maybe(QObject::tr("Aspect ratio"),
                    has(o, "aspect_ratio")
                        ? QStringLiteral("%1  (%2)").arg(str(o, "aspect_label"), num(o.value(QLatin1String("aspect_ratio")).toDouble(), 3))
                        : QString());
    s.rows << maybe(QObject::tr("Resolution class"), str(o, "resolution"));
    const int frames = o.value(QLatin1String("frames")).toInt();
    if (frames > 1)
        s.rows << row(QObject::tr("Frames"), QObject::tr("%1 (size shown is the first frame)").arg(frames));
    return s;
}

PropertySection orientationSection(const QJsonObject &o, const QJsonObject &d)
{
    PropertySection s{QObject::tr("ORIENTATION"), {}};
    QString layout = str(o, "layout");
    if (!layout.isEmpty())
        layout[0] = layout[0].toUpper();
    s.rows << maybe(QObject::tr("Layout"), layout);
    if (has(d, "orientation_tag")) {
        s.rows << row(QObject::tr("EXIF orientation"),
                      QStringLiteral("%1  (tag %2)").arg(str(d, "orientation_label")).arg(d.value(QLatin1String("orientation_tag")).toInt()));
        s.rows << row(QObject::tr("Applied to displayed size"),
                      d.value(QLatin1String("exif_rotated")).toBool() ? QObject::tr("Yes: width and height are swapped")
                                                                      : QObject::tr("No change to width/height"));
    } else {
        s.rows << maybe(QObject::tr("EXIF orientation"), QString());
    }
    return s;
}

PropertySection colorSection(const QJsonObject &c)
{
    PropertySection s{QObject::tr("COLOR"), {}};
    if (c.isEmpty()) {
        s.rows << maybe(QObject::tr("Color mode"), QString());
        return s;
    }
    s.rows << row(QObject::tr("Color mode"), QStringLiteral("%1  (%2)").arg(str(c, "mode_name"), str(c, "mode")));
    s.rows << maybe(QObject::tr("Bit depth"),
                    has(c, "bits_per_channel")
                        ? QObject::tr("%1 bits per channel, %2 channels").arg(c.value(QLatin1String("bits_per_channel")).toInt())
                                                                          .arg(c.value(QLatin1String("channels")).toInt())
                        : QString());
    s.rows << row(QObject::tr("Transparency"), c.value(QLatin1String("has_alpha")).toBool() ? QObject::tr("Yes") : QObject::tr("No"));
    const QJsonValue icc = c.value(QLatin1String("icc_profile"));
    if (icc.isObject()) {
        const QJsonObject i = icc.toObject();
        const QString desc = str(i, "description");
        s.rows << row(QObject::tr("Color profile"),
                      desc.isEmpty() ? QObject::tr("Embedded (name unreadable), %1 bytes").arg(i.value(QLatin1String("size")).toInt()) : desc);
    } else {
        s.rows << maybe(QObject::tr("Color profile"), QString());
    }
    return s;
}

PropertySection densitySection(const QJsonValue &dpiValue)
{
    PropertySection s{QObject::tr("DENSITY"), {}};
    if (!dpiValue.isObject()) {
        s.rows << maybe(QObject::tr("DPI / PPI"), QString());
        return s;
    }
    const QJsonObject d = dpiValue.toObject();
    const double x = d.value(QLatin1String("x")).toDouble(), y = d.value(QLatin1String("y")).toDouble();
    const bool whole = x == int(x) && y == int(y);
    s.rows << row(QObject::tr("DPI / PPI"),
                  x == y ? num(x, whole ? 0 : 2) : QStringLiteral("%1 × %2").arg(num(x, whole ? 0 : 2), num(y, whole ? 0 : 2)));
    s.rows << row(QObject::tr("Source"), str(d, "source"));
    return s;
}

PropertySection exifSection(const QJsonValue &exifValue)
{
    PropertySection s{QObject::tr("EXIF"), {}};
    if (!exifValue.isObject() || !exifValue.toObject().value(QLatin1String("available")).toBool()) {
        s.rows << maybe(QObject::tr("EXIF metadata"), QString());
        return s;
    }
    const QJsonObject e = exifValue.toObject();
    for (const QJsonValue &f : e.value(QLatin1String("fields")).toArray()) {
        const QJsonObject fo = f.toObject();
        s.rows << row(str(fo, "label"), str(fo, "value"));
    }
    if (e.value(QLatin1String("gps_present")).toBool())
        s.rows << row(QObject::tr("GPS location"), QObject::tr("Present in file (coordinates are not shown)"));
    return s;
}
}  // namespace

namespace PropertiesModel {

QString unavailableText()
{
    return QObject::tr("Not available");
}

QString formatBytes(qint64 bytes)
{
    const QString exact = QLocale(QLocale::English).toString(bytes);
    if (bytes < 1024)
        return QObject::tr("%1 bytes").arg(exact);
    const char *units[] = {"KB", "MB", "GB", "TB"};
    double v = bytes;
    int u = -1;
    while (v >= 1024 && u < 3) {
        v /= 1024;
        ++u;
    }
    return QStringLiteral("%1 %2  (%3 bytes)").arg(num(v, 2), QLatin1String(units[u]), exact);
}

QString formatDimensions(int width, int height)
{
    return QStringLiteral("%1 × %2 px").arg(QLocale(QLocale::English).toString(width), QLocale(QLocale::English).toString(height));
}

ImageProperties fromJson(const QJsonObject &o)
{
    ImageProperties p;
    p.path = str(o, "path");
    p.name = str(o, "name");
    p.status = str(o, "status");
    p.error = str(o, "error");
    for (const QJsonValue &w : o.value(QLatin1String("warnings")).toArray())
        p.warnings << w.toString();
    if (!p.ok()) {
        if (p.status.isEmpty())
            p.status = QStringLiteral("unreadable");
        return p;
    }
    const QJsonObject dims = o.value(QLatin1String("dimensions")).toObject();
    p.sections << fileSection(o) << dimensionsSection(o, dims) << orientationSection(o, dims)
               << colorSection(o.value(QLatin1String("color")).toObject())
               << densitySection(o.value(QLatin1String("dpi"))) << exifSection(o.value(QLatin1String("exif")));
    return p;
}

QList<ImageProperties> fromResult(const QJsonObject &data)
{
    QList<ImageProperties> out;
    for (const QJsonValue &v : data.value(QLatin1String("images")).toArray())
        out << fromJson(v.toObject());
    return out;
}

}  // namespace PropertiesModel

QString ImageProperties::toPlainText() const
{
    QString t = name + QLatin1Char('\n');
    if (!ok())
        return t + QStringLiteral("%1: %2\n").arg(status, error);
    for (const PropertySection &s : sections) {
        t += QLatin1Char('\n') + s.title + QLatin1Char('\n');
        for (const PropertyRow &r : s.rows)
            t += QStringLiteral("  %1: %2\n").arg(r.label, r.value);
    }
    return t;
}
