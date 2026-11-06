#include "duplicatesmodel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>

namespace {
DupFile fileFrom(const QJsonObject &o)
{
    DupFile f;
    f.path = o.value("path").toString();
    f.name = o.value("name").toString();
    f.folder = o.value("folder").toString();
    f.format = o.value("format").toString();
    f.resolution = o.value("resolution").toString();
    f.width = o.value("width").toInt();
    f.height = o.value("height").toInt();
    f.size = o.contains("file_size") ? qint64(o.value("file_size").toDouble()) : -1;
    if (o.contains("similarity")) {
        f.similarity = o.value("similarity").toDouble();
        f.category = o.value("category").toString();
        f.anchor = o.value("anchor").toBool();
        f.identicalCopies = o.value("identical_copies").toInt();
    }
    return f;
}

DupGroup groupFrom(const QJsonObject &o, bool exact)
{
    DupGroup g;
    g.exact = exact;
    g.id = o.value("id").toString();
    g.category = o.value("category").toString();
    g.lowest = o.contains("lowest_similarity") ? o.value("lowest_similarity").toDouble() : -1;
    g.totalBytes = qint64(o.value("total_bytes").toDouble());
    g.savingsBytes = qint64(o.value("savings_bytes").toDouble());
    for (const QJsonValue &v : o.value("files").toArray())
        g.files.append(fileFrom(v.toObject()));
    return g;
}
}  // namespace

QByteArray DupRequest::paramsJson() const
{
    QJsonObject o{{"recursive", recursive}, {"similar", similar}, {"threshold", threshold},
                  {"exclude", QJsonArray::fromStringList(exclude)}};
    if (!folders.isEmpty())
        o["folders"] = QJsonArray::fromStringList(folders);
    if (!files.isEmpty())
        o["files"] = QJsonArray::fromStringList(files);
    if (!cachePath.isEmpty())
        o["cache_path"] = cachePath;
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

DupResult DuplicatesModel::fromJson(const QJsonObject &d)
{
    DupResult r;
    if (!d.contains("stats"))
        return r;
    r.valid = true;
    const QJsonObject s = d.value("stats").toObject();
    auto n = [&](const char *k) { return qint64(s.value(k).toDouble()); };
    r.stats = {n("total_files"), n("empty_files"), n("exact_groups"), n("exact_files"), n("exact_extra_files"),
               n("exact_bytes"), n("exact_savings_bytes"), n("similar_groups"), n("similar_files"),
               n("similar_bytes"), n("skipped"), n("cache_hits"), n("flat_images")};
    for (const QJsonValue &v : d.value("exact_groups").toArray())
        r.exact.append(groupFrom(v.toObject(), true));
    for (const QJsonValue &v : d.value("similar_groups").toArray())
        r.similar.append(groupFrom(v.toObject(), false));
    for (const QJsonValue &v : d.value("skipped").toArray()) {
        const QJsonObject o = v.toObject();
        r.skipped << QStringLiteral("%1 — %2: %3").arg(o.value("path").toString(), o.value("status").toString(),
                                                      o.value("error").toString());
    }
    r.skippedTruncated = d.value("skipped_truncated").toBool();
    r.cacheWarning = d.value("cache_warning").toString();
    const QJsonObject st = d.value("settings").toObject();
    r.threshold = st.value("threshold").toDouble(90);
    r.similarEnabled = st.value("similar").toBool(true);
    return r;
}

QString DuplicatesModel::bytesText(qint64 bytes)
{
    if (bytes < 0)
        return QStringLiteral("Not available");
    return QLocale::c().formattedDataSize(bytes, 2, QLocale::DataSizeTraditionalFormat);
}

QString DuplicatesModel::similarityText(double p)
{
    return p < 0 ? QStringLiteral("—") : QStringLiteral("%1%").arg(p, 0, 'f', 1);
}

QString DuplicatesModel::groupTitle(const DupGroup &g)
{
    return g.exact ? QObject::tr("%1 identical files").arg(g.files.size())
                   : QObject::tr("%1 visually similar images").arg(g.files.size());
}
