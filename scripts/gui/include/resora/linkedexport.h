#pragma once
// Linked Data Export (Phase 3): the same dashboard resources as Bundled Export, but the ORIGINAL image
// bytes are NOT packaged. Each image is recorded as an external reference with an identity record
// (absolute path, optional relative path, size, mtime, SHA-256, dimensions). Every source is validated
// and hashed before the container is finalized; broken references are never exported silently.
// Qt Core only. Path policy and schema: docs/RESORA_FORMAT.md ("Linked Data Export").
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include "resora/bundledexport.h"  // shared ExportError, ImageIssue, ExportProgress(Fn), id helpers

namespace resora {

// One external image. The file is opened read-only and never modified; its bytes are hashed but not stored.
struct LinkedImage
{
    QString sourcePath;      // ABSOLUTE path of the original file (required)
    QString name;            // display file name; empty => taken from sourcePath
    QJsonObject metadata;    // docs/schemas/resora/metadata.schema.json; width/height/format are copied into the identity record
    QByteArray thumbnail;    // optional encoded thumbnail (PNG/JPEG); empty => none
    QString thumbnailFormat; // informational
    // Optional identity recorded at analysis time. When set, a source that no longer matches is
    // reported as SourceIssue::Changed instead of being exported with the new identity.
    qint64 expectedSize = -1;   // -1 => not checked
    QString expectedSha256;     // lowercase hex; empty => not checked
};

// How relative_path values are computed and later resolved (docs/RESORA_FORMAT.md).
enum class RelativeBase {
    None,      // no relative paths are recorded
    Folder,    // relative to LinkedExportInput::baseDir (an explicit image root, e.g. the scanned folder)
    Document   // relative to the folder that will contain the .resora file (only when the user chooses it)
};

struct LinkedExportInput
{
    QString appVersion;
    QJsonObject source;
    QJsonObject layout;
    QJsonArray charts;
    QJsonObject analytics;
    int engineProtocol = -1;
    QJsonObject config;
    QList<LinkedImage> images;
    RelativeBase relativeBase = RelativeBase::None;
    QString baseDir;  // required (absolute, existing folder) when relativeBase == Folder
};

struct DuplicateName
{
    QString name;
    QList<int> indexes;  // positions in LinkedExportInput::images
    bool samePath = false;  // true => the very same file is listed more than once; false => different files share the name
};

struct LinkedExportResult : ExportResult  // imageBytes = total bytes of the referenced files (not stored)
{
    QList<DuplicateName> duplicateNames;  // warnings; export still succeeds, records are flagged name_collision
    int relativeCount = 0;                // references that received a relative_path
};

// Exports to `targetPath` atomically (same temp -> verify -> finalizeDestination pipeline as Bundled).
// On any failure or cancel the target is untouched and no temporary file remains. The target must not be
// one of the source images. Blocking; run on a worker thread.
LinkedExportResult exportLinked(const LinkedExportInput &input, const QString &targetPath,
                                const ExportProgressFn &progress = {});

}  // namespace resora
