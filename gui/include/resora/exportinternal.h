#pragma once
// Internal (tests and the two export implementations): export entry points with injectable seams.
// Not part of the public API; production callers use exportBundled / exportLinked.
#include "resora/bundledexport.h"
#include "resora/exportseams.h"
#include "resora/linkedexport.h"

namespace resora::detail {

ExportResult exportBundledWith(const BundledExportInput &input, const QString &targetPath,
                               const ExportProgressFn &progress, const ExportSeams &seams);
LinkedExportResult exportLinkedWith(const LinkedExportInput &input, const QString &targetPath,
                                    const ExportProgressFn &progress, const ExportSeams &seams);

}  // namespace resora::detail
