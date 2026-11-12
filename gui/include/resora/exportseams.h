#pragma once
// Fault-injection seams for the export pipeline (Phase 7). Production callers never set them: every
// default is the real behaviour. Tests use them to force storage failures deterministically instead of
// filling a disk. Qt Core only.
#include <QString>
#include <functional>

#include "resora/destinationfinalizer.h"
#include "resora/resorawriter.h"

namespace resora::detail {

inline constexpr quint64 kExportReserveBytes = quint64(16) << 20;  // safety reserve on top of the payload

// Sets the simulated device capacity of a Writer's temporary file: once a write would exceed `bytes` it
// fails with Error::OutOfSpace and the target is untouched. 0 = unlimited. Internal; not public Writer API.
struct WriterLimit
{
    static void set(Writer &w, quint64 bytes) { w.limit_ = bytes; }
};

struct ExportSeams
{
    // Free bytes on the volume holding `dir`; a negative value means "unknown" and skips the check.
    // Default: QStorageInfo.
    std::function<qint64(const QString &dir)> availableBytes;
    // Reserve required on top of the container size (default kExportReserveBytes).
    quint64 reserveBytes = kExportReserveBytes;
    // Simulated device capacity for the temporary file (0 = unlimited).
    quint64 writeLimit = 0;
    // Called once the temporary file is complete and before it is verified (tests may damage it).
    std::function<void(const QString &tmp)> afterWrite;
    // Destination swap. Default: finalizeDestination.
    std::function<FinalizeResult(const QString &tmp, const QString &target)> finalize;
};

// True when `needed` bytes fit on the volume of `dir` (or the free space is unknown). `available` receives
// the measured value (-1 when unknown).
bool hasRoomFor(const QString &dir, quint64 needed, const ExportSeams &seams, qint64 *available);

}  // namespace resora::detail
