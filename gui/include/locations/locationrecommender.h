#pragma once
// Pure recommendation logic (no widgets): which saved locations to offer for a path input, and what
// value choosing one produces. Only locations whose folder currently exists are recommended.
#include <QList>
#include <QString>

#include "locations/savedlocation.h"

namespace LocationRecommender {

enum class Kind
{
    Folder,      // the input holds a directory
    ReportFile,  // the input holds a report file path; a location supplies the directory
};

struct Context
{
    Kind kind = Kind::Folder;
    QString extension;  // ReportFile only, without dot ("csv")
};

struct Recommendation
{
    SavedLocation location;
    QString value;  // text to put into the input when chosen
};

// `typed` is the input's current text. Folder: empty shows everything, otherwise a case-insensitive
// substring match on name or path. ReportFile: a bare file name (no separator) shows everything;
// a typed directory part narrows to locations whose path starts with it.
QList<Recommendation> recommend(const QList<SavedLocation> &all, const QString &typed, const Context &context);

// Folder -> the directory (native separators). ReportFile -> directory + the file name already typed,
// or `image_resolution_report.<ext>` when none; the extension is added when missing.
QString valueFor(const SavedLocation &location, const QString &typed, const Context &context);

}  // namespace LocationRecommender
