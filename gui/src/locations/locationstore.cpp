#include "locations/locationstore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace {
QString tr(const char *s) { return QCoreApplication::translate("LocationStore", s); }
}  // namespace

LocationStore::LocationStore(const QString &filePath, QObject *parent) : QObject(parent), filePath_(filePath)
{
    load();
}

QString LocationStore::defaultFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty())
        dir = QDir::home().filePath(QStringLiteral(".resora"));
    return QDir(dir).filePath(QStringLiteral("saved_locations.json"));
}

int LocationStore::indexOf(const QString &id) const
{
    for (int i = 0; i < items_.size(); ++i)
        if (items_[i].id == id)
            return i;
    return -1;
}

SavedLocation LocationStore::find(const QString &id) const
{
    const int i = indexOf(id);
    return i < 0 ? SavedLocation{} : items_[i];
}

// ---------------------------------------------------------------- loading

void LocationStore::reload()
{
    load();
    emit changed();
}

void LocationStore::load()
{
    items_.clear();
    unreadable_ = QJsonArray();
    warnings_.clear();
    readOnly_ = false;

    QFile file(filePath_);
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly)) {
        warnings_ << tr("The saved locations file could not be read (%1). Changes are disabled so it is not overwritten.")
                         .arg(file.errorString());
        readOnly_ = true;
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        const QString backup = filePath_ + QStringLiteral(".corrupt-")
                               + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        if (QFile::rename(filePath_, backup))
            warnings_ << tr("The saved locations file was damaged. It was kept as %1 and a new list was started.")
                             .arg(QDir::toNativeSeparators(backup));
        else {
            warnings_ << tr("The saved locations file is damaged and could not be moved aside. Changes are disabled.");
            readOnly_ = true;
        }
        return;
    }

    const QJsonObject root = doc.object();
    const int version = root.value(QStringLiteral("version")).toInt(0);
    if (version > kFormatVersion) {
        warnings_ << tr("The saved locations file was written by a newer RESORA (format %1). It is shown read-only.")
                         .arg(version);
        readOnly_ = true;
    }
    int skipped = 0;
    for (const QJsonValue &v : root.value(QStringLiteral("locations")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString name = o.value(QStringLiteral("name")).toString().trimmed();
        const QString path = SavedLocations::normalizePath(o.value(QStringLiteral("path")).toString());
        if (!v.isObject() || name.isEmpty() || path.isEmpty()) {
            unreadable_.append(v);  // preserved verbatim on save, never dropped
            ++skipped;
            continue;
        }
        QString id = o.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || indexOf(id) >= 0)
            id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        items_.append({id, name, path});
    }
    if (skipped > 0)
        warnings_ << tr("%1 saved location(s) in the file could not be read. They are kept in the file untouched.").arg(skipped);
}

// ---------------------------------------------------------------- saving

LocationStore::Result LocationStore::save() const
{
    QJsonArray array;
    for (const SavedLocation &l : items_)
        array.append(QJsonObject{{"id", l.id}, {"name", l.name}, {"path", l.path}});
    for (const QJsonValue &raw : unreadable_)
        array.append(raw);
    const QJsonObject root{{"version", kFormatVersion}, {"locations", array}};

    const QFileInfo info(filePath_);
    if (!QDir().mkpath(info.absolutePath()))
        return {Error::SaveFailed, tr("Could not create the settings folder %1.").arg(QDir::toNativeSeparators(info.absolutePath()))};
    QSaveFile out(filePath_);
    if (!out.open(QIODevice::WriteOnly))
        return {Error::SaveFailed, tr("Could not write the saved locations file: %1").arg(out.errorString())};
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!out.commit())
        return {Error::SaveFailed, tr("Could not write the saved locations file: %1").arg(out.errorString())};
    return {};
}

LocationStore::Result LocationStore::commit(const QList<SavedLocation> &previous)
{
    const Result r = save();
    if (!r.ok()) {
        items_ = previous;  // memory must never claim more than the disk holds
        return r;
    }
    emit changed();
    return r;
}

// ---------------------------------------------------------------- validation

LocationStore::Result LocationStore::validateName(const QString &name, const QString &exceptId, QString *clean) const
{
    const QString n = name.simplified();
    if (n.isEmpty())
        return {Error::EmptyName, tr("Give the location a name.")};
    if (n.size() > SavedLocations::kMaxNameLength)
        return {Error::NameTooLong, tr("Names can be at most %1 characters.").arg(SavedLocations::kMaxNameLength)};
    for (const SavedLocation &l : items_)
        if (l.id != exceptId && l.name.compare(n, Qt::CaseInsensitive) == 0)
            return {Error::DuplicateName, tr("A location named \"%1\" already exists.").arg(l.name)};
    *clean = n;
    return {};
}

LocationStore::Result LocationStore::validatePath(const QString &path, const QString &exceptId, QString *clean) const
{
    if (path.trimmed().isEmpty())
        return {Error::EmptyPath, tr("Choose a folder.")};
    if (path.size() > SavedLocations::kMaxPathLength)
        return {Error::PathTooLong, tr("That path is too long.")};
    const QString p = SavedLocations::normalizePath(path);
    if (p.isEmpty())
        return {Error::NotAbsolute, tr("Enter a full folder path such as D:\\Pictures (relative paths are not accepted).")};
    if (SavedLocations::probe(p) == LocationStatus::NotDirectory)
        return {Error::NotADirectory, tr("That path is a file, not a folder.")};
    for (const SavedLocation &l : items_)
        if (l.id != exceptId && SavedLocations::samePath(l.path, p))
            return {Error::DuplicatePath, tr("That folder is already saved as \"%1\".").arg(l.name)};
    *clean = p;
    return {};
}

// ---------------------------------------------------------------- mutations

LocationStore::Result LocationStore::add(const QString &name, const QString &path, QString *newId)
{
    if (readOnly_)
        return {Error::ReadOnly, tr("Saved locations are read-only (see the warning above).")};
    QString n, p;
    if (Result r = validateName(name, QString(), &n); !r.ok())
        return r;
    if (Result r = validatePath(path, QString(), &p); !r.ok())
        return r;
    const QList<SavedLocation> previous = items_;
    SavedLocation item{QUuid::createUuid().toString(QUuid::WithoutBraces), n, p};
    items_.append(item);
    if (newId)
        *newId = item.id;
    return commit(previous);
}

LocationStore::Result LocationStore::rename(const QString &id, const QString &name)
{
    if (readOnly_)
        return {Error::ReadOnly, tr("Saved locations are read-only (see the warning above).")};
    const int i = indexOf(id);
    if (i < 0)
        return {Error::NotFound, tr("That location no longer exists.")};
    QString n;
    if (Result r = validateName(name, id, &n); !r.ok())
        return r;
    const QList<SavedLocation> previous = items_;
    items_[i].name = n;
    return commit(previous);
}

LocationStore::Result LocationStore::setPath(const QString &id, const QString &path)
{
    if (readOnly_)
        return {Error::ReadOnly, tr("Saved locations are read-only (see the warning above).")};
    const int i = indexOf(id);
    if (i < 0)
        return {Error::NotFound, tr("That location no longer exists.")};
    QString p;
    if (Result r = validatePath(path, id, &p); !r.ok())
        return r;
    const QList<SavedLocation> previous = items_;
    items_[i].path = p;
    return commit(previous);
}

LocationStore::Result LocationStore::remove(const QString &id)
{
    if (readOnly_)
        return {Error::ReadOnly, tr("Saved locations are read-only (see the warning above).")};
    const int i = indexOf(id);
    if (i < 0)
        return {Error::NotFound, tr("That location no longer exists.")};
    const QList<SavedLocation> previous = items_;
    items_.removeAt(i);
    return commit(previous);
}
