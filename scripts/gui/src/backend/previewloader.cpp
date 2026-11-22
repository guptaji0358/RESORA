#include "backend/previewloader.h"

#include <QFileInfo>
#include <QImageReader>

namespace PreviewLoader {

PreviewResult load(const QString &path, int maxEdge)
{
    PreviewResult out;
    const QFileInfo info(path);
    if (!info.isFile()) {
        out.error = QObject::tr("The file no longer exists.");
        return out;
    }
    QImageReader reader(path);  // opened read-only
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        out.error = reader.errorString().isEmpty() ? QObject::tr("Unsupported or unreadable image.") : reader.errorString();
        return out;
    }
    out.fullSize = reader.size();  // header only; invalid for a few formats
    out.frames = qMax(1, reader.imageCount());
    if (out.fullSize.isValid() && qMax(out.fullSize.width(), out.fullSize.height()) > maxEdge) {
        QSize target = out.fullSize;
        target.scale(maxEdge, maxEdge, Qt::KeepAspectRatio);
        reader.setScaledSize(target);
        out.downscaled = true;
    }
    out.image = reader.read();
    if (out.image.isNull()) {
        out.error = reader.errorString().isEmpty() ? QObject::tr("The image could not be decoded (corrupted?).") : reader.errorString();
        return out;
    }
    if (!out.fullSize.isValid())
        out.fullSize = out.image.size();
    return out;
}

}  // namespace PreviewLoader
