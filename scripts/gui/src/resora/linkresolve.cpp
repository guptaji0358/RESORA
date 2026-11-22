#include "resora/exportcommon.h"
#include "resora/importinternal.h"

#include <QDir>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace resora::detail {

#ifdef Q_OS_WIN
namespace {
// Classifies a drive letter WITHOUT touching the file system behind it. QueryDosDeviceW only reads the
// object-manager symbolic link of "X:" (no I/O to the volume or the network), so a mapped share, a
// disconnected mapping and an unreachable server all answer instantly and identically.
bool driveIsRemote(wchar_t letter, int depth)
{
    if (depth > 4)  // SUBST chain too deep or cyclic: not provably local
        return true;
    const wchar_t dev[3] = {letter, L':', 0};
    wchar_t buf[1024] = {};
    if (QueryDosDeviceW(dev, buf, DWORD(sizeof(buf) / sizeof(buf[0]))) == 0)
        return false;  // letter not assigned: nothing can be contacted; the lookup will report "not found"
    const QString t = QString::fromWCharArray(buf);  // first (current) mapping of the multi-string
    const auto starts = [&](const char *p) { return t.startsWith(QLatin1String(p), Qt::CaseInsensitive); };
    if (starts("\\??\\")) {  // SUBST: judge what it points at
        const QString rest = t.mid(4);
        if (rest.startsWith(QLatin1String("UNC\\"), Qt::CaseInsensitive))
            return true;
        if (rest.size() >= 2 && rest[1] == QLatin1Char(':') && rest[0].isLetter())
            return driveIsRemote(wchar_t(rest[0].toUpper().unicode()), depth + 1);
        return false;  // \??\Volume{guid}\ or a local directory
    }
    for (const char *remote : {"\\Device\\LanmanRedirector", "\\Device\\Mup", "\\Device\\WebDavRedirector", "\\Device\\MRxDAV",
                               "\\Device\\VBoxMiniRdr", "\\Device\\vmhgfs", "\\Device\\WinFsp"})
        if (starts(remote))
            return true;
    for (const char *local : {"\\Device\\HarddiskVolume", "\\Device\\Harddisk", "\\Device\\CdRom", "\\Device\\Floppy"})
        if (starts(local))
            return false;
    // Unknown device (third-party volume driver): ask the OS and stay conservative.
    const wchar_t root[4] = {letter, L':', L'\\', 0};
    const UINT type = GetDriveTypeW(root);
    return type == DRIVE_REMOTE || type == DRIVE_UNKNOWN || type == DRIVE_NO_ROOT_DIR;
}
}  // namespace
#endif

bool isRemoteDriveLetter(QChar letter)
{
#ifdef Q_OS_WIN
    if (!letter.isLetter() || letter.unicode() > 0x7f)
        return false;
    return driveIsRemote(wchar_t(letter.toUpper().unicode()), 0);
#else
    Q_UNUSED(letter);
    return false;
#endif
}

bool isSafeRelativePath(const QString &rel)
{
    if (rel.isEmpty() || rel.size() > 1024 || rel.startsWith(QLatin1Char('/')))
        return false;
    for (const QChar c : rel)
        if (c == QLatin1Char('\\') || c == QLatin1Char(':') || c.unicode() < 0x20)
            return false;
    const QStringList parts = rel.split(QLatin1Char('/'));
    for (const QString &s : parts)
        if (s.isEmpty() || s == QLatin1String(".") || s == QLatin1String(".."))
            return false;
    return true;
}

bool isLocalAbsolutePath(const QString &path)
{
    if (path.isEmpty() || !QDir::isAbsolutePath(path))
        return false;
    // Decided from the string only, before any filesystem call: no "\\server\share", "//server/share",
    // "\\?\..." or "\\.\..." (any two leading separators of either kind), and no stray control characters.
    const auto sep = [](QChar c) { return c == QLatin1Char('/') || c == QLatin1Char('\\'); };
    if (path.size() >= 2 && sep(path[0]) && sep(path[1]))
        return false;
    // "C:foo" is relative to a per-drive current directory, not an absolute location.
    if (path.size() >= 2 && path[1] == QLatin1Char(':') && (path.size() == 2 || !sep(path[2])))
        return false;
    for (const QChar c : path)
        if (c.unicode() < 0x20)
            return false;
    // Mapped network drive / SUBST of a share: judged from the local DOS-device table only (no remote access).
    if (path.size() >= 2 && path[1] == QLatin1Char(':') && isRemoteDriveLetter(path[0]))
        return false;
    return true;
}

QString joinInside(const QString &base, const QString &rel)
{
    if (base.isEmpty() || !isSafeRelativePath(rel) || !QDir::isAbsolutePath(base))
        return QString();
    const QString b = QDir::cleanPath(base);
    const QString j = QDir::cleanPath(b + QLatin1Char('/') + rel);
    const QString prefix = b.endsWith(QLatin1Char('/')) ? b : b + QLatin1Char('/');
    if (!j.startsWith(prefix, kPathCase))
        return QString();
    // A symlink/junction inside the base must not lead outside it.
    const QFileInfo fi(j);
    if (fi.exists()) {
        const QString cb = QFileInfo(b).canonicalFilePath(), cj = fi.canonicalFilePath();
        if (!cb.isEmpty() && !cj.startsWith(cb.endsWith(QLatin1Char('/')) ? cb : cb + QLatin1Char('/'), kPathCase))
            return QString();
    }
    return j;
}

namespace {
struct Check
{
    LinkState state = LinkState::Missing;
    QString message;
    bool cancelled = false;
};

int rank(LinkState s)
{
    switch (s) {
    case LinkState::IdentityMismatch: return 4;
    case LinkState::Unreadable: return 3;
    case LinkState::UnsafePath: return 2;
    case LinkState::Missing: return 1;
    default: return 0;
    }
}

// Read-only identity check: exists, regular, readable, size, then SHA-256 (streamed, cancellable).
Check checkFile(const QString &path, const Record &rec, quint64 *done, quint64 total, Reporter &rep)
{
    Check c;
    const QFileInfo fi(path);
    if (!fi.exists()) {
        c.message = QStringLiteral("File not found");
        return c;
    }
    if (!fi.isFile()) {
        c.state = LinkState::Unreadable;
        c.message = QStringLiteral("Not a regular file");
        return c;
    }
    if (quint64(fi.size()) != rec.size) {
        c.state = LinkState::IdentityMismatch;
        c.message = QStringLiteral("Size differs (%1 bytes, expected %2)").arg(fi.size()).arg(rec.size);
        return c;
    }
    Source s;
    QString why;
    const bool ok = hashFile(path, &s, &why, [&](quint64 n) {
        *done += n;
        if (!rep(ImportProgress::Validating, *done, total)) {
            c.cancelled = true;
            return false;
        }
        return true;
    });
    if (c.cancelled)
        return c;
    if (!ok) {
        c.state = LinkState::Unreadable;
        c.message = QStringLiteral("Cannot read file: ") + why;
        return c;
    }
    if (QString::fromLatin1(s.sha.toHex()) != rec.sha256) {
        c.state = LinkState::IdentityMismatch;
        c.message = QStringLiteral("Content differs from the recorded SHA-256");
        return c;
    }
    c.state = LinkState::Ok;
    return c;
}
}  // namespace

bool validateLinks(const Parsed &p, const OpenOptions &opts, const QString &docDir, Reporter &rep,
                   QList<LinkStatus> *out)
{
    quint64 total = 0, done = 0;
    for (const Record &r : p.records)
        if (!opts.omitImages.contains(r.ref.imageId))
            total += r.size;
    for (const Record &rec : p.records) {
        if (opts.omitImages.contains(rec.ref.imageId)) {
            LinkStatus om;  // explicit, user-approved omission: not checked, reported as Omitted
            om.index = rec.ref.index;
            om.imageId = rec.ref.imageId;
            om.name = rec.ref.name;
            om.recordedPath = rec.path;
            om.relativePath = rec.relativePath;
            om.state = LinkState::Omitted;
            om.message = QStringLiteral("Omitted by request");
            out->append(om);
            continue;
        }
        LinkStatus st;
        st.index = rec.ref.index;
        st.imageId = rec.ref.imageId;
        st.name = rec.ref.name;
        st.recordedPath = rec.path;
        st.relativePath = rec.relativePath;
        const auto tryPath = [&](const QString &path, bool rel, bool relink) -> int {  // 1 ok, 0 failed, -1 cancel
            const Check c = checkFile(path, rec, &done, total, rep);
            if (c.cancelled)
                return -1;
            if (c.state == LinkState::Ok) {
                st.state = LinkState::Ok;
                st.resolvedPath = path;
                st.viaRelative = rel;
                st.viaRelink = relink;
                st.message.clear();
                return 1;
            }
            if (rank(c.state) >= rank(st.state) || st.resolvedPath.isEmpty()) {
                st.state = c.state;
                st.resolvedPath = path;
                st.viaRelative = rel;
                st.viaRelink = relink;
            }
            st.message += (st.message.isEmpty() ? QString() : QStringLiteral("; ")) + path + QStringLiteral(": ") + c.message;
            return 0;
        };
        st.state = LinkState::Missing;
        int res = 0;
        const auto relinkIt = opts.relinks.constFind(rec.ref.imageId);
        if (relinkIt != opts.relinks.constEnd()) {
            // An explicit relink is the only candidate: no silent fall back to another file.
            const QString rp = relinkIt.value();
            if (!isLocalAbsolutePath(rp)) {
                st.state = LinkState::UnsafePath;
                st.resolvedPath = rp;
                st.viaRelink = true;
                st.message = QDir::isAbsolutePath(rp) ? QStringLiteral("Network or device paths are not allowed for relinking")
                                                      : QStringLiteral("Relink path must be absolute");
            } else {
                res = tryPath(QDir::cleanPath(rp), false, true);
            }
        } else {
            if (!isLocalAbsolutePath(rec.path)) {
                st.state = LinkState::UnsafePath;
                st.resolvedPath = rec.path;
                st.message = QDir::isAbsolutePath(rec.path) ? QStringLiteral("Network or device path rejected (not accessed)")
                                                            : QStringLiteral("Recorded path is not absolute");
            } else {
                res = tryPath(QDir::cleanPath(rec.path), false, false);
            }
            if (res == 0 && !rec.relativePath.isEmpty()) {
                if (!isSafeRelativePath(rec.relativePath)) {
                    st.message += (st.message.isEmpty() ? QString() : QStringLiteral("; ")) + QStringLiteral("unsafe relative path rejected");
                    if (rank(LinkState::UnsafePath) > rank(st.state))
                        st.state = LinkState::UnsafePath;
                } else {
                    QStringList bases;
                    if (p.baseKind == QLatin1String("folder")) {
                        if (!opts.imageRoot.isEmpty())
                            bases << opts.imageRoot;
                        if (!p.baseHint.isEmpty() && QString::compare(p.baseHint, opts.imageRoot, kPathCase) != 0)
                            bases << p.baseHint;
                    } else if (p.baseKind == QLatin1String("document")) {
                        bases << docDir;
                    }
                    for (const QString &b : bases) {
                        // Base folders from the file (hint_path) or the caller must be local; the document's own
                        // folder is where the user opened the file, so it is the one permitted exception.
                        if (b != docDir && !isLocalAbsolutePath(b)) {
                            st.message += (st.message.isEmpty() ? QString() : QStringLiteral("; ")) + QStringLiteral("network base folder rejected (not accessed)");
                            if (rank(LinkState::UnsafePath) > rank(st.state))
                                st.state = LinkState::UnsafePath;
                            continue;
                        }
                        const QString j = joinInside(b, rec.relativePath);
                        if (j.isEmpty()) {
                            st.message += (st.message.isEmpty() ? QString() : QStringLiteral("; ")) + QStringLiteral("relative path escapes its base");
                            continue;
                        }
                        res = tryPath(j, true, false);
                        if (res != 0)
                            break;
                    }
                }
            }
        }
        if (res < 0)
            return false;
        out->append(st);
    }
    return true;
}

}  // namespace resora::detail
