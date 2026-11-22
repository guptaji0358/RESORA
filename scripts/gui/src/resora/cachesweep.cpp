#include "resora/importinternal.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <string>

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

namespace detail {

DirLock::DirLock(const QString &dir)
{
#ifdef Q_OS_WIN
    const std::wstring w = QDir::toNativeSeparators(dir).toStdWString();
    HANDLE h = CreateFileW(w.c_str(), FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(h, &info)) {
        CloseHandle(h);
        return;
    }
    reparse_ = (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    if (reparse_ || !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        CloseHandle(h);
        return;
    }
    h_ = h;
    ok_ = true;
#else
    Q_UNUSED(dir);
    ok_ = true;
#endif
}

void DirLock::release()
{
#ifdef Q_OS_WIN
    if (h_)
        CloseHandle(HANDLE(h_));
#endif
    h_ = nullptr;
}

CleanupResult removeOwnedCacheDir(const QString &dirIn, CleanupMode mode)
{
    CleanupResult r;
    if (dirIn.isEmpty()) {
        r.status = CleanupStatus::NotOwned;
        r.detail = QStringLiteral("No folder given");
        return r;
    }
    const QString dir = QDir::cleanPath(dirIn);
    const QFileInfo fi(dir);
    // lstat-like: a dangling or live link must be seen as a link, not as "gone".
    if (!fi.exists() && !isLinkLike(fi)) {
        r.status = CleanupStatus::AlreadyGone;
        return r;
    }
    const auto refuse = [&](const QString &why) {
        r.status = CleanupStatus::NotOwned;
        r.detail = why;
        return r;
    };
    static const QRegularExpression folder(QStringLiteral("^doc-[0-9a-f]{32}$"));
    if (!folder.match(fi.fileName()).hasMatch())
        return refuse(QStringLiteral("Folder name is not a RESORA cache name"));
    if (isLinkLike(fi) || !fi.isDir())
        return refuse(QStringLiteral("Not a plain directory (link or file)"));
    // From here until the final rmdir the folder cannot be swapped for a junction/symlink or renamed.
    DirLock lock(dir);
    if (lock.isReparsePoint())
        return refuse(QStringLiteral("Not a plain directory (link or file)"));
    if (!lock.ok()) {
        r.status = CleanupStatus::Failed;
        r.detail = QStringLiteral("Cache folder could not be locked for removal (in use); retry later");
        return r;
    }
    QStringList files;
    if (mode == CleanupMode::Abort && !QFileInfo::exists(dir + QLatin1Char('/') + QLatin1String(kCacheMarker))) {
        // Created by this very open; the marker write may have failed. Content rules below still apply.
    } else {
        const Owner o = readMarker(dir);
        if (!o.valid)
            return refuse(QStringLiteral("Cache marker is missing or invalid"));
        if (mode == CleanupMode::Close && o.pid != 0 && o.pid != QCoreApplication::applicationPid())
            return refuse(QStringLiteral("Cache folder belongs to another RESORA process"));
    }
    if (!contentIsOurs(dir, &files))
        return refuse(QStringLiteral("Folder holds entries RESORA did not create (kept untouched)"));
    // Marker LAST: if anything fails the folder still proves its ownership, so a later retry (or the
    // startup sweep) can finish the job instead of finding an unmarked, "foreign-looking" leftover.
    const QString markerPath = QDir::cleanPath(dir + QLatin1Char('/') + QLatin1String(kCacheMarker));
    bool ok = true;
    for (const QString &f : files)
        if (QDir::cleanPath(f) != markerPath && !QFile::remove(f) && QFileInfo::exists(f))
            ok = false;
    if (ok && QFileInfo::exists(markerPath) && !QFile::remove(markerPath))
        ok = false;
    lock.release();  // the lock would block our own rmdir; on a junction swapped in now rmdir removes only the link
    if (!ok || !QDir().rmdir(dir)) {
        r.status = CleanupStatus::Failed;
        r.detail = QStringLiteral("Some cache files could not be removed (in use or read-only); retry later");
        return r;
    }
    return r;
}

}  // namespace detail

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
        // Shared ownership check + non-recursive deletion (re-validates the folder just before removing).
        if (detail::removeOwnedCacheDir(fi.filePath(), detail::CleanupMode::Sweep).done()) {
            ++rep.removed;
            rep.removedDirs << fi.filePath();
        } else {
            ++rep.failed;
        }
    }
    return rep;
}

}  // namespace resora
