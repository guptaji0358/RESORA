#include "resora/importinternal.h"

#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <utility>

namespace resora::detail {

namespace {
constexpr quint64 kMaxJson = Q_UINT64_C(64) << 20;  // per JSON resource, bounds memory

bool isHex(const QString &s, int len)
{
    static const QRegularExpression re(QStringLiteral("^[0-9a-f]+$"));
    return s.size() == len && re.match(s).hasMatch();
}

bool readJson(Reader &rd, const QString &id, QJsonObject *out, QString *why)
{
    QByteArray b;
    const Result r = rd.readResource(id, &b, kMaxJson);
    if (!r.ok()) {
        *why = QStringLiteral("Resource '%1': %2").arg(id, r.message);
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(b, &pe);
    if (pe.error != QJsonParseError::NoError || !d.isObject()) {
        *why = QStringLiteral("Resource '%1' is not a valid JSON object").arg(id);
        return false;
    }
    *out = d.object();
    return true;
}

// Non-negative integer stored as a JSON number.
bool asSize(const QJsonValue &v, quint64 *out)
{
    if (!v.isDouble())
        return false;
    const double d = v.toDouble();
    if (d < 0 || d > 9007199254740992.0 || d != double(quint64(d)))
        return false;
    *out = quint64(d);
    return true;
}
}  // namespace

OpenResult failWith(OpenError e, const QString &msg)
{
    OpenResult r;
    r.error = e;
    r.message = msg;
    return r;
}

bool Reporter::operator()(ImportProgress::Stage st, quint64 done, quint64 total)
{
    static constexpr int base[] = {0, 50, 50, 1000};
    static constexpr int span[] = {50, 900, 900, 0};
    const int frac = total > 0 ? int(qMin<quint64>(span[st], quint64(span[st]) * done / total)) : 0;
    last_ = qMax(last_, base[st] + frac);
    if (!fn_)
        return true;
    ImportProgress p;
    p.stage = st;
    p.done = done;
    p.total = total;
    p.permille = st == ImportProgress::Done ? 1000 : qMin(last_, 999);
    return fn_(p);
}

bool parseDocument(Reader &rd, const QString &containerPath, Parsed *out, OpenResult *err)
{
    const auto bad = [&](const QString &m) {
        *err = failWith(OpenError::BadManifest, m);
        return false;
    };
    QString why;
    QJsonObject manifest, layout, analytics, config, charts, index;
    for (const char *id : {"manifest", "layout", "charts", "analytics", "config", "images-index"})
        if (!rd.contains(QString::fromLatin1(id)))
            return bad(QStringLiteral("Required resource '%1' is missing").arg(QString::fromLatin1(id)));
    if (!readJson(rd, "manifest", &manifest, &why) || !readJson(rd, "layout", &layout, &why)
        || !readJson(rd, "charts", &charts, &why) || !readJson(rd, "analytics", &analytics, &why)
        || !readJson(rd, "config", &config, &why) || !readJson(rd, "images-index", &index, &why))
        return bad(why);

    for (const auto &[doc, what] : {std::pair{&manifest, "manifest"}, std::pair{&index, "images-index"}, std::pair{&charts, "charts"}}) {
        const QString why2 = schemaVersionProblem(doc->value("schema_version"), 1, QString::fromLatin1(what));
        if (!why2.isEmpty())
            return bad(why2);
    }
    const QString mode = manifest.value("mode").toString();
    if (mode != QLatin1String("bundled") && mode != QLatin1String("linked"))
        return bad(QStringLiteral("Unknown document mode '%1'").arg(mode));
    const QString docId = manifest.value("document_id").toString();
    if (!isHex(docId, 32))
        return bad(QStringLiteral("Invalid document_id"));
    if (!index.value("images").isArray() || !charts.value("charts").isArray())
        return bad(QStringLiteral("images-index or charts has no array"));

    Parsed &p = *out;
    p.doc.containerPath = containerPath;
    p.doc.linked = mode == QLatin1String("linked");
    p.doc.documentId = docId;
    p.doc.manifest = manifest;
    p.doc.layout = layout;
    p.doc.analytics = analytics;
    p.doc.config = config;
    p.doc.charts = charts.value("charts").toArray();

    if (p.doc.linked) {
        const QJsonValue base = manifest.value("path_resolution").toObject().value("base");
        if (base.isObject()) {
            p.baseKind = base.toObject().value("kind").toString();
            if (p.baseKind == QLatin1String("folder"))
                p.baseHint = base.toObject().value("hint_path").toString();
            else if (p.baseKind != QLatin1String("document"))
                return bad(QStringLiteral("Unknown path_resolution base kind"));
        } else if (!base.isNull() && !base.isUndefined()) {
            return bad(QStringLiteral("Malformed path_resolution base"));
        }
    }

    const QJsonArray arr = index.value("images").toArray();
    if (manifest.value("image_count").toInt(-1) != arr.size())
        return bad(QStringLiteral("image_count (%1) does not match images-index (%2)")
                       .arg(manifest.value("image_count").toInt(-1)).arg(arr.size()));
    QSet<QString> ids, resources;
    for (int i = 0; i < arr.size(); ++i) {
        if (!arr[i].isObject())
            return bad(QStringLiteral("Image record %1 is not an object").arg(i + 1));
        const QJsonObject o = arr[i].toObject();
        Record rec;
        ImageRef &r = rec.ref;
        r.index = i;
        r.imageId = o.value("image_id").toString();
        r.name = o.value("name").toString();
        r.metadata = o.value("metadata").toObject();
        if (r.imageId.isEmpty() || r.imageId.size() > 64)
            return bad(QStringLiteral("Image record %1 has no valid image_id").arg(i + 1));
        if (ids.contains(r.imageId))
            return bad(QStringLiteral("Duplicate image_id '%1'").arg(r.imageId));
        ids.insert(r.imageId);
        const QJsonValue th = o.value("thumbnail");
        if (th.isString()) {
            r.thumbnailResource = th.toString();
            if (!isValidResourceId(r.thumbnailResource) || !rd.contains(r.thumbnailResource))
                return bad(QStringLiteral("Image '%1': thumbnail resource is missing or invalid").arg(r.imageId));
        } else if (!th.isNull() && !th.isUndefined()) {
            return bad(QStringLiteral("Image '%1': malformed thumbnail").arg(r.imageId));
        }
        if (p.doc.linked) {
            if (!o.value("linked").isObject() || o.contains("bundled"))
                return bad(QStringLiteral("Image '%1': expected a 'linked' record").arg(r.imageId));
            const QJsonObject l = o.value("linked").toObject();
            rec.path = l.value("path").toString();
            rec.relativePath = l.value("relative_path").toString();
            rec.sha256 = l.value("sha256").toString();
            if (rec.path.isEmpty() || !asSize(l.value("size"), &rec.size) || !isHex(rec.sha256, 64))
                return bad(QStringLiteral("Image '%1': invalid linked identity (path/size/sha256)").arg(r.imageId));
            r.size = rec.size;
            r.sha256 = rec.sha256;
        } else {
            if (!o.value("bundled").isObject() || o.contains("linked"))
                return bad(QStringLiteral("Image '%1': expected a 'bundled' record").arg(r.imageId));
            const QJsonObject b = o.value("bundled").toObject();
            rec.bundledResource = b.value("resource").toString();
            rec.sha256 = b.value("sha256").toString();
            if (!isValidResourceId(rec.bundledResource) || !asSize(b.value("size"), &rec.size) || !isHex(rec.sha256, 64))
                return bad(QStringLiteral("Image '%1': invalid bundled record").arg(r.imageId));
            if (resources.contains(rec.bundledResource))
                return bad(QStringLiteral("Image resource '%1' is referenced twice").arg(rec.bundledResource));
            resources.insert(rec.bundledResource);
            const ResourceInfo ri = rd.info(rec.bundledResource);
            if (ri.id.isEmpty() || ri.type != TypeImage)
                return bad(QStringLiteral("Image '%1': resource '%2' is missing or not an image").arg(r.imageId, rec.bundledResource));
            if (ri.compression != CompressionNone || ri.storedLength != rec.size
                || QString::fromLatin1(ri.sha256.toHex()) != rec.sha256)
                return bad(QStringLiteral("Image '%1': index disagrees with the container directory").arg(r.imageId));
            r.size = rec.size;
            r.sha256 = rec.sha256;
        }
        p.records.append(rec);
    }
    return true;
}

}  // namespace resora::detail
