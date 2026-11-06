#include "locationrecommender.h"

#include <QDir>
#include <QFileInfo>

namespace LocationRecommender {

namespace {
bool hasSeparator(const QString &s)
{
    return s.contains(QLatin1Char('/')) || s.contains(QLatin1Char('\\'));
}

// "D:/Pics/rep" -> "D:/Pics"; "rep" -> "" ; "D:/Pics/" -> "D:/Pics"
QString directoryPart(const QString &typed)
{
    const QString u = QDir::fromNativeSeparators(typed.trimmed());
    const int cut = u.lastIndexOf(QLatin1Char('/'));
    return cut <= 0 ? (cut == 0 ? QStringLiteral("/") : QString()) : u.left(cut);
}

QString fileNamePart(const QString &typed, const QString &extension)
{
    const QString u = QDir::fromNativeSeparators(typed.trimmed());
    QString name = u.mid(u.lastIndexOf(QLatin1Char('/')) + 1);
    if (name.isEmpty())
        return QStringLiteral("image_resolution_report.") + extension;
    if (QFileInfo(name).suffix().compare(extension, Qt::CaseInsensitive) != 0)
        name += name.endsWith(QLatin1Char('.')) ? extension : QLatin1Char('.') + extension;
    return name;
}
}  // namespace

QString valueFor(const SavedLocation &location, const QString &typed, const Context &context)
{
    if (context.kind == Kind::Folder)
        return location.displayPath();
    return QDir::toNativeSeparators(QDir(location.path).filePath(fileNamePart(typed, context.extension)));
}

QList<Recommendation> recommend(const QList<SavedLocation> &all, const QString &typed, const Context &context)
{
    const QString needle = typed.trimmed();
    QList<Recommendation> out;
    for (const SavedLocation &l : all) {
        if (!SavedLocations::isUsable(SavedLocations::probe(l.path)))
            continue;
        bool match = true;
        if (!needle.isEmpty()) {
            if (context.kind == Kind::Folder) {
                const QString n = QDir::fromNativeSeparators(needle);
                match = l.name.contains(needle, Qt::CaseInsensitive) || l.path.contains(n, Qt::CaseInsensitive);
            } else if (hasSeparator(needle)) {
                const QString dir = directoryPart(needle);
                match = dir.isEmpty() || l.path.startsWith(dir, Qt::CaseInsensitive);
            }
        }
        if (match)
            out.append({l, valueFor(l, typed, context)});
    }
    return out;
}

}  // namespace LocationRecommender
