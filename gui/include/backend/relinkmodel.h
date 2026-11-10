#pragma once
// Widget-free state of the relinking table (Phase 6): one row per linked image that failed validation, with
// the user's choices (replacement file / remove from dashboard). Qt Core only. Choices are PROPOSALS for one
// open operation: the import engine still validates every replacement (size + SHA-256) and blocks on every
// image that is neither verified nor explicitly omitted. Nothing here touches any file.
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include "resora/resoraimport.h"

namespace resoraui {

class RelinkModel
{
public:
    enum class Status {
        Missing,            // cannot be found / read at the recorded location
        WrongImage,         // a file exists but is not the recorded image (or a chosen replacement was rejected)
        ReplacementChosen,  // a replacement is selected but not verified yet (verified only by the next open)
        Resolved,           // verified by the engine (replacement accepted, or the original reappeared)
        Removed             // user removed the dashboard image association (this open only)
    };
    struct Row
    {
        QString imageId, name, recordedPath, message, replacement;
        QStringList roles;  // dashboard associations that use this image
        Status status = Status::Missing;
    };

    // First call lists every failing image at once; later calls (after a retry) fold in the engine's verdict:
    // chosen replacements still failing become WrongImage, the others Resolved. Removed rows stay removed.
    void update(const QList<resora::LinkStatus> &problems);
    void clear();

    bool assign(const QString &imageId, const QString &path);  // false: unknown id or Removed
    bool clearReplacement(const QString &imageId);             // back to unresolved; false: nothing to clear
    bool remove(const QString &imageId);                       // false: unknown id
    // Rows that still lack a verified image (for the file-name search helper).
    QList<resora::LinkStatus> unresolved() const;

    const QList<Row> &rows() const { return rows_; }
    const Row *find(const QString &imageId) const;
    // Replacements to send with the next open (chosen + already verified) and images to omit.
    QHash<QString, QString> relinks() const;
    QSet<QString> omitted() const;
    int count(Status s) const;
    // True when no row is Missing/WrongImage: only then may the next open be attempted.
    bool complete() const;

    static QString statusText(Status s);

private:
    Row *mut(const QString &imageId);
    QList<Row> rows_;
};

}  // namespace resoraui
