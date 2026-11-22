#pragma once
// Saved Locations persistence + validation. Owns the list, enforces unique names / paths, and keeps
// a small versioned JSON file (QSaveFile: atomic) in the per-user config directory.
//
// Safety rules: entries are never deleted because their folder is missing; an entry the file
// contains but this version cannot read is preserved untouched on every save; a corrupt file is
// moved aside (never overwritten); a file written by a newer version, or one that cannot be read,
// puts the store in read-only mode so nothing is clobbered.
#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QStringList>

#include "locations/savedlocation.h"

class LocationStore : public QObject
{
    Q_OBJECT
public:
    enum class Error {
        None, EmptyName, NameTooLong, DuplicateName, EmptyPath, NotAbsolute, PathTooLong,
        NotADirectory, DuplicatePath, NotFound, ReadOnly, SaveFailed,
    };
    struct Result
    {
        Error error = Error::None;
        QString message;
        bool ok() const { return error == Error::None; }
    };

    static constexpr int kFormatVersion = 1;

    explicit LocationStore(const QString &filePath, QObject *parent = nullptr);

    // %APPDATA%/<app>/saved_locations.json (QStandardPaths::AppConfigLocation); never hardcoded.
    static QString defaultFilePath();

    QString filePath() const { return filePath_; }
    const QList<SavedLocation> &locations() const { return items_; }
    SavedLocation find(const QString &id) const;  // empty id when unknown
    QStringList warnings() const { return warnings_; }  // problems found while loading
    bool isReadOnly() const { return readOnly_; }

    Result add(const QString &name, const QString &path, QString *newId = nullptr);
    Result rename(const QString &id, const QString &name);
    Result setPath(const QString &id, const QString &path);
    Result remove(const QString &id);
    void reload();  // re-read the file (discards in-memory state)

signals:
    void changed();

private:
    Result validateName(const QString &name, const QString &exceptId, QString *clean) const;
    Result validatePath(const QString &path, const QString &exceptId, QString *clean) const;
    Result commit(const QList<SavedLocation> &previous);  // save, or roll back in memory
    Result save() const;
    void load();
    int indexOf(const QString &id) const;

    QString filePath_;
    QList<SavedLocation> items_;
    QJsonArray unreadable_;  // raw entries we could not parse; written back verbatim
    QStringList warnings_;
    bool readOnly_ = false;
};
