#pragma once
// Saved Locations data model: one named, absolute directory shortcut. Plain value type, no I/O
// except the explicit availability probe.
#include <QString>

struct SavedLocation
{
    QString id;    // stable identity (UUID); survives renames and path changes
    QString name;  // user-visible display name
    QString path;  // absolute, cleaned, forward slashes (use displayPath() for the UI)

    QString displayPath() const;  // native separators
};

enum class LocationStatus
{
    Available,     // exists and is a directory
    Missing,       // not found: moved, deleted, or drive/share not connected
    NotDirectory,  // exists but is a file
    Invalid,       // not an absolute path
};

namespace SavedLocations {

constexpr int kMaxNameLength = 64;
constexpr int kMaxPathLength = 4096;  // far above MAX_PATH; long paths are accepted and elided in the UI

// Absolute, cleaned path with forward slashes and no trailing slash; empty when `input` is blank or
// not absolute (relative paths are never accepted: they would depend on the working directory).
QString normalizePath(const QString &input);

// True when two normalized paths name the same place (case-insensitive on Windows).
bool samePath(const QString &a, const QString &b);

LocationStatus probe(const QString &path);
bool isUsable(LocationStatus s);  // only Available locations are recommended
QString statusText(LocationStatus s);

}  // namespace SavedLocations
