#pragma once
// "Darkroom" identity: warm black, paper-white text, one ember safelight accent.
// Colour tokens, fonts and the stylesheet for the few stock widgets (line edits, scroll bars, tooltips).
// Custom-painted widgets read the same tokens from here.
#include <QColor>
#include <QFont>
#include <QString>

class QApplication;
class QWidget;

namespace Theme {

void applyPalette(QApplication &app);
QString styleSheet();

QColor ink();      // window background
QColor panel();    // slightly raised surface
QColor line();     // hairlines
QColor lineHi();   // hovered / emphasised hairlines
QColor paper();    // primary text
QColor mute();     // secondary text (>= 4.5:1 on ink)
QColor ember();    // accent: running, primary action, focus
QColor signal();   // success
QColor alert();    // failure / danger

QString accentColor();
QString dangerColor();
QString successColor();
QString neutralColor();

QFont display(qreal pt, int weight = QFont::DemiBold, qreal tracking = 0);  // Bahnschrift, DIN-like
QFont mono(qreal pt, int weight = QFont::Normal);                           // Cascadia Mono / Consolas

// Sets a dynamic property and re-polishes so [name="value"] selectors re-evaluate.
void setProperty(QWidget *w, const char *name, const QString &value);

}  // namespace Theme
