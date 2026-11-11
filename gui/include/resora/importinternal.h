#pragma once
// Internal helpers of the import engine (not public API).
#include <QString>

#include "resora/resoraimport.h"
#include "resora/resorareader.h"

namespace resora::detail {

// One images-index record plus the untrusted values it declared.
struct Record
{
    ImageRef ref;
    QString bundledResource;   // Bundled
    // Linked identity
    QString path, relativePath;
    quint64 size = 0;
    QString sha256;
};

struct Parsed
{
    OpenedDocument doc;
    QList<Record> records;
    QString baseKind;      // "folder" | "document" | "" (none)
    QString baseHint;      // folder hint_path
};

// Monotonic progress with a per-stage window of the 0..1000 bar.
class Reporter
{
public:
    explicit Reporter(const ImportProgressFn &fn) : fn_(fn) {}
    // false => cancelled
    bool operator()(ImportProgress::Stage st, quint64 done, quint64 total);
private:
    ImportProgressFn fn_;
    int last_ = 0;
};

OpenResult failWith(OpenError e, const QString &msg);

// Reads and validates manifest, layout, charts, analytics, config and images-index. On failure
// `*err` is filled and false returned. The reader must already be open.
bool parseDocument(Reader &rd, const QString &containerPath, Parsed *out, OpenResult *err);

// ---- path safety / linked resolution ----
// A stored relative path is acceptable only if it is non-empty, uses '/', has no '..', '.', empty
// segments, drive letters, ':' , control characters or leading '/'.
bool isSafeRelativePath(const QString &rel);
// base + rel, cleaned; empty when it would leave `base`.
QString joinInside(const QString &base, const QString &rel);

// Validates every record (never stops at the first failure). Returns false when cancelled.
bool validateLinks(const Parsed &p, const OpenOptions &opts, const QString &docDir, Reporter &rep,
                   QList<LinkStatus> *out);

// ---- Bundled ----
// Marker file written first into every cache folder; its JSON content (app, kind, document id, owner pid,
// creation time) is what the stale-cache sweep and closeDocument() use to recognise a folder as ours.
constexpr const char *kCacheMarker = ".resora-cache";
QByteArray cacheMarkerContent(const QString &documentId);
// Extension of an extracted image file: only a known image extension taken from the (untrusted) display
// name, else ".img". Never an executable/script extension.
QString safeImageExtension(const QString &displayName);
// File stem for an extracted image: the validated resource id, prefixed when it would be a Windows device name.
QString safeStem(const QString &resourceId);

// Extracts every bundled image into a fresh marked folder under the cache root, verifying size and
// SHA-256. On any failure/cancel the folder is removed. Fills ref.path.
OpenResult extractBundled(Reader &rd, Parsed *p, const QString &cacheRoot, Reporter &rep);

}  // namespace resora::detail
