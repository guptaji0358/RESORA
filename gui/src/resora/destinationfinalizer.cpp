#include "resora/destinationfinalizer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QUuid>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <cerrno>
#include <cstdio>
#endif

namespace resora {

QString tempSiblingName(const QString &target)
{
    return target + QStringLiteral(".") + QUuid::createUuid().toString(QUuid::Id128).left(8) + QStringLiteral(".tmp");
}

namespace {

// Win32 codes as plain numbers so the algorithm compiles (and is testable) on every platform.
constexpr unsigned long kFileNotFound = 2, kPathNotFound = 3, kAccessDenied = 5, kSharingViolation = 32,
                        kLockViolation = 33, kFileExists = 80, kAlreadyExists = 183,
                        kUnableToRemoveReplaced = 1175, kUnableToMoveReplacement = 1176,
                        kUnableToMoveReplacement2 = 1177;

FinalizeResult failure(FinalizeStatus s, const QString &msg, unsigned long err, bool keepTemp = false)
{
    FinalizeResult r;
    r.status = s;
    r.message = msg;
    r.osError = err;
    r.keepTemp = keepTemp;
    return r;
}

bool isTransient(unsigned long e) { return e == kSharingViolation || e == kLockViolation || e == kAccessDenied; }

QString native(const QString &p) { return QDir::toNativeSeparators(p); }

// Clean failure (state verified): the previous file is intact at the destination.
FinalizeResult describe(unsigned long e, bool hadPrevious)
{
    const QString kept = hadPrevious ? QStringLiteral("; the previous file was kept") : QString();
    switch (e) {
    case kSharingViolation:
    case kLockViolation:
        return failure(FinalizeStatus::DestinationLocked,
                       QStringLiteral("The destination is in use by another program (close it and try again)") + kept, e);
    case kAccessDenied:
        return failure(FinalizeStatus::DestinationLocked,
                       QStringLiteral("Access to the destination was denied (read-only, locked, or no permission)") + kept, e);
    case kFileNotFound:
    case kPathNotFound:
        return failure(FinalizeStatus::Failed, QStringLiteral("The destination folder or temporary file disappeared") + kept, e);
    case kUnableToRemoveReplaced:
    case kUnableToMoveReplacement:
    case kUnableToMoveReplacement2:
        return failure(FinalizeStatus::DestinationLocked,
                       QStringLiteral("Cannot replace the existing file (is it open elsewhere?)") + kept, e);
    default:
        return failure(FinalizeStatus::Failed,
                       QStringLiteral("Cannot move the finished file into place (Windows error %1)").arg(e) + kept, e);
    }
}

// After a failed ReplaceFileW: observe the real state instead of trusting the code (docs: 1177 leaves the
// replaced file under a different name). Preserves every file whose role is not provable.
FinalizeResult settleAfterFailedReplace(const detail::FinalizeOps &ops, const QString &tmp, const QString &target,
                                        const QString &backup, unsigned long err)
{
    const bool tgt = QFileInfo::exists(target), bak = QFileInfo::exists(backup), tm = QFileInfo::exists(tmp);

    if (tgt && !bak && tm)
        return describe(err, true);  // nothing changed: documented for 1175, 1176 (+backup name) and other errors

    if (!tgt && bak && tm) {  // documented 1177 shape: old content lives only under the backup name
        const unsigned long r = ops.restore(backup, target);  // non-overwriting move
        if (r == 0 && QFileInfo::exists(target) && !QFileInfo::exists(backup))
            return describe(err, true);  // old destination restored; tmp still holds the verified new export
        return failure(FinalizeStatus::NeedsAttention,
                       QStringLiteral("The destination could not be replaced and the previous file could not be restored. "
                                      "Nothing was deleted. Previous version: %1. New export: %2.")
                           .arg(native(backup), native(tmp)),
                       err, true);
    }

    QStringList have;  // anything else: report what survives, delete nothing
    if (tgt) have << QStringLiteral("destination: ") + native(target);
    if (bak) have << QStringLiteral("previous version: ") + native(backup);
    if (tm) have << QStringLiteral("new export: ") + native(tmp);
    return failure(FinalizeStatus::NeedsAttention,
                   QStringLiteral("The destination was left in an unexpected state after a failed replace (Windows error %1). "
                                  "Nothing was deleted. Remaining files - %2.")
                       .arg(err)
                       .arg(have.isEmpty() ? QStringLiteral("none found") : have.join(QStringLiteral("; "))),
                   err, true);
}

}  // namespace

namespace detail {

FinalizeResult finalizeWith(const FinalizeOps &ops, const QString &tmp, const QString &target)
{
    unsigned long last = 0;
    bool hadPrevious = false;
    for (int attempt = 1; attempt <= ops.attempts; ++attempt) {
        last = ops.moveNew(tmp, target);  // new destination: refuses to overwrite (closes the exists/rename race)
        if (last == 0)
            return {};
        if (last == kAlreadyExists || last == kFileExists) {
            hadPrevious = true;
            const QString backup = tempSiblingName(target);
            last = ops.replace(target, tmp, backup);
            if (last == 0) {
                QFile::remove(backup);  // ReplaceFileW succeeded => the backup is the OLD content; best effort
                return {};
            }
            const FinalizeResult settled = settleAfterFailedReplace(ops, tmp, target, backup, last);
            if (settled.status == FinalizeStatus::NeedsAttention)
                return settled;  // uncertain state: never retry, never touch anything
            if (last == kUnableToRemoveReplaced || last == kUnableToMoveReplacement || last == kUnableToMoveReplacement2)
                return settled;  // documented non-transient replace failures
        }
        if (!isTransient(last) || attempt == ops.attempts)
            break;
        QThread::msleep(50u * unsigned(attempt));
    }
    return describe(last, hadPrevious);
}

}  // namespace detail

FinalizeResult finalizeDestination(const QString &tmp, const QString &target)
{
    if (!QFileInfo(tmp).isFile())
        return failure(FinalizeStatus::TempMissing, QStringLiteral("The finished temporary file is missing"), 0);
    if (QFileInfo(target).isDir())
        return failure(FinalizeStatus::DestinationInvalid, QStringLiteral("The destination is a folder"), 0);

#ifdef Q_OS_WIN
    const auto wide = [](const QString &p) { return native(p).toStdWString(); };
    {  // push the finished temp file to disk before it becomes the destination. Nothing has moved yet, so
       // a failure here is a clean failure: the previous destination and the temp file are untouched.
        const HANDLE h = CreateFileW(wide(tmp).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return failure(FinalizeStatus::Failed,
                           QStringLiteral("Could not open the new file to flush it to disk (Windows error %1); the previous file was kept")
                               .arg(GetLastError()),
                           GetLastError());
        const bool flushed = FlushFileBuffers(h) != 0;
        const DWORD err = flushed ? 0 : GetLastError();
        CloseHandle(h);
        if (!flushed)
            return failure(FinalizeStatus::Failed,
                           QStringLiteral("Could not flush the new file to disk (Windows error %1); the previous file was kept")
                               .arg(err),
                           err);
    }
    detail::FinalizeOps ops;
    ops.moveNew = [&](const QString &from, const QString &to) -> unsigned long {
        return MoveFileExW(wide(from).c_str(), wide(to).c_str(), 0) ? 0 : GetLastError();
    };
    ops.restore = ops.moveNew;
    ops.replace = [&](const QString &dst, const QString &src, const QString &bak) -> unsigned long {
        return ReplaceFileW(wide(dst).c_str(), wide(src).c_str(), wide(bak).c_str(), REPLACEFILE_IGNORE_MERGE_ERRORS,
                            nullptr, nullptr)
                   ? 0
                   : GetLastError();
    };
    return detail::finalizeWith(ops, tmp, target);
#else
    if (std::rename(QFile::encodeName(tmp).constData(), QFile::encodeName(target).constData()) == 0)
        return {};
    const int e = errno;
    const bool locked = e == EACCES || e == EPERM || e == EBUSY;
    return failure(locked ? FinalizeStatus::DestinationLocked : FinalizeStatus::Failed,
                   QStringLiteral("Cannot move the finished file into place; the previous file was kept"), unsigned(e));
#endif
}

}  // namespace resora
