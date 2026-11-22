#include "backend/resoraworkflow.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

#include "resora/exportcommon.h"

namespace resoraui {

namespace {
const QString kExt = QStringLiteral("resora");

bool samePath(const QString &a, const QString &b)
{
    return QString::compare(QDir::cleanPath(a), QDir::cleanPath(b), resora::detail::kPathCase) == 0;
}
}  // namespace

DestinationCheck checkDestination(const QString &input, const QStringList &sourceImages)
{
    DestinationCheck c;
    const QString text = QDir::fromNativeSeparators(input.trimmed());
    if (text.isEmpty()) {
        c.error = QObject::tr("Choose where to save the .resora file.");
        return c;
    }
    QFileInfo fi(text);
    if (fi.isDir() || text.endsWith(QLatin1Char('/'))) {
        c.error = QObject::tr("The destination is a folder. Enter a file name ending in .resora.");
        return c;
    }
    const QString suffix = fi.suffix();
    QString path = fi.absoluteFilePath();
    if (suffix.isEmpty())
        path += QLatin1Char('.') + kExt;
    else if (suffix.compare(kExt, Qt::CaseInsensitive) != 0) {
        c.error = QObject::tr("The file name must end in .%1 (not .%2).").arg(kExt, suffix);
        return c;
    } else if (suffix != kExt)
        path = path.left(path.size() - suffix.size()) + kExt;  // ".RESORA" -> ".resora"
    fi = QFileInfo(path);
    if (fi.fileName().compare(QLatin1Char('.') + kExt, Qt::CaseInsensitive) == 0) {
        c.error = QObject::tr("Enter a file name before the .%1 extension.").arg(kExt);
        return c;
    }
    for (const QString &src : sourceImages)
        if (samePath(src, path)) {
            c.error = QObject::tr("That is one of the original images. Choose a different .resora file.");
            return c;
        }
    if (!QFileInfo(fi.absolutePath()).isDir()) {
        c.error = QObject::tr("The folder %1 does not exist.").arg(QDir::toNativeSeparators(fi.absolutePath()));
        return c;
    }
    if (fi.exists() && !fi.isFile()) {
        c.error = QObject::tr("The destination exists and is not a regular file.");
        return c;
    }
    c.ok = true;
    c.path = path;
    c.exists = fi.exists();
    return c;
}

Estimate estimate(const dashboard::State &state)
{
    Estimate e;
    const dashboard::Plan p = dashboard::plan(state);
    if (!p.ok) {
        e.problems = p.error.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        return e;
    }
    for (const resora::ExportImage &im : p.images) {
        const QFileInfo fi(im.sourcePath);
        e.sources << im.sourcePath;
        ++e.images;
        if (!fi.isFile())
            e.problems << QObject::tr("Missing image: %1").arg(QDir::toNativeSeparators(im.sourcePath));
        else
            e.bytes += quint64(fi.size());
    }
    return e;
}

QString stageText(const resora::ExportProgress &p)
{
    switch (p.stage) {
    case resora::ExportProgress::Validating:
        return p.imageIndex >= 0 ? QObject::tr("Checking image %1…").arg(p.imageIndex + 1) : QObject::tr("Checking images…");
    case resora::ExportProgress::Writing: return QObject::tr("Writing dashboard…");
    case resora::ExportProgress::Verifying: return QObject::tr("Verifying file…");
    case resora::ExportProgress::Finalizing: return QObject::tr("Saving…");
    case resora::ExportProgress::Done: return QObject::tr("Done");
    }
    return {};
}

QString stageText(const resora::ImportProgress &p)
{
    switch (p.stage) {
    case resora::ImportProgress::Reading: return QObject::tr("Reading file…");
    case resora::ImportProgress::Validating: return QObject::tr("Checking images…");
    case resora::ImportProgress::Extracting: return QObject::tr("Unpacking images…");
    case resora::ImportProgress::Done: return QObject::tr("Done");
    }
    return {};
}

QString describe(const resora::ExportResult &r)
{
    QString text = r.message;
    for (const resora::ImageIssue &i : r.issues)
        text += QStringLiteral("\n• %1: %2").arg(QDir::toNativeSeparators(i.path), i.message);
    return text;
}

QHash<QString, QString> matchByName(const QList<resora::LinkStatus> &problems, const QString &folder)
{
    QHash<QString, QString> out;
    if (problems.isEmpty() || !QFileInfo(folder).isDir())
        return out;
    QSet<QString> wanted;
    for (const resora::LinkStatus &p : problems)
        wanted.insert(p.name.toLower());
    QHash<QString, QString> found;  // lower-case name -> first file
    QDirIterator it(folder, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString f = it.next();
        const QString key = it.fileName().toLower();
        if (wanted.contains(key) && !found.contains(key))
            found.insert(key, f);
    }
    for (const resora::LinkStatus &p : problems)
        if (found.contains(p.name.toLower()))
            out.insert(p.imageId, found.value(p.name.toLower()));
    return out;
}

bool allAssigned(const QList<resora::LinkStatus> &problems, const QHash<QString, QString> &chosen)
{
    if (problems.isEmpty())
        return false;
    for (const resora::LinkStatus &p : problems)
        if (chosen.value(p.imageId).isEmpty())
            return false;
    return true;
}

QString stateText(resora::LinkState s)
{
    switch (s) {
    case resora::LinkState::Ok: return QObject::tr("OK");
    case resora::LinkState::Missing: return QObject::tr("Missing");
    case resora::LinkState::Unreadable: return QObject::tr("Unreadable");
    case resora::LinkState::IdentityMismatch: return QObject::tr("Different file");
    case resora::LinkState::UnsafePath: return QObject::tr("Unsafe path");
    case resora::LinkState::Omitted: return QObject::tr("Removed");
    }
    return {};
}

}  // namespace resoraui
