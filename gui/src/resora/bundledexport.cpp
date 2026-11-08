#include "resora/bundledexport.h"

#include "resora/destinationfinalizer.h"
#include "resora/exportcommon.h"
#include "resora/resorareader.h"
#include "resora/resorawriter.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QUuid>

namespace resora {

namespace {
using detail::Source;
using detail::hashFile;
using detail::kPathCase;
using detail::num;
using detail::sameSource;

ExportResult fail(ExportError e, const QString &m)
{
    ExportResult r;
    r.error = e;
    r.message = m;
    return r;
}

}  // namespace

QString imageResourceId(int index) { return QStringLiteral("image-") + num(index); }
QString thumbResourceId(int index) { return QStringLiteral("thumb-") + num(index); }
QString imageId(int index) { return QStringLiteral("img-") + num(index); }

ExportResult exportBundled(const BundledExportInput &in, const QString &targetPath, const ExportProgressFn &progress)
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
    const QString targetCanon = QFileInfo(targetInfo.absoluteDir().canonicalPath() + QLatin1Char('/') + targetInfo.fileName()).absoluteFilePath();

    int lastPermille = 0;
    const auto report = [&](ExportProgress::Stage st, quint64 d, quint64 t, int idx) {
        // Stage windows of the overall bar: validate 0-300, write 300-800, verify 800-900, finalize 900, done 1000.
        static constexpr int base[] = {0, 300, 800, 900, 1000};
        static constexpr int span[] = {300, 500, 100, 0, 0};
        const int frac = t > 0 ? int(qMin<quint64>(span[st], quint64(span[st]) * d / t)) : 0;
        lastPermille = qMax(lastPermille, base[st] + frac);
        return !progress || progress({st, d, t, idx, lastPermille});
    };

    // ---- 1. validate sources and take identity (size, mtime, SHA-256), streaming ----
    ExportResult res;
    QList<Source> srcs(in.images.size());
    quint64 total = 0;
    for (int i = 0; i < in.images.size(); ++i) {
        const QFileInfo fi(in.images[i].sourcePath);
        if (in.images[i].sourcePath.isEmpty() || !fi.exists() || !fi.isFile())
            continue;
        total += quint64(qMax<qint64>(0, fi.size()));
    }
    quint64 done = 0;
    for (int i = 0; i < in.images.size(); ++i) {
        const QString &p = in.images[i].sourcePath;
        const QFileInfo fi(p);
        if (p.isEmpty() || !fi.exists()) {
            res.issues.append({i, p, SourceIssue::Missing, QStringLiteral("File not found")});
            continue;
        }
        if (QString::compare(fi.canonicalFilePath(), targetCanon, kPathCase) == 0) {
            return fail(ExportError::InvalidInput, QStringLiteral("The destination is one of the source images: ") + p);
        }
        if (!fi.isFile()) {
            res.issues.append({i, p, SourceIssue::Unreadable, QStringLiteral("Not a regular file")});
            continue;
        }
        srcs[i].mtimeMs = fi.lastModified().toMSecsSinceEpoch();
        QString why;
        bool cancelled = false;
        const bool ok = hashFile(p, &srcs[i], &why, [&](quint64 n) {
            done += n;
            if (!report(ExportProgress::Validating, done, total, i)) {
                cancelled = true;
                return false;
            }
            return true;
        });
        if (cancelled)
            return fail(ExportError::Cancelled, QStringLiteral("Export cancelled"));
        if (!ok)
            res.issues.append({i, p, SourceIssue::Unreadable, QStringLiteral("Cannot read file: ") + why});
    }
    if (!res.issues.isEmpty()) {
        res.error = ExportError::SourceProblem;
        res.message = QStringLiteral("%1 source image(s) cannot be packaged").arg(res.issues.size());
        return res;
    }

    // ---- 2. build the container description ----
    res.documentId = QUuid::createUuid().toString(QUuid::Id128);
    res.imageCount = in.images.size();
    Writer w;
    QJsonArray records, inventory;
    const auto add = [&](const Result &r) { return r.ok(); };
    const auto addJson = [&](const QString &id, const QJsonObject &o) { return detail::addJsonResource(w, inventory, id, o); };
    bool built = true;
    for (int i = 0; i < in.images.size(); ++i) {
        const ExportImage &im = in.images[i];
        QJsonObject rec{{"image_id", imageId(i)},
                        {"name", im.name.isEmpty() ? QFileInfo(im.sourcePath).fileName() : im.name},
                        {"metadata", im.metadata},
                        {"thumbnail", QJsonValue::Null}};
        if (!im.thumbnail.isEmpty()) {
            built = built && add(w.addBytes(thumbResourceId(i), TypeThumbnail, im.thumbnail));
            inventory.append(QJsonObject{{"id", thumbResourceId(i)}, {"type", "thumbnail"}});
            rec["thumbnail"] = thumbResourceId(i);
            if (!im.thumbnailFormat.isEmpty())
                rec["thumbnail_format"] = im.thumbnailFormat;
        }
        built = built && add(w.addFile(imageResourceId(i), TypeImage, im.sourcePath));
        inventory.append(QJsonObject{{"id", imageResourceId(i)}, {"type", "image"}});
        rec["bundled"] = QJsonObject{{"resource", imageResourceId(i)},
                                     {"sha256", QString::fromLatin1(srcs[i].sha.toHex())},
                                     {"size", double(srcs[i].size)}};
        records.append(rec);
        res.imageBytes += srcs[i].size;
    }
    QJsonObject images{{"schema_version", 1}, {"images", records}};
    built = built && detail::addCommonResources(w, inventory, in.layout, in.charts, in.analytics, in.engineProtocol, in.config);
    built = built && addJson(QStringLiteral("images-index"), images);
    inventory.append(QJsonObject{{"id", "manifest"}, {"type", "json"}});
    QJsonObject manifest{{"schema_version", 1},
                         {"format_version", QStringLiteral("%1.%2").arg(kVersionMajor).arg(kVersionMinor)},
                         {"app_version", in.appVersion},
                         {"mode", "bundled"},
                         {"created_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                         {"document_id", res.documentId},
                         {"image_count", in.images.size()},
                         {"source", in.source.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(in.source)},
                         {"resources", inventory}};
    built = built && addJson(QStringLiteral("manifest"), manifest);
    if (!built)
        return fail(ExportError::InvalidInput, QStringLiteral("Cannot describe the container (resource id or count problem)"));

    // ---- 3. write to a temporary sibling (Phase 1 Writer streams the images) ----
    const QString tmp = tempSiblingName(targetPath);
    const auto cleanup = [&]() { QFile::remove(tmp); };
    const Result wr = w.write(tmp, [&](quint64 d, quint64 t) { return report(ExportProgress::Writing, d, t, -1); });
    if (!wr.ok()) {
        cleanup();
        return fail(wr.error == Error::Cancelled ? ExportError::Cancelled : ExportError::WriteFailed,
                    wr.error == Error::Cancelled ? QStringLiteral("Export cancelled") : wr.message);
    }

    // ---- 4. verify: reopen with the Reader, check every resource and every image identity ----
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
        const int expected = 6 + in.images.size() + [&] { int n = 0; for (const ExportImage &im : in.images) n += im.thumbnail.isEmpty() ? 0 : 1; return n; }();
        bool good = rd.resources().size() == expected;
        for (int i = 0; good && i < in.images.size(); ++i) {
            const ResourceInfo e = rd.info(imageResourceId(i));
            if (e.id.isEmpty() || e.storedLength != srcs[i].size || e.sha256 != srcs[i].sha) {
                res.issues.append({i, in.images[i].sourcePath, SourceIssue::Changed,
                                   QStringLiteral("Source changed while exporting")});
                good = false;
            }
        }
        rd.close();
        if (!good) {
            cleanup();
            if (!res.issues.isEmpty()) {
                res.error = ExportError::SourceProblem;
                res.message = QStringLiteral("A source image changed during the export");
                return res;
            }
            return fail(ExportError::VerifyFailed, QStringLiteral("Verification failed: unexpected resource set"));
        }
    }
    for (int i = 0; i < in.images.size(); ++i)
        if (!sameSource(in.images[i].sourcePath, srcs[i])) {
            cleanup();
            res.issues.append({i, in.images[i].sourcePath, SourceIssue::Changed, QStringLiteral("Source changed while exporting")});
            res.error = ExportError::SourceProblem;
            res.message = QStringLiteral("A source image changed during the export");
            return res;
        }

    // ---- 5. atomic swap into place ----
    report(ExportProgress::Finalizing, 0, 0, -1);
    const FinalizeResult fin = finalizeDestination(tmp, targetPath);  // ReplaceFileW / MoveFileExW; see destinationfinalizer.h
    if (!fin.ok()) {
        if (!fin.keepTemp)  // uncertain state: the temp may be the only valid copy, never delete it
            cleanup();
        return fail(ExportError::FinalizeFailed, fin.message);
    }
    res.fileBytes = quint64(QFileInfo(targetPath).size());
    report(ExportProgress::Done, 0, 0, -1);  // 100% only now that the file is in place
    return res;
}

}  // namespace resora
