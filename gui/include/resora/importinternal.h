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

// ---- cache ownership / deletion (cachesweep.cpp) ----
// A cache folder may be removed only if: its name is doc-<32 hex>, it is not a symlink/junction/shortcut,
// it carries a valid RESORA marker, and EVERY direct entry is the marker or a regular (non-link) file whose
// name RESORA could have generated. Deletion is never recursive and never follows links. Anything else is
// preserved untouched (NotOwned).
enum class CleanupStatus { Removed, AlreadyGone, NotOwned, Failed };
struct CleanupResult
{
    CleanupStatus status = CleanupStatus::Removed;
    QString detail;
    bool done() const { return status == CleanupStatus::Removed || status == CleanupStatus::AlreadyGone; }
};
enum class CleanupMode
{
    Close,  // an open document of THIS process: marker pid must be ours (or a legacy marker)
    Sweep,  // stale folder; the caller has already decided the owner is dead and the folder old enough
    Abort   // folder created a moment ago by a failed open; the marker may not have been written yet
};
CleanupResult removeOwnedCacheDir(const QString &dir, CleanupMode mode);

// Windows: holds the directory open WITHOUT FILE_SHARE_DELETE and with FILE_FLAG_OPEN_REPARSE_POINT, so while
// it lives the directory cannot be renamed, deleted or replaced by a junction/symlink (nor can its parents be
// renamed), yet files inside can still be deleted. Used between "validated" and "deleted". Elsewhere a no-op
// that reports ok().
class DirLock
{
public:
    explicit DirLock(const QString &dir);
    ~DirLock() { release(); }
    DirLock(const DirLock &) = delete;
    DirLock &operator=(const DirLock &) = delete;
    bool ok() const { return ok_; }              // locked (or nothing to lock on this platform)
    bool isReparsePoint() const { return reparse_; }  // the opened object is a junction/symlink, not a plain directory
    void release();

private:
    void *h_ = nullptr;
    bool ok_ = false, reparse_ = false;
};

// ---- path safety / linked resolution ----
// Local-only policy: an absolute path must be a local one. UNC (\\server\share, //server/share), device
// (\\?\, \\.\) and any other double-separator prefix are rejected from the string alone, before any
// filesystem access.
bool isLocalAbsolutePath(const QString &path);
// Windows: true when drive letter `letter` is a mapped network drive, a SUBST of a network location, or a
// device that is not provably local. Uses QueryDosDeviceW (local object-manager lookup, no I/O to the
// volume or network), so disconnected mappings and dead servers answer immediately. Always false elsewhere.
// Limit: a local folder that is itself a symlink/junction to a network location is NOT detected.
bool isRemoteDriveLetter(QChar letter);
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
