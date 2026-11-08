#pragma once
// Import / opening engine (Phase 4): opens a Bundled or Linked .resora into an in-memory document model
// without any UI. Qt Core only. Blocking: call from a worker thread. Rules and cache policy:
// docs/RESORA_FORMAT.md ("Import and Opening (Phase 4)").
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <functional>

#include "resora/resorafmt.h"

namespace resora {

enum class OpenError {
    None,
    InvalidInput,        // empty path, bad options
    Container,           // unreadable/corrupt/truncated/unsupported container (see containerError)
    BadManifest,         // manifest or images-index missing, malformed or inconsistent
    LinksBlocked,        // Linked: at least one image failed validation (see problems)
    InsufficientStorage,
    ExtractionFailed,    // Bundled: corrupt payload, I/O failure
    Cancelled
};

enum class LinkState { Ok, Missing, Unreadable, IdentityMismatch, UnsafePath };

// One linked image's validation outcome. Reported for EVERY image in `OpenResult::links`; the blocking
// ones are returned in `problems`.
struct LinkStatus
{
    int index = -1;
    QString imageId;
    QString name;
    LinkState state = LinkState::Missing;
    QString recordedPath;   // absolute path stored in the file
    QString relativePath;   // stored relative path (may be empty)
    QString resolvedPath;   // path that passed (Ok) or the last candidate that was tried
    bool viaRelative = false;
    bool viaRelink = false;
    QString message;
    bool ok() const { return state == LinkState::Ok; }
};

struct ImageRef
{
    int index = -1;
    QString imageId;
    QString name;
    QJsonObject metadata;
    QString thumbnailResource;  // empty => none
    QString path;               // readable file: extracted copy (Bundled) or validated original (Linked)
    quint64 size = 0;
    QString sha256;             // lowercase hex, verified
};

struct OpenedDocument
{
    QString containerPath;
    bool linked = false;
    QString documentId;
    QJsonObject manifest, layout, analytics, config;
    QJsonArray charts;
    QList<ImageRef> images;
    QString cacheDir;  // Bundled only: app-managed folder created for this open; remove with closeDocument()
};

struct ImportProgress
{
    enum Stage { Reading, Validating, Extracting, Done };
    Stage stage = Reading;
    quint64 done = 0, total = 0;
    int permille = 0;  // monotonic; 1000 only with Stage::Done
};
using ImportProgressFn = std::function<bool(const ImportProgress &)>;  // return false to cancel

struct OpenOptions
{
    // Bundled extraction root. Empty => QStandardPaths::CacheLocation + "/documents".
    QString cacheRoot;
    // Linked + folder-based relative paths: the image root chosen by the user. Empty => the recorded hint.
    QString imageRoot;
    // Linked: explicit replacement files by image_id. Validated like any other candidate.
    QHash<QString, QString> relinks;
};

struct OpenResult
{
    OpenError error = OpenError::None;
    Error containerError = Error::None;
    QString message;
    QList<LinkStatus> problems;  // non-Ok links when error == LinksBlocked
    QList<LinkStatus> links;     // all links (Linked mode), also on success
    OpenedDocument document;     // valid ONLY when ok(); never partially filled on failure
    bool ok() const { return error == OpenError::None; }
};

OpenResult openResora(const QString &path, const OpenOptions &opts = {}, const ImportProgressFn &progress = {});

// Convenience: adds `replacements` (image_id -> file) to `opts.relinks` and reopens. EVERY link is
// revalidated from scratch; the document opens only if all pass.
OpenResult relinkAndOpen(const QString &path, OpenOptions opts, const QHash<QString, QString> &replacements,
                         const ImportProgressFn &progress = {});

// Removes the extracted cache folder of a Bundled document, only if it is a marked folder created by
// openResora directly under its cache root. Never touches anything else. Returns true when nothing is left.
bool closeDocument(OpenedDocument *doc);

// Thumbnail bytes of image `index` (empty on none/failure); reads the container on demand.
QByteArray readThumbnail(const OpenedDocument &doc, int index);

QString defaultCacheRoot();

}  // namespace resora
