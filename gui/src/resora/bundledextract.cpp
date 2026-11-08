#include "resora/exportcommon.h"
#include "resora/importinternal.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
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

namespace {
constexpr const char *kMarker = ".resora-cache";

// File extension taken from the (untrusted) display name only when it is short and alphanumeric.
QString safeExtension(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9]{1,5}$"));
    const QString s = QFileInfo(name).suffix();
    return re.match(s).hasMatch() ? QLatin1Char('.') + s.toLower() : QString();
}

OpenResult abortWith(const QString &dir, OpenError e, const QString &m)
{
    QDir(dir).removeRecursively();  // `dir` was created by us a moment ago and holds only our files
    return failWith(e, m);
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
        QFile m(dir + QLatin1Char('/') + QLatin1String(kMarker));
        if (!m.open(QIODevice::WriteOnly) || m.write(p->doc.documentId.toLatin1()) < 0)
            return abortWith(dir, OpenError::ExtractionFailed, QStringLiteral("Cannot write the cache marker"));
    }
    quint64 done = 0;
    if (!rep(ImportProgress::Extracting, 0, total))
        return abortWith(dir, OpenError::Cancelled, QStringLiteral("Opening cancelled"));
    for (Record &rec : p->records) {
        // Name is generated from the validated resource id + a sanitised extension; never from file contents.
        const QString final = dir + QLatin1Char('/') + rec.bundledResource + safeExtension(rec.ref.name);
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
    const QFileInfo fi(doc->cacheDir);
    const bool marked = fi.isDir() && !fi.isSymLink() && fi.fileName().startsWith(QLatin1String("doc-"))
                        && QFile::exists(doc->cacheDir + QLatin1Char('/') + QLatin1String(detail::kMarker));
    bool gone = !fi.exists();
    if (marked)
        gone = QDir(doc->cacheDir).removeRecursively();
    if (gone) {
        doc->cacheDir.clear();
        for (ImageRef &r : doc->images)
            r.path.clear();
    }
    return gone;
}

}  // namespace resora
