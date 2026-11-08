#include "resora/exportcommon.h"
#include "resora/importinternal.h"

#include <QDir>
#include <QFileInfo>

namespace resora::detail {

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
        total += r.size;
    for (const Record &rec : p.records) {
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
            if (rp.isEmpty() || !QDir::isAbsolutePath(rp)) {
                st.state = LinkState::UnsafePath;
                st.resolvedPath = rp;
                st.viaRelink = true;
                st.message = QStringLiteral("Relink path must be absolute");
            } else {
                res = tryPath(QDir::cleanPath(rp), false, true);
            }
        } else {
            if (!QDir::isAbsolutePath(rec.path)) {
                st.state = LinkState::UnsafePath;
                st.resolvedPath = rec.path;
                st.message = QStringLiteral("Recorded path is not absolute");
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
