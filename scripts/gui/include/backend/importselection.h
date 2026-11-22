#pragma once
// What the user imported (a folder, one image, or several images) and why anything was left out.
// Presentation-side bookkeeping only: the GUI never reads image content. The extension list is a
// pre-check for clear messages and for the file dialog filter; the Python engine stays authoritative.
#include <QString>
#include <QStringList>

struct ImportSelection
{
    enum Kind { None, Folder, SingleImage, MultipleImages };

    Kind kind = None;
    QString folder;       // Folder kind: absolute native path
    QStringList files;    // image kinds: accepted, unique, absolute native paths, in the order given
    QStringList missing;      // do not exist / are not files
    QStringList unsupported;  // exist but have no known image extension
    int duplicates = 0;       // exact repeats that were collapsed
    QString problem;          // why nothing can be scanned (empty when scannable)

    static bool isSupportedExtension(const QString &path);
    static QStringList imageExtensions();   // lowercase, no dot
    static QString dialogFilter();          // "Images (*.jpg ...);;All files (*)"

    static ImportSelection fromFolder(const QString &path);
    static ImportSelection fromFiles(const QStringList &paths);
    // Dropped items: exactly one directory -> folder, otherwise treated as files.
    static ImportSelection fromPaths(const QStringList &paths);

    bool isValid() const { return problem.isEmpty() && kind != None; }
    int imageCount() const { return files.size(); }
    QString kindLabel() const;     // "FOLDER", "SINGLE IMAGE", "MULTIPLE IMAGES"
    QString title() const;         // folder name / file name / "12 images"
    QString location() const;      // where it lives (for several images: the common folder, if any)
    QStringList notes() const;     // human explanations of anything skipped or collapsed
};
