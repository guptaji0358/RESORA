#pragma once
// RESORA icon family: SVG sources in Assets/icons, compiled in through Assets/resora.qrc.
// Every SVG is drawn with two literal tokens, paper #EFE9DD (main strokes) and ember #FF5B2E
// (the single accent "pixel"); they are swapped for the requested colours at render time, so one
// file serves every state (normal / hover / disabled / on-primary) and stays sharp at any DPR.
// A missing or unparsable asset logs a warning and renders a visible "broken" glyph, never nothing.
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace Icons {

// Resource path of a family member, e.g. "folder" -> ":/icons/folder.svg"; also "branding/mark",
// "illustrations/empty-results".
QString resourcePath(const QString &name);
bool exists(const QString &name);

// Renders `name` into a logical `size` square (or `box` for a non-square illustration) at `dpr`.
QPixmap pixmap(const QString &name, const QSize &box, const QColor &main, const QColor &accent, qreal dpr = 1.0);
QPixmap pixmap(const QString &name, int size, const QColor &main, const QColor &accent, qreal dpr = 1.0);

// Un-tinted rendering for artwork that carries its own colours (mark, illustrations).
QPixmap artwork(const QString &name, const QSize &box, qreal dpr = 1.0);

// State-aware icon for stock widgets (QPushButton::setIcon ...): Normal = paper + ember,
// Active (hover) = ember, Disabled = dim, Selected = ember. Re-renders for each size and DPR.
QIcon icon(const QString &name);

// Colours a custom-painted widget should pass for a given interaction state.
QColor dim();

}  // namespace Icons
