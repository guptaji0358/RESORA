#include "importselection.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace {
const QStringList kExtensions{"jpg", "jpeg", "jpe", "jfif", "png", "webp", "bmp", "dib", "tif", "tiff",
                              "gif", "ico", "ppm", "pgm", "pbm", "tga", "jp2", "j2k", "pcx",
                              "avif", "heic", "heif", "jxl"};
constexpr int kListedNames = 5;

QString nativeAbsolute(const QString &p)
{
    return QDir::toNativeSeparators(QFileInfo(p).absoluteFilePath());
}

QString nameList(const QStringList &paths)
{
    QStringList names;
    for (int i = 0; i < paths.size() && i < kListedNames; ++i)
        names << QFileInfo(paths[i]).fileName();
    QString text = names.join(QStringLiteral(", "));
    if (paths.size() > kListedNames)
        text += QObject::tr(" and %1 more").arg(paths.size() - kListedNames);
    return text;
}
}  // namespace

QStringList ImportSelection::imageExtensions() { return kExtensions; }

bool ImportSelection::isSupportedExtension(const QString &path)
{
    return kExtensions.contains(QFileInfo(path).suffix().toLower());
}

QString ImportSelection::dialogFilter()
{
    QStringList globs;
    for (const QString &e : kExtensions)
        globs << QStringLiteral("*.") + e;
    return QObject::tr("Images (%1);;All files (*)").arg(globs.join(' '));
}

ImportSelection ImportSelection::fromFolder(const QString &path)
{
    ImportSelection s;
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        s.problem = QObject::tr("Nothing was selected.");
        return s;
    }
    const QFileInfo info(trimmed);
    if (!info.exists())
        s.problem = QObject::tr("This folder does not exist: %1").arg(QDir::toNativeSeparators(trimmed));
    else if (!info.isDir())
        s.problem = QObject::tr("This path is not a folder: %1").arg(QDir::toNativeSeparators(trimmed));
    else {
        s.kind = Folder;
        s.folder = nativeAbsolute(trimmed);
    }
    return s;
}

ImportSelection ImportSelection::fromFiles(const QStringList &paths)
{
    ImportSelection s;
    QSet<QString> seen;
    for (const QString &raw : paths) {
        if (raw.trimmed().isEmpty())
            continue;
        const QString full = nativeAbsolute(raw.trimmed());
        if (seen.contains(full.toLower())) {  // Windows paths are case-insensitive
            ++s.duplicates;
            continue;
        }
        seen.insert(full.toLower());
        if (!QFileInfo(full).isFile())
            s.missing << full;
        else if (!isSupportedExtension(full))
            s.unsupported << full;
        else
            s.files << full;
    }
    if (s.files.isEmpty() && s.missing.isEmpty() && s.unsupported.isEmpty())
        s.problem = QObject::tr("Nothing was selected.");
    else if (s.files.isEmpty())
        s.problem = QObject::tr("None of the selected files can be scanned (see below).");
    else
        s.kind = s.files.size() == 1 ? SingleImage : MultipleImages;
    return s;
}

ImportSelection ImportSelection::fromPaths(const QStringList &paths)
{
    if (paths.size() == 1 && QFileInfo(paths.first()).isDir())
        return fromFolder(paths.first());
    return fromFiles(paths);
}

QString ImportSelection::kindLabel() const
{
    switch (kind) {
    case Folder: return QObject::tr("FOLDER");
    case SingleImage: return QObject::tr("SINGLE IMAGE");
    case MultipleImages: return QObject::tr("MULTIPLE IMAGES");
    case None: break;
    }
    return QObject::tr("NOTHING IMPORTED");
}

QString ImportSelection::title() const
{
    switch (kind) {
    case Folder: {
        const QString name = QFileInfo(folder).fileName();
        return name.isEmpty() ? folder : name;  // drive roots have no name
    }
    case SingleImage: return QFileInfo(files.first()).fileName();
    case MultipleImages: return QObject::tr("%1 images").arg(files.size());
    case None: break;
    }
    return {};
}

QString ImportSelection::location() const
{
    if (kind == Folder)
        return QDir::toNativeSeparators(QFileInfo(folder).absolutePath());
    if (files.isEmpty())
        return {};
    const QString first = QFileInfo(files.first()).absolutePath();
    for (const QString &f : files)
        if (QFileInfo(f).absolutePath().compare(first, Qt::CaseInsensitive) != 0)
            return QObject::tr("several folders");
    return QDir::toNativeSeparators(first);
}

QStringList ImportSelection::notes() const
{
    QStringList out;
    if (!missing.isEmpty())
        out << QObject::tr("%1 selected path(s) are missing or are not files and were left out: %2")
                   .arg(missing.size()).arg(nameList(missing));
    if (!unsupported.isEmpty())
        out << QObject::tr("%1 file(s) do not have an image extension and were left out: %2")
                   .arg(unsupported.size()).arg(nameList(unsupported));
    if (duplicates > 0)
        out << QObject::tr("%1 duplicate selection(s) were counted once.").arg(duplicates);
    return out;
}
