#pragma once
// RESORA brand identity: names, tagline and the painted mark ("the resolving grid").
// The mark is a square quadtree: coarse cells at top-left subdivide toward bottom-right until a
// single ember pixel — resolution increasing until every pixel is clear. Same geometry is mirrored
// by assets/generate_icon.py for the .ico; keep the two in sync.
#include <QIcon>
#include <QRectF>
#include <QString>

class QPainter;

namespace Brand {

QString name();         // "RESORA"
QString description();  // "Image Resolution Analyzer"
QString tagline();      // "See every pixel clearly."
QString version();

// Paints the mark inside `target` (kept square, centred). `ember` false draws it monochrome.
void paintMark(QPainter &p, const QRectF &target);

QIcon icon();  // window / taskbar icon rendered from paintMark at several sizes

}  // namespace Brand
