#include "locations/savedlocation.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

QString SavedLocation::displayPath() const
{
    return QDir::toNativeSeparators(path);
}

namespace SavedLocations {

QString normalizePath(const QString &input)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty())
        return {};
    const QString unified = QDir::fromNativeSeparators(trimmed);
    if (!QDir::isAbsolutePath(unified))
        return {};
    return QDir::cleanPath(unified);
}

bool samePath(const QString &a, const QString &b)
{
#ifdef Q_OS_WIN
    return a.compare(b, Qt::CaseInsensitive) == 0;
#else
    return a == b;
#endif
}

LocationStatus probe(const QString &path)
{
    if (path.isEmpty() || !QDir::isAbsolutePath(path))
        return LocationStatus::Invalid;
    const QFileInfo info(path);
    if (info.isDir())
        return LocationStatus::Available;
    return info.exists() ? LocationStatus::NotDirectory : LocationStatus::Missing;
}

bool isUsable(LocationStatus s)
{
    return s == LocationStatus::Available;
}

QString statusText(LocationStatus s)
{
    switch (s) {
    case LocationStatus::Available: return QCoreApplication::translate("SavedLocations", "Available");
    case LocationStatus::Missing:
        return QCoreApplication::translate("SavedLocations", "Folder not found - moved, deleted or drive not connected");
    case LocationStatus::NotDirectory: return QCoreApplication::translate("SavedLocations", "This path is a file, not a folder");
    case LocationStatus::Invalid: return QCoreApplication::translate("SavedLocations", "Not a valid absolute path");
    }
    return {};
}

}  // namespace SavedLocations
