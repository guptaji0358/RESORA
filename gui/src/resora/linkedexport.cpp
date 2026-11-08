#include "resora/linkedexport.h"

#include "resora/destinationfinalizer.h"
#include "resora/exportcommon.h"
#include "resora/resorareader.h"
#include "resora/resorawriter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QUuid>

namespace resora {

namespace {
using detail::kPathCase;
using detail::Source;

LinkedExportResult fail(ExportError e, const QString &m)
{
    LinkedExportResult r;
    r.error = e;
    r.message = m;
    return r;
}

QString cleanAbs(const QString &p) { return QDir::cleanPath(QFileInfo(p).absoluteFilePath()); }

// "dir/sub/a.png" relative to `base`, or null when the file is not inside `base` (no ".." ever recorded).
QString relativeInside(const QString &base, const QString &abs)
{
    if (base.isEmpty())
        return QString();
    const QString rel = QDir(base).relativeFilePath(abs);
    if (rel.isEmpty() || QDir::isAbsolutePath(rel) || rel == QLatin1String("..") || rel.startsWith(QLatin1String("../")))
        return QString();
    return rel;
}

QJsonValue nz(const QJsonValue &v) { return v.isUndefined() ? QJsonValue(QJsonValue::Null) : v; }

// Stage windows of the overall bar: validate 0-850, write 850-920, verify 920-950, finalize 950, done 1000.
constexpr int kBase[] = {0, 850, 920, 950, 1000};
constexpr int kSpan[] = {850, 70, 30, 0, 0};
}  // namespace

LinkedExportResult exportLinked(const LinkedExportInput &in, const QString &targetPath, const ExportProgressFn &progress)
{
    if (targetPath.isEmpty())
        return fail(ExportError::InvalidInput, QStringLiteral("No destination path"));
    if (in.images.size() > 60000)
        return fail(ExportError::InvalidInput, QStringLiteral("Too many images for one container"));
    const QFileInfo targetInfo(targetPath);
    if (!targetInfo.absoluteDir().exists())
        return fail(ExportError::InvalidInput, QStringLiteral("Destination folder does not exist"));
    if (targetInfo.isDir())
        return fail(ExportError::InvalidInput, QStringLiteral("Destination is a folder"));
    const QString targetDir = targetInfo.absoluteDir().canonicalPath();
    const QString targetCanon = QFileInfo(targetDir + QLatin1Char('/') + targetInfo.fileName()).absoluteFilePath();

    // Relative-path base (explicit; never guessed).
    QString base;
    QJsonValue baseJson = QJsonValue::Null;
    if (in.relativeBase == RelativeBase::Folder) {
        const QFileInfo bi(in.baseDir);
        if (in.baseDir.isEmpty() || !QDir::isAbsolutePath(in.baseDir) || !bi.isDir())
            return fail(ExportError::InvalidInput, QStringLiteral("baseDir must be an existing absolute folder"));
        base = cleanAbs(in.baseDir);
        baseJson = QJsonObject{{"kind", "folder"}, {"hint_path", base}};
    } else if (in.relativeBase == RelativeBase::Document) {
        base = QDir::cleanPath(targetDir);
        baseJson = QJsonObject{{"kind", "document"}};
    }

    int lastPermille = 0;
    const auto report = [&](ExportProgress::Stage st, quint64 d, quint64 t, int idx) {
        const int frac = t > 0 ? int(qMin<quint64>(kSpan[st], quint64(kSpan[st]) * d / t)) : 0;
        lastPermille = qMax(lastPermille, kBase[st] + frac);
        return !progress || progress({st, d, t, idx, lastPermille});
    };

    // ---- 1. validate every reference: exists, regular, readable; hash; compare expected identity ----
    LinkedExportResult res;
    QList<Source> srcs(in.images.size());
    QStringList abs(in.images.size());
    quint64 total = 0;
    for (int i = 0; i < in.images.size(); ++i) {
        const QString &p = in.images[i].sourcePath;
        if (p.isEmpty() || !QDir::isAbsolutePath(p))
            return fail(ExportError::InvalidInput, QStringLiteral("Image %1 needs an absolute source path").arg(i + 1));
        abs[i] = cleanAbs(p);
        const QFileInfo fi(p);
        if (fi.exists() && fi.isFile())
            total += quint64(qMax<qint64>(0, fi.size()));
    }
    quint64 done = 0;
    for (int i = 0; i < in.images.size(); ++i) {
        const LinkedImage &im = in.images[i];
        const QFileInfo fi(im.sourcePath);
        if (!fi.exists()) {
            res.issues.append({i, im.sourcePath, SourceIssue::Missing, QStringLiteral("File not found")});
            continue;
        }
        if (QString::compare(fi.canonicalFilePath(), targetCanon, kPathCase) == 0)
            return fail(ExportError::InvalidInput, QStringLiteral("The destination is one of the source images: ") + im.sourcePath);
        if (!fi.isFile()) {
            res.issues.append({i, im.sourcePath, SourceIssue::Unreadable, QStringLiteral("Not a regular file")});
            continue;
        }
        srcs[i].mtimeMs = fi.lastModified().toMSecsSinceEpoch();
        QString why;
        bool cancelled = false;
        const bool ok = detail::hashFile(im.sourcePath, &srcs[i], &why, [&](quint64 n) {
            done += n;
            if (!report(ExportProgress::Validating, done, total, i)) {
                cancelled = true;
                return false;
            }
            return true;
        });
        if (cancelled)
            return fail(ExportError::Cancelled, QStringLiteral("Export cancelled"));
        if (!ok) {
            res.issues.append({i, im.sourcePath, SourceIssue::Unreadable, QStringLiteral("Cannot read file: ") + why});
            continue;
        }
        const QString hex = QString::fromLatin1(srcs[i].sha.toHex());
        if (im.expectedSize >= 0 && quint64(im.expectedSize) != srcs[i].size)
            res.issues.append({i, im.sourcePath, SourceIssue::Changed,
                               QStringLiteral("Size differs from the analysed file (%1 -> %2 bytes)").arg(im.expectedSize).arg(srcs[i].size)});
        else if (!im.expectedSha256.isEmpty() && im.expectedSha256.toLower() != hex)
            res.issues.append({i, im.sourcePath, SourceIssue::Changed, QStringLiteral("Content differs from the analysed file")});
    }
    if (!res.issues.isEmpty()) {
        res.error = ExportError::SourceProblem;
        res.message = QStringLiteral("%1 referenced image(s) cannot be exported").arg(res.issues.size());
        return res;
    }

    // ---- 2. duplicate file names (safe: identity never depends on the name) ----
    QHash<QString, QList<int>> byName;
    QStringList nameOrder;
    QList<QString> names(in.images.size());
    for (int i = 0; i < in.images.size(); ++i) {
        names[i] = in.images[i].name.isEmpty() ? QFileInfo(in.images[i].sourcePath).fileName() : in.images[i].name;
        const QString key = kPathCase == Qt::CaseInsensitive ? names[i].toLower() : names[i];
        if (!byName.contains(key))
            nameOrder.append(key);
        byName[key].append(i);
    }
    QList<bool> collides(in.images.size(), false);
    for (const QString &key : nameOrder) {
        const QList<int> &idx = byName[key];
        if (idx.size() < 2)
            continue;
        bool samePath = true;
        for (int k : idx) {
            collides[k] = true;
            if (QString::compare(abs[k], abs[idx.first()], kPathCase) != 0)
                samePath = false;
        }
        res.duplicateNames.append({names[idx.first()], idx, samePath});
    }

    // ---- 3. build the container description (no image resources) ----
    res.documentId = QUuid::createUuid().toString(QUuid::Id128);
    res.imageCount = in.images.size();
    Writer w;
    QJsonArray records, inventory;
    bool built = true;
    for (int i = 0; i < in.images.size(); ++i) {
        const LinkedImage &im = in.images[i];
        QJsonObject rec{{"image_id", imageId(i)}, {"name", names[i]}, {"metadata", im.metadata}, {"thumbnail", QJsonValue::Null}};
        if (!im.thumbnail.isEmpty()) {
            built = built && w.addBytes(thumbResourceId(i), TypeThumbnail, im.thumbnail).ok();
            inventory.append(QJsonObject{{"id", thumbResourceId(i)}, {"type", "thumbnail"}});
            rec["thumbnail"] = thumbResourceId(i);
            if (!im.thumbnailFormat.isEmpty())
                rec["thumbnail_format"] = im.thumbnailFormat;
        }
        const QString rel = relativeInside(base, abs[i]);
        if (!rel.isEmpty())
            ++res.relativeCount;
        const QFileInfo fi(im.sourcePath);
        rec["linked"] = QJsonObject{
            {"path", abs[i]},
            {"relative_path", rel.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(rel)},
            {"file_name", fi.fileName()},
            {"size", double(srcs[i].size)},
            {"modified_utc", fi.lastModified().toUTC().toString(Qt::ISODateWithMs)},
            {"sha256", QString::fromLatin1(srcs[i].sha.toHex())},
            {"width", nz(im.metadata.value(QStringLiteral("width")))},
            {"height", nz(im.metadata.value(QStringLiteral("height")))},
            {"format", nz(im.metadata.value(QStringLiteral("format")))},
            {"name_collision", collides[i]}};
        records.append(rec);
        res.imageBytes += srcs[i].size;
    }
    built = built && detail::addCommonResources(w, inventory, in.layout, in.charts, in.analytics, in.engineProtocol, in.config);
    built = built && detail::addJsonResource(w, inventory, QStringLiteral("images-index"),
                                             QJsonObject{{"schema_version", 1}, {"images", records}});
    inventory.append(QJsonObject{{"id", "manifest"}, {"type", "json"}});
    const QJsonObject manifest{
        {"schema_version", 1},
        {"format_version", QStringLiteral("%1.%2").arg(kVersionMajor).arg(kVersionMinor)},
        {"app_version", in.appVersion},
        {"mode", "linked"},
        {"created_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"document_id", res.documentId},
        {"image_count", in.images.size()},
        {"source", in.source.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(in.source)},
        {"path_resolution", QJsonObject{{"policy", "absolute-then-relative"}, {"relative_separator", "/"}, {"base", baseJson}}},
        {"resources", inventory}};
    built = built && w.addBytes(QStringLiteral("manifest"), TypeJson, detail::toJson(manifest)).ok();
    if (!built)
        return fail(ExportError::InvalidInput, QStringLiteral("Cannot describe the container (resource id or count problem)"));

    // ---- 4. write to a temporary sibling ----
    const QString tmp = tempSiblingName(targetPath);
    const auto cleanup = [&]() { QFile::remove(tmp); };
    const Result wr = w.write(tmp, [&](quint64 d, quint64 t) { return report(ExportProgress::Writing, d, t, -1); });
    if (!wr.ok()) {
        cleanup();
        return fail(wr.error == Error::Cancelled ? ExportError::Cancelled : ExportError::WriteFailed,
                    wr.error == Error::Cancelled ? QStringLiteral("Export cancelled") : wr.message);
    }

    // ---- 5. verify: reopen, exact resource set, and NO image resource ----
    if (!report(ExportProgress::Verifying, 0, 1, -1)) {
        cleanup();
        return fail(ExportError::Cancelled, QStringLiteral("Export cancelled"));
    }
    {
        Reader rd;
        const Result o = rd.open(tmp);
        if (!o.ok()) {
            rd.close();
            cleanup();
            return fail(ExportError::VerifyFailed, QStringLiteral("Verification failed: ") + o.message);
        }
        int thumbs = 0;
        for (const LinkedImage &im : in.images)
            thumbs += im.thumbnail.isEmpty() ? 0 : 1;
        bool good = rd.resources().size() == 6 + thumbs;
        for (const ResourceInfo &e : rd.resources())
            if (e.type == TypeImage)
                good = false;
        rd.close();
        if (!good) {
            cleanup();
            return fail(ExportError::VerifyFailed, QStringLiteral("Verification failed: unexpected resource set"));
        }
    }
    // Source identity re-check: size + mtime first (cheap), then re-read and rehash EVERY source and compare
    // size + SHA-256 with the step-1 values (catches same-size edits with a restored mtime). Progress is
    // reported by bytes in the Verifying window (never backwards, never 100% before finalization).
    const auto changed = [&](int i, const QString &why) {
        cleanup();
        res.issues.append({i, in.images[i].sourcePath, SourceIssue::Changed, why});
        res.error = ExportError::SourceProblem;
        res.message = QStringLiteral("A referenced image changed during the export");
        return res;
    };
    for (int i = 0; i < in.images.size(); ++i)
        if (!detail::sameSource(in.images[i].sourcePath, srcs[i]))
            return changed(i, QStringLiteral("Source changed while exporting"));
    quint64 reDone = 0;
    for (int i = 0; i < in.images.size(); ++i) {
        detail::Source again;
        QString why;
        bool cancelled = false;
        const bool ok = detail::hashFile(in.images[i].sourcePath, &again, &why, [&](quint64 n) {
            reDone += n;
            if (!report(ExportProgress::Verifying, reDone, total, i)) {
                cancelled = true;
                return false;
            }
            return true;
        });
        if (cancelled) {
            cleanup();
            return fail(ExportError::Cancelled, QStringLiteral("Export cancelled"));
        }
        if (!ok)
            return changed(i, QStringLiteral("Source can no longer be read: ") + why);
        if (again.size != srcs[i].size || again.sha != srcs[i].sha)
            return changed(i, QStringLiteral("Content changed while exporting"));
    }

    // ---- 6. atomic swap into place (shared finalizer) ----
    report(ExportProgress::Finalizing, 0, 0, -1);
    const FinalizeResult fin = finalizeDestination(tmp, targetPath);
    if (!fin.ok()) {
        if (!fin.keepTemp)
            cleanup();
        return fail(ExportError::FinalizeFailed, fin.message);
    }
    res.fileBytes = quint64(QFileInfo(targetPath).size());
    report(ExportProgress::Done, 0, 0, -1);
    return res;
}

}  // namespace resora
