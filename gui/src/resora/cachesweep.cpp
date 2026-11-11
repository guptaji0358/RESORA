#include "resora/importinternal.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#endif

namespace resora {

namespace {

// Conservative: unknown => "running" (so the folder is kept).
bool processRunning(qint64 pid)
{
    if (pid <= 0)
        return true;
#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(pid));
    if (!h)
        return GetLastError() != ERROR_INVALID_PARAMETER;  // access denied etc.: assume alive
    DWORD code = 0;
    const bool alive = !GetExitCodeProcess(h, &code) || code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
#else
    return ::kill(pid_t(pid), 0) == 0 || errno == EPERM;
#endif
}

bool isLinkLike(const QFileInfo &fi) { return fi.isSymLink() || fi.isJunction() || fi.isShortcut(); }

struct Owner
{
    bool valid = false;   // marker recognised as ours
    qint64 pid = 0;       // 0 => unknown (legacy marker)
};

Owner readMarker(const QString &dir)
{
    Owner o;
    const QFileInfo mi(dir + QLatin1Char('/') + QLatin1String(detail::kCacheMarker));
    if (!mi.isFile() || isLinkLike(mi) || mi.size() > 4096)
        return o;
    QFile f(mi.filePath());
    if (!f.open(QIODevice::ReadOnly))
        return o;
    const QByteArray b = f.readAll();
    static const QRegularExpression legacy(QStringLiteral("^[0-9a-f]{32}$"));  // Phase 4 marker: document id only
    if (legacy.match(QString::fromLatin1(b)).hasMatch()) {
        o.valid = true;
        return o;
    }
    const QJsonObject j = QJsonDocument::fromJson(b).object();
    if (j.value("app").toString() != QLatin1String("RESORA") || j.value("kind").toString() != QLatin1String("resora-document-cache")
        || j.value("version").toInt(-1) != 1 || !j.value("pid").isDouble())
        return o;
    o.pid = qint64(j.value("pid").toDouble());
    o.valid = o.pid > 0;
    return o;
}

// Every direct entry must be the marker or a regular file with a name we could have generated.
bool contentIsOurs(const QString &dir, QStringList *files)
{
    static const QRegularExpression name(QStringLiteral("^(r-)?[a-z0-9][a-z0-9_-]{0,62}\\.(png|jpe?g|gif|bmp|webp|tiff?|ico|heic|heif|avif|jxl|jp2|dds|tga|img)(\\.part)?$"));
    const QFileInfoList all = QDir(dir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    if (all.size() > 70000)
        return false;
    for (const QFileInfo &fi : all) {
        if (!fi.isFile() || isLinkLike(fi))
            return false;
        if (fi.fileName() != QLatin1String(detail::kCacheMarker) && !name.match(fi.fileName()).hasMatch())
            return false;
        files->append(fi.filePath());
    }
    return true;
}

}  // namespace

SweepReport sweepStaleCache(const QString &cacheRootIn, const SweepOptions &opts)
{
    SweepReport rep;
    const QString root = QDir::cleanPath(cacheRootIn.isEmpty() ? defaultCacheRoot() : cacheRootIn);
    const QFileInfo ri(root);
    if (!ri.isDir() || isLinkLike(ri))  // missing root: nothing to do; a linked root is never followed
        return rep;
    static const QRegularExpression folder(QStringLiteral("^doc-[0-9a-f]{32}$"));
    const qint64 self = QCoreApplication::applicationPid();
    const QDateTime now = QDateTime::currentDateTimeUtc();

    const QFileInfoList kids = QDir(root).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &fi : kids) {
        ++rep.examined;
        if (!folder.match(fi.fileName()).hasMatch() || !fi.isDir() || isLinkLike(fi)) {
            ++rep.skippedUnsafe;
            continue;
        }
        const Owner owner = readMarker(fi.filePath());
        QStringList files;
        if (!owner.valid || !contentIsOurs(fi.filePath(), &files)) {
            ++rep.skippedUnsafe;
            continue;
        }
        if (owner.pid == self || (owner.pid > 0 && processRunning(owner.pid))) {
            ++rep.skippedActive;  // an open dashboard (here or in another RESORA window) must never lose its images
            continue;
        }
        QDateTime newest = fi.lastModified();
        for (const QString &f : files)
            newest = qMax(newest, QFileInfo(f).lastModified());
        const qint64 needed = owner.pid > 0 ? opts.minAgeSeconds : opts.unknownOwnerMinAgeSeconds;
        if (newest.secsTo(now) < needed) {
            ++rep.skippedRecent;
            continue;
        }
        bool ok = true;  // files first (never recursive), then the now-empty folder
        for (const QString &f : files)
            ok = QFile::remove(f) && ok;
        ok = ok && QDir().rmdir(fi.filePath());
        if (ok) {
            ++rep.removed;
            rep.removedDirs << fi.filePath();
        } else {
            ++rep.failed;
        }
    }
    return rep;
}

}  // namespace resora
