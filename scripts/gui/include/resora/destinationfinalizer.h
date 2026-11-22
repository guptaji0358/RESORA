#pragma once
// Crash-safe destination finalization for exports. Qt Core only (Win32 on Windows).
//
// Moves a fully written and verified temporary file (same folder => same volume) onto its final
// path without ever deleting or truncating the previous destination first:
//   * Windows, destination absent : MoveFileExW(tmp, dst, 0)    - same-volume rename, refuses to overwrite.
//   * Windows, destination exists : ReplaceFileW(dst, tmp, backup, IGNORE_MERGE_ERRORS); the old content is
//     kept under `backup` and removed only after ReplaceFileW reported success.
//   * Other platforms             : rename(2), which atomically replaces.
//
// Failure handling (Windows) never trusts an error code alone. After a failed ReplaceFileW the real
// state of {target, backup, tmp} is observed:
//   * target present, backup absent, tmp present -> nothing changed (documented for 1175, 1176 with a
//     backup name, and "other errors"); the caller may delete tmp.
//   * target absent, backup present (documented for 1177: the replaced file keeps a different name) ->
//     the backup is moved back with a non-overwriting move; if that succeeds the state is clean.
//   * anything else -> state is UNCERTAIN: nothing is deleted or moved, `keepTemp` is set, and the
//     message names every surviving file so the user can recover manually.
// Details and the split between documented guarantees and assumptions: docs/RESORA_FORMAT.md.
#include <QString>
#include <functional>

namespace resora {

enum class FinalizeStatus {
    Ok,
    TempMissing,        // the temporary file is gone or unreadable
    DestinationLocked,  // sharing violation / access denied / ReplaceFileW 1175-1177, state clean
    DestinationInvalid, // destination is a directory
    Failed,             // any other OS failure, state clean
    NeedsAttention      // file state uncertain after a failed replace; every surviving file was preserved
};

struct FinalizeResult
{
    FinalizeStatus status = FinalizeStatus::Ok;
    QString message;            // user-presentable when status != Ok
    unsigned long osError = 0;  // GetLastError()/errno of the last attempt, 0 when Ok
    bool keepTemp = false;      // true => the caller MUST NOT delete the temporary file (it may be the only valid copy)
    bool ok() const { return status == FinalizeStatus::Ok; }
};

// Unique "<target>.<8 hex>.tmp" sibling name (temporary or backup files).
QString tempSiblingName(const QString &target);

// Finalizes `tmp` onto `target`. On success `tmp` no longer exists. On failure the previous `target`
// (if any) is intact under its own name, or - only with `keepTemp` - preserved under a reported name.
// `tmp` is left for the caller to remove unless `keepTemp` is set. Never throws.
FinalizeResult finalizeDestination(const QString &tmp, const QString &target);

namespace detail {
// Seam for tests: the three OS operations used by the Windows algorithm (error code 0 = success).
// Outcome of pushing the finished temporary file to disk (before anything is moved).
struct FlushResult
{
    unsigned long error = 0;  // 0 = flushed
    bool openFailed = false;  // true: the file could not be opened for flushing; false: the flush itself failed
};

struct FinalizeOps
{
    // Open + flush the finished temp file (FlushFileBuffers on Windows). Empty = skip. Runs before any
    // destination or backup file is touched, so a failure is always a clean failure.
    std::function<FlushResult(const QString &tmp)> flush;
    std::function<unsigned long(const QString &tmp, const QString &dst)> moveNew;  // MoveFileExW, no overwrite
    std::function<unsigned long(const QString &dst, const QString &tmp, const QString &backup)> replace;  // ReplaceFileW
    std::function<unsigned long(const QString &from, const QString &to)> restore;  // MoveFileExW, no overwrite
    int attempts = 6;  // retries for transient sharing errors; 50*n ms backoff
};
// The platform-independent Windows algorithm (also compiled on other platforms so it can be tested).
FinalizeResult finalizeWith(const FinalizeOps &ops, const QString &tmp, const QString &target);
}  // namespace detail

}  // namespace resora
