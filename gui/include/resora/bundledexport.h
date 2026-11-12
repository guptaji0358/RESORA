#pragma once
// Bundled Export (Phase 2): packages a dashboard definition plus the ORIGINAL image bytes into one
// .resora container using the Phase 1 Writer/Reader. Qt Core only; knows nothing about widgets or
// dashboard presentation. Callers hand in plain data and a destination path. Resource mapping and
// behaviour: docs/RESORA_FORMAT.md ("Bundled Export").
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <functional>


namespace resora {

// One image to bundle. The source file is opened read-only and never modified.
struct ExportImage
{
    QString sourcePath;     // absolute path of the original file (required)
    QString name;           // display file name (metadata only); empty => taken from sourcePath
    QJsonObject metadata;   // docs/schemas/resora/metadata.schema.json (opaque here)
    QByteArray thumbnail;   // optional encoded thumbnail (PNG/JPEG); empty => none
    QString thumbnailFormat;  // informational ("png", "jpeg"); optional
};

struct BundledExportInput
{
    QString appVersion;       // recorded in the manifest
    QJsonObject source;       // optional {"kind": "folder"|"files", "root": ...}; empty => null
    QJsonObject layout;       // dashboard layout/view state (schema_version added if missing)
    QJsonArray charts;        // chart definitions
    QJsonObject analytics;    // engine analytics object, stored unmodified under "data"
    int engineProtocol = -1;  // backend protocol version, -1 => null
    QJsonObject config;       // analysis options / dashboard settings
    QList<ExportImage> images;
};

enum class ExportError {
    None, InvalidInput, SourceProblem, Cancelled, WriteFailed, VerifyFailed, FinalizeFailed,
    InsufficientStorage  // Phase 7 (appended): destination volume lacks room; nothing was written to the target
};

enum class SourceIssue { Missing, Unreadable, Changed };

struct ImageIssue
{
    int index = -1;  // index into BundledExportInput::images
    QString path;
    SourceIssue kind = SourceIssue::Missing;
    QString message;
};

struct ExportResult
{
    ExportError error = ExportError::None;
    QString message;
    QList<ImageIssue> issues;  // filled for ExportError::SourceProblem
    QString documentId;        // 32 hex chars, also stored in the manifest
    int imageCount = 0;
    quint64 imageBytes = 0;    // total original bytes packaged
    quint64 fileBytes = 0;     // size of the final .resora
    bool ok() const { return error == ExportError::None; }
};

struct ExportProgress
{
    enum Stage { Validating, Writing, Verifying, Finalizing, Done };
    Stage stage = Validating;
    quint64 done = 0;   // bytes within the stage (0/0 for Finalizing and Done)
    quint64 total = 0;
    int imageIndex = -1;  // image being hashed during Validating (and Linked Export's final rehash), otherwise -1
    int permille = 0;     // overall progress 0..1000; never decreases; 1000 only with Stage::Done (after the swap)
};
// Return false to cancel. Called from the exporting thread (never the GUI thread by itself).
using ExportProgressFn = std::function<bool(const ExportProgress &)>;

// Exports to `targetPath` atomically: written to a temporary sibling, verified by reopening it with
// the Phase 1 Reader, then swapped into place. On any failure or cancel the target is untouched
// (an existing file survives) and no temporary file remains. The target must not be one of the
// source images. Blocking; run it on a worker thread from the UI.
ExportResult exportBundled(const BundledExportInput &input, const QString &targetPath,
                           const ExportProgressFn &progress = {});

// Resource id helpers; `index` is the 0-based position in BundledExportInput::images and the
// number is index+1: "image-0001", "thumb-0001"; image ids "img-0001".
QString imageResourceId(int index);
QString thumbResourceId(int index);
QString imageId(int index);

}  // namespace resora
