#pragma once
// Export/Import UI logic (Phase 6), widget-free (Qt Core): destination validation, size estimate,
// progress wording and relink helpers. Dialogs and jobs call these; none of it touches the file format.
// Rules: docs/RESORA_FORMAT.md ("Export/Import UI (Phase 6)").
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include "backend/dashboardstate.h"

namespace resoraui {

enum class Mode { Bundled, Linked };

struct DestinationCheck
{
    bool ok = false;
    QString error;   // shown to the user when !ok
    QString path;    // normalized absolute path with the ".resora" extension (valid when ok)
    bool exists = false;  // an existing file would be replaced: the user must confirm first
};

// `sourceImages`: the original image files of the dashboard (never an allowed destination).
DestinationCheck checkDestination(const QString &input, const QStringList &sourceImages);

struct Estimate
{
    int images = 0;
    quint64 bytes = 0;   // total size of the original images (what Bundled would contain)
    QStringList problems;  // missing/unreadable originals or an unexportable dashboard
    QStringList sources;   // original image paths (for destination checks)
};
// Stat-only (no hashing, no reads); cheap enough for the GUI thread.
Estimate estimate(const dashboard::State &state);

QString stageText(const resora::ExportProgress &p);
QString stageText(const resora::ImportProgress &p);

// Failure text of an export, including every affected image.
QString describe(const resora::ExportResult &r);

// Relink helpers -----------------------------------------------------------------------------------
// Candidate replacements for `problems` found by file name under `folder` (recursive). Candidates are
// only proposals: the import engine still checks size and SHA-256 of every one.
QHash<QString, QString> matchByName(const QList<resora::LinkStatus> &problems, const QString &folder);
// True when every problem has a chosen replacement (necessary, not sufficient, to try opening).
bool allAssigned(const QList<resora::LinkStatus> &problems, const QHash<QString, QString> &chosen);
QString stateText(resora::LinkState s);

}  // namespace resoraui
