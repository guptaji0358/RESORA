#pragma once
// Efficient, read-only preview decoding for the comparison viewer. Qt decodes only for display:
// QImageReader with a scaled size (long edge capped), first frame only (animated images are shown
// as a still), the file is opened read-only and never written. Header sniffing first, so a
// corrupted/unsupported file fails fast with a reason instead of a blank pane.
#include <QImage>
#include <QSize>
#include <QString>

struct PreviewResult
{
    QImage image;     // possibly downscaled; null on failure
    QSize fullSize;   // original pixel size (after EXIF auto-transform), from the header
    QString error;    // empty on success
    int frames = 1;   // >1: animated / multi-frame, only the first is shown
    bool downscaled = false;

    bool ok() const { return error.isEmpty() && !image.isNull(); }
};

namespace PreviewLoader {

constexpr int kDefaultMaxEdge = 4096;  // 4096 px square RGBA = 64 MB worst case per image

// Synchronous; call from a worker thread. Safe to run concurrently for different files.
PreviewResult load(const QString &path, int maxEdge = kDefaultMaxEdge);

}  // namespace PreviewLoader
