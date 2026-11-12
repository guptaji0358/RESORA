#include "resora/exportcommon.h"
#include "resora/importinternal.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUuid>

namespace resora {

QString defaultCacheRoot()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty())
        base = QDir::tempPath() + QStringLiteral("/resora-cache");
    return QDir::cleanPath(base + QStringLiteral("/documents"));
}

namespace detail {

QByteArray cacheMarkerContent(const QString &documentId)
{
    const QJsonObject o{{"app", "RESORA"},
                        {"kind", "resora-document-cache"},
                        {"version", 1},
                        {"document_id", documentId},
                        {"pid", double(QCoreApplication::applicationPid())},
                        {"created_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QString safeImageExtension(const QString &displayName)
{
    static const QSet<QString> known{QStringLiteral("png"),  QStringLiteral("jpg"),  QStringLiteral("jpeg"), QStringLiteral("gif"),
                                     QStringLiteral("bmp"),  QStringLiteral("webp"), QStringLiteral("tif"),  QStringLiteral("tiff"),
                                     QStringLiteral("ico"),  QStringLiteral("heic"), QStringLiteral("heif"), QStringLiteral("avif"),
                                     QStringLiteral("jxl"),  QStringLiteral("jp2"),  QStringLiteral("dds"),  QStringLiteral("tga")};
    const QString s = QFileInfo(displayName).suffix().toLower();
    return known.contains(s) ? QLatin1Char('.') + s : QStringLiteral(".img");
}

QString safeStem(const QString &id)
{
    static const QRegularExpression device(QStringLiteral("^(con|prn|aux|nul|com[0-9]|lpt[0-9])$"));
    return device.match(id).hasMatch() ? QStringLiteral("r-") + id : id;
}

namespace {
OpenResult abortWith(const QString &dir, OpenError e, const QString &m)
{
    // `dir` was created by us a moment ago; the same ownership rules as every other removal apply
    // (non-recursive, never follows links, keeps anything foreign).
    const CleanupResult c = removeOwnedCacheDir(dir, CleanupMode::Abort);
    OpenResult r = failWith(e, c.done() ? m : m + QStringLiteral(" (cache folder kept: ") + c.detail + QLatin1Char(')'));
    return r;
}
}  // namespace

OpenResult extractBundled(Reader &rd, Parsed *p, const QString &cacheRoot, Reporter &rep)
{
    quint64 total = 0;
    for (const Record &r : p->records)
        total += r.size;
    if (!QDir().mkpath(cacheRoot))
        return failWith(OpenError::ExtractionFailed, QStringLiteral("Cannot create the cache folder: ") + cacheRoot);
    const QStorageInfo si(cacheRoot);
    if (si.isValid() && si.bytesAvailable() >= 0 && quint64(si.bytesAvailable()) < total + (Q_UINT64_C(16) << 20))
        return failWith(OpenError::InsufficientStorage,
                        QStringLiteral("Not enough free space to extract %1 bytes").arg(total));

    // Unique, freshly created folder: mkdir fails if it already exists, so existing data is never reused.
    const QString dir = QDir::cleanPath(cacheRoot + QStringLiteral("/doc-") + QUuid::createUuid().toString(QUuid::Id128));
    if (!QDir().mkdir(dir))
        return failWith(OpenError::ExtractionFailed, QStringLiteral("Cannot create the extraction folder"));
    {
        QFile m(dir + QLatin1Char('/') + QLatin1String(kCacheMarker));
        if (!m.open(QIODevice::WriteOnly) || m.write(cacheMarkerContent(p->doc.documentId)) < 0)
            return abortWith(dir, OpenError::ExtractionFailed, QStringLiteral("Cannot write the cache marker"));
    }
    quint64 done = 0;
    if (!rep(ImportProgress::Extracting, 0, total))
        return abortWith(dir, OpenError::Cancelled, QStringLiteral("Opening cancelled"));
    for (Record &rec : p->records) {
        // Name is generated from the validated resource id + a sanitised extension; never from file contents.
        const QString final = dir + QLatin1Char('/') + safeStem(rec.bundledResource) + safeImageExtension(rec.ref.name);
        const QString part = final + QStringLiteral(".part");
        {
            QFile f(part);
            if (!f.open(QIODevice::WriteOnly))
                return abortWith(dir, OpenError::ExtractionFailed, QStringLiteral("Cannot create ") + rec.bundledResource);
            bool stopped = false;
            const quint64 start = done;
            const Result r = rd.copyResource(rec.bundledResource, &f, [&](quint64 n) {
                if (!rep(ImportProgress::Extracting, start + n, total * 2)) {
                    stopped = true;
                    return false;
                }
                return true;
            });
            const bool flushed = r.ok() && f.flush();
            f.close();
            if (stopped)  // closed above; abortWith removes the .part with the whole fresh folder
                return abortWith(dir, OpenError::Cancelled, QStringLiteral("Opening cancelled"));
            if (!r.ok() || !flushed) {
                const QStorageInfo now(dir);
                const bool full = now.isValid() && now.bytesAvailable() >= 0 && quint64(now.bytesAvailable()) < rec.size;
                return abortWith(dir, full ? OpenError::InsufficientStorage : OpenError::ExtractionFailed,
                                 QStringLiteral("Image '%1' could not be extracted: %2")
                                     .arg(rec.ref.imageId, r.ok() ? QStringLiteral("write failed") : r.message));
            }
        }
        // Independent verification of the bytes now on disk (size + SHA-256) before exposing the file.
        Source s;
        QString why;
        bool cancelled = false;
        const quint64 before = done;
        const bool ok = hashFile(part, &s, &why, [&](quint64 n) {
            if (!rep(ImportProgress::Extracting, before + n, total * 2)) {
                cancelled = true;
                return false;
            }
            return true;
        });
        if (cancelled)
            return abortWith(dir, OpenError::Cancelled, QStringLiteral("Opening cancelled"));
        if (!ok || s.size != rec.size || QString::fromLatin1(s.sha.toHex()) != rec.sha256)
            return abortWith(dir, OpenError::ExtractionFailed,
                             QStringLiteral("Image '%1' failed size/SHA-256 verification after extraction").arg(rec.ref.imageId));
        if (!QFile::rename(part, final))
            return abortWith(dir, OpenError::ExtractionFailed, QStringLiteral("Cannot finalise ") + rec.bundledResource);
        rec.ref.path = final;
        done += rec.size;
        if (!rep(ImportProgress::Extracting, done, total))
            return abortWith(dir, OpenError::Cancelled, QStringLiteral("Opening cancelled"));
    }
    p->doc.cacheDir = dir;
    return {};
}

}  // namespace detail

bool closeDocument(OpenedDocument *doc)
{
    if (!doc || doc->cacheDir.isEmpty())
        return true;
    // Same ownership policy as the startup sweep; on NotOwned/Failed the folder is preserved, cacheDir is kept
    // so the caller can report and retry, and false is returned.
    const detail::CleanupResult c = detail::removeOwnedCacheDir(doc->cacheDir, detail::CleanupMode::Close);
    const bool gone = c.done();
    if (gone) {
        doc->cacheDir.clear();
        for (ImageRef &r : doc->images)
            r.path.clear();
    }
    return gone;
}

}  // namespace resora
