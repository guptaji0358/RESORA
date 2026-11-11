#pragma once
// Builds a .resora container. Resources are opaque bytes (from memory or streamed from a file at
// write time). write() is atomic: data goes to a temporary file and replaces the target only on
// success (QSaveFile), so a failed write never leaves a partial file under the final name.
#include <QList>
#include <QSet>
#include <functional>
#include <QString>

#include "resora/resorafmt.h"

namespace resora {

class Writer
{
public:
    explicit Writer(quint32 flags = 0) : flags_(flags) {}

    Result addBytes(const QString &id, quint32 type, const QByteArray &data);
    // The file is opened read-only and streamed in chunks when write() runs.
    Result addFile(const QString &id, quint32 type, const QString &sourcePath);

    int count() const { return int(items_.size()); }
    // Optional progress callback (bytesDone, bytesTotal); return false to cancel (Error::Cancelled,
    // target untouched). Additive Phase 2 extension; behaviour without a callback is unchanged.
    using ProgressFn = std::function<bool(quint64, quint64)>;
    Result write(const QString &targetPath, const ProgressFn &progress = {}) const;

private:
    struct Item
    {
        QString id;
        quint32 type = 0;
        QByteArray bytes;
        QString sourcePath;  // non-empty => stream from file
    };
    Result checkId(const QString &id) const;

    quint32 flags_ = 0;
    QList<Item> items_;
    QSet<QString> ids_;  // O(1) duplicate check (60000-image exports)
};

}  // namespace resora
