#include "backend/insightmetrics.h"

#include <QLocale>
#include <cmath>

#include "backend/comparison.h"
#include "backend/propertiesmodel.h"

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

SideFacts sideFacts(const QJsonObject &o, const QString &name)
{
    SideFacts f;
    f.name = name;
    const QJsonObject d = o.value(QLatin1String("dimensions")).toObject();
    if (has(d, "width") && has(d, "height")) {
        f.w = d.value(QLatin1String("width")).toInteger();
        f.h = d.value(QLatin1String("height")).toInteger();
        f.hasSize = f.w > 0 && f.h > 0;
        if (f.hasSize) {
            const double mp = o.value(QLatin1String("megapixels")).toDouble(-1);
            f.mp = has(o, "megapixels") && mp > 0 ? mp : double(f.w) * double(f.h) / 1e6;
        }
    }
    if (has(o, "file_size")) {
        f.bytes = o.value(QLatin1String("file_size")).toInteger(-1);
        f.hasBytes = f.bytes >= 0;
    }
    if (has(o, "aspect_ratio")) {
        f.aspect = o.value(QLatin1String("aspect_ratio")).toDouble();
        f.hasAspect = f.aspect > 0;
    } else if (f.hasSize) {
        f.aspect = double(f.w) / double(f.h);
        f.hasAspect = true;
    }
    if (f.hasAspect)
        f.aspectLabel = InsightMetricsBuilder::aspectText(o);
    return f;
}

const CompareRow *findRow(const Comparison &c, const QString &label)
{
    for (const CompareRow &r : c.rows)
        if (r.label == label)
            return &r;
    return nullptr;
}

MetaRow metaFrom(const Comparison &c, const QString &shown, const QString &label)
{
    MetaRow m;
    m.label = shown;
    const CompareRow *r = findRow(c, label);
    m.a = r ? r->left : PropertiesModel::unavailableText();
    m.b = r ? r->right : PropertiesModel::unavailableText();
    m.aAvailable = r && r->leftAvailable;
    m.bAvailable = r && r->rightAvailable;
    m.differs = r && r->differs;
    return m;
}

MetaRow exifRow(const QJsonObject &lo, const QJsonObject &ro)
{
    MetaRow m;
    m.label = QObject::tr("EXIF");
    m.aAvailable = lo.value(QLatin1String("exif")).toObject().value(QLatin1String("available")).toBool();
    m.bAvailable = ro.value(QLatin1String("exif")).toObject().value(QLatin1String("available")).toBool();
    m.a = m.aAvailable ? QObject::tr("Present") : PropertiesModel::unavailableText();
    m.b = m.bAvailable ? QObject::tr("Present") : PropertiesModel::unavailableText();
    m.differs = m.aAvailable != m.bAvailable;
    return m;
}

MetaRow orientationRow(const Comparison &c)
{
    MetaRow m;
    m.label = QObject::tr("Orientation");
    const CompareRow *layout = findRow(c, QObject::tr("Layout"));
    const CompareRow *tag = findRow(c, QObject::tr("EXIF orientation"));
    auto compose = [&](bool left, bool &avail) {
        QStringList parts;
        if (layout && (left ? layout->leftAvailable : layout->rightAvailable))
            parts << (left ? layout->left : layout->right);
        if (tag && (left ? tag->leftAvailable : tag->rightAvailable))
            parts << QObject::tr("EXIF: %1").arg(left ? tag->left : tag->right);
        avail = !parts.isEmpty();
        return avail ? parts.join(QStringLiteral("  ·  ")) : PropertiesModel::unavailableText();
    };
    m.a = compose(true, m.aAvailable);
    m.b = compose(false, m.bAvailable);
    m.differs = (m.aAvailable || m.bAvailable) && (m.aAvailable != m.bAvailable || m.a != m.b);
    return m;
}

void addVerdict(InsightMetrics &m)
{
    const QString A = QObject::tr("Image A"), B = QObject::tr("Image B");
    if (m.pixelsComparable) {
        if (m.morePixels == Side::Tie)
            m.verdict << QObject::tr("Both images have the same pixel count (%1 MP).").arg(n(m.a.mp, 2));
        else if (m.pixelRatio < 1.05)
            m.verdict << QObject::tr("The pixel counts are nearly equal (within 5%).");
        else
            m.verdict << QObject::tr("%1 has %2 the pixel count of %3.")
                             .arg(m.morePixels == Side::A ? A : B, InsightMetricsBuilder::multiplier(m.pixelRatio),
                                  m.morePixels == Side::A ? B : A);
    } else {
        m.verdict << QObject::tr("Pixel counts cannot be compared: dimensions are missing or invalid for at least one image.");
    }
    if (m.sizesComparable) {
        if (m.largerFile == Side::Tie)
            m.verdict << QObject::tr("Both files are the same size (%1).").arg(PropertiesModel::formatBytes(m.a.bytes));
        else {
            const bool aSmaller = m.largerFile == Side::B;
            m.verdict << QObject::tr("%1 occupies less storage (%2 vs %3).")
                             .arg(aSmaller ? A : B, PropertiesModel::formatBytes(aSmaller ? m.a.bytes : m.b.bytes).section(QStringLiteral("  ("), 0, 0),
                                  PropertiesModel::formatBytes(aSmaller ? m.b.bytes : m.a.bytes).section(QStringLiteral("  ("), 0, 0));
        }
    } else {
        m.verdict << QObject::tr("File sizes cannot be compared: a size is not available.");
    }
    if (m.aspectComparable) {
        if (m.aspectSame)
            m.verdict << QObject::tr("Both images have the same aspect ratio (%1).").arg(m.a.aspectLabel);
        else
            m.verdict << QObject::tr("The aspect ratios differ: %1 vs %2.").arg(m.a.aspectLabel, m.b.aspectLabel);
    }
    m.verdict << QObject::tr("Pixel count and file size do not by themselves say which image looks better.");
}
}  // namespace

namespace InsightMetricsBuilder {

QString aspectText(const QJsonObject &props)
{
    const QString label = props.value(QLatin1String("aspect_label")).toString();
    if (!label.isEmpty())
        return label;
    const double r = props.value(QLatin1String("aspect_ratio")).toDouble();
    return r > 0 ? n(r, 2) + QStringLiteral(":1") : QString();
}

QString multiplier(double ratio)
{
    return n(ratio, ratio >= 10 ? 1 : 2) + QStringLiteral("×");
}

QString sizeText(qint64 w, qint64 h)
{
    return QStringLiteral("%1 × %2").arg(n(double(w)), n(double(h)));
}

InsightMetrics build(const QJsonObject &lo, const QJsonObject &ro, const Comparison &c)
{
    InsightMetrics m;
    m.a = sideFacts(lo, c.left.name);
    m.b = sideFacts(ro, c.right.name);

    m.pixelsComparable = m.a.hasSize && m.b.hasSize;
    if (m.pixelsComparable) {
        const qint64 pa = m.a.pixels(), pb = m.b.pixels();
        m.morePixels = pa == pb ? Side::Tie : (pa > pb ? Side::A : Side::B);
        m.pixelRatio = double(qMax(pa, pb)) / double(qMin(pa, pb));
        m.mpDifference = std::abs(m.a.mp - m.b.mp);
        m.pixelPercent = (m.pixelRatio - 1.0) * 100.0;
        m.widthRatio = double(m.b.w) / double(m.a.w);
        m.heightRatio = double(m.b.h) / double(m.a.h);
    }

    m.sizesComparable = m.a.hasBytes && m.b.hasBytes;
    if (m.sizesComparable) {
        m.largerFile = m.a.bytes == m.b.bytes ? Side::Tie : (m.a.bytes > m.b.bytes ? Side::A : Side::B);
        m.byteDifference = std::abs(m.a.bytes - m.b.bytes);
        const qint64 small = qMin(m.a.bytes, m.b.bytes);
        m.bytePercentValid = small > 0;
        if (m.bytePercentValid)
            m.bytePercent = double(m.byteDifference) / double(small) * 100.0;
    }

    m.aspectComparable = m.a.hasAspect && m.b.hasAspect;
    if (m.aspectComparable) {
        m.aspectRelDiff = std::abs(m.a.aspect - m.b.aspect) / qMin(m.a.aspect, m.b.aspect);
        m.aspectSame = m.aspectRelDiff <= kAspectSameTolerance || (!m.a.aspectLabel.isEmpty() && m.a.aspectLabel == m.b.aspectLabel);
    }

    m.meta << metaFrom(c, QObject::tr("Format"), QObject::tr("Format")) << metaFrom(c, QObject::tr("Color mode"), QObject::tr("Color mode"))
           << metaFrom(c, QObject::tr("DPI / PPI"), QObject::tr("DPI / PPI")) << metaFrom(c, QObject::tr("Color profile"), QObject::tr("Color profile"))
           << exifRow(lo, ro) << orientationRow(c);

    addVerdict(m);
    return m;
}

}  // namespace InsightMetricsBuilder
