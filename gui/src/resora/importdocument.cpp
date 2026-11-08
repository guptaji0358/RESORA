#include "resora/importinternal.h"

#include <QDir>
#include <QFileInfo>

namespace resora {

OpenResult openResora(const QString &path, const OpenOptions &opts, const ImportProgressFn &progress)
{
    using namespace detail;
    if (path.isEmpty())
        return failWith(OpenError::InvalidInput, QStringLiteral("No file given"));
    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile())
        return failWith(OpenError::InvalidInput, QStringLiteral("File not found: ") + path);
    Reporter rep(progress);
    if (!rep(ImportProgress::Reading, 0, 1))
        return failWith(OpenError::Cancelled, QStringLiteral("Opening cancelled"));

    Reader rd;  // read-only; the .resora is never modified
    const Result o = rd.open(path);
    if (!o.ok()) {
        OpenResult r = failWith(OpenError::Container, o.message);
        r.containerError = o.error;
        return r;
    }
    Parsed parsed;
    OpenResult err;
    if (!parseDocument(rd, fi.absoluteFilePath(), &parsed, &err))
        return err;
    if (!rep(ImportProgress::Reading, 1, 1))
        return failWith(OpenError::Cancelled, QStringLiteral("Opening cancelled"));

    OpenResult res;
    if (parsed.doc.linked) {
        QList<LinkStatus> links;
        if (!validateLinks(parsed, opts, fi.absoluteDir().absolutePath(), rep, &links))
            return failWith(OpenError::Cancelled, QStringLiteral("Opening cancelled"));
        for (const LinkStatus &s : links)
            if (!s.ok())
                res.problems.append(s);
        if (!res.problems.isEmpty()) {
            // Blocked: no document is handed out, only the diagnostics.
            res.error = OpenError::LinksBlocked;
            res.message = QStringLiteral("%1 of %2 linked image(s) cannot be opened").arg(res.problems.size()).arg(links.size());
            res.links = links;
            return res;
        }
        for (int i = 0; i < parsed.records.size(); ++i) {
            parsed.records[i].ref.path = links[i].resolvedPath;
        }
        res.links = links;
    } else {
        const OpenResult x = extractBundled(rd, &parsed, opts.cacheRoot.isEmpty() ? defaultCacheRoot() : opts.cacheRoot, rep);
        if (!x.ok())
            return x;
    }
    for (const Record &r : parsed.records)
        parsed.doc.images.append(r.ref);
    res.document = parsed.doc;
    rep(ImportProgress::Done, 1, 1);
    return res;
}

OpenResult relinkAndOpen(const QString &path, OpenOptions opts, const QHash<QString, QString> &replacements,
                         const ImportProgressFn &progress)
{
    for (auto it = replacements.constBegin(); it != replacements.constEnd(); ++it)
        opts.relinks.insert(it.key(), it.value());
    return openResora(path, opts, progress);
}

QByteArray readThumbnail(const OpenedDocument &doc, int index)
{
    if (index < 0 || index >= doc.images.size() || doc.images[index].thumbnailResource.isEmpty())
        return {};
    Reader rd;
    QByteArray b;
    if (!rd.open(doc.containerPath).ok() || !rd.readResource(doc.images[index].thumbnailResource, &b, Q_UINT64_C(32) << 20).ok())
        return {};
    return b;
}

}  // namespace resora
