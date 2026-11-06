#include "theme.h"

#include <QApplication>
#include <QPalette>
#include <QStringList>
#include <QStyle>
#include <QVariant>
#include <QWidget>

namespace {

const char kSheet[] = R"CSS(
QWidget { color: @paper; font-family: "Bahnschrift", "Segoe UI", sans-serif; font-size: 10pt; }
QMainWindow, QStackedWidget, #stage { background: @ink; }
QToolTip { color: @paper; background: @panel; border: 1px solid @lineHi; padding: 4px 6px; }
QDialog, QMessageBox { background: @panel; }
QLabel { background: transparent; }

QLabel[role="eyebrow"] { font-size: 8pt; font-weight: 600; color: @ember; letter-spacing: 3px; }
QLabel[role="display"] { font-size: 30pt; font-weight: 600; letter-spacing: 0.5px; }
QLabel[role="lede"] { font-size: 11pt; color: @mute; }
QLabel[role="muted"] { color: @mute; }
QLabel[role="caption"] { color: @mute; font-size: 8pt; font-weight: 600; letter-spacing: 2px; }
QLabel[role="error"] { color: @alert; font-size: 11pt; }
QLabel[role="message"] { font-size: 15pt; font-weight: 600; }
QLabel[role="mono"] { font-family: "Cascadia Mono", "Consolas", monospace; color: @mute; }
QLabel[role="metricValue"] { font-size: 30pt; font-weight: 600; letter-spacing: 1px; }
QLabel[role="metricValueSm"] { font-size: 19pt; font-weight: 600; letter-spacing: 0.5px; }
QLabel[role="metricValue"][tone="good"], QLabel[role="metricValueSm"][tone="good"] { color: @signal; }
QLabel[role="metricValue"][tone="bad"], QLabel[role="metricValueSm"][tone="bad"] { color: @alert; }
QLabel[role="state"] { font-size: 8pt; font-weight: 700; letter-spacing: 3px; color: @mute; }
QLabel[role="state"][state="running"], QLabel[role="state"][state="starting"] { color: @ember; }
QLabel[role="state"][state="succeeded"] { color: @signal; }
QLabel[role="state"][state="failed"] { color: @alert; }

QLineEdit { background: transparent; border: none; border-bottom: 1px solid @lineHi; padding: 6px 2px;
            font-family: "Cascadia Mono", "Consolas", monospace; selection-background-color: @ember;
            selection-color: #0D0C0B; }
QLineEdit:hover { border-bottom-color: @mute; }
QLineEdit:focus { border-bottom: 2px solid @ember; }
QLineEdit[invalid="true"] { border-bottom: 2px solid @alert; }
QLineEdit:disabled { color: #5E574F; border-bottom-color: @line; }

QProgressBar { background: @line; border: none; max-height: 3px; min-height: 3px; }
QProgressBar::chunk { background: @ember; }

QPlainTextEdit { background: transparent; border: 1px solid @line; font-family: "Cascadia Mono", "Consolas", monospace;
                 font-size: 9pt; color: @mute; }
QScrollArea, QAbstractScrollArea { background: transparent; border: none; }
QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
QScrollBar::handle:vertical { background: @lineHi; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: @mute; }
QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page { height: 0; background: none; }
QMenu { background: @panel; border: 1px solid @lineHi; padding: 4px; }
QMenu::item { padding: 5px 20px; }
QMenu::item:selected { background: @ember; color: #0D0C0B; }
)CSS";

struct Token { const char *name; const char *value; };
const Token kTokens[] = {
    {"@ink", "#0D0C0B"},  {"@panel", "#161412"}, {"@lineHi", "#4A443D"}, {"@line", "#2A2622"},
    {"@paper", "#EFE9DD"}, {"@mute", "#9A9186"},  {"@ember", "#FF5B2E"},  {"@signal", "#C9F26B"},
    {"@alert", "#FF4D6D"},
};

}  // namespace

namespace Theme {

QColor ink() { return QColor(0x0d, 0x0c, 0x0b); }
QColor panel() { return QColor(0x16, 0x14, 0x12); }
QColor line() { return QColor(0x2a, 0x26, 0x22); }
QColor lineHi() { return QColor(0x4a, 0x44, 0x3d); }
QColor paper() { return QColor(0xef, 0xe9, 0xdd); }
QColor mute() { return QColor(0x9a, 0x91, 0x86); }
QColor ember() { return QColor(0xff, 0x5b, 0x2e); }
QColor signal() { return QColor(0xc9, 0xf2, 0x6b); }
QColor alert() { return QColor(0xff, 0x4d, 0x6d); }

QString accentColor() { return ember().name(); }
QString dangerColor() { return alert().name(); }
QString successColor() { return signal().name(); }
QString neutralColor() { return QStringLiteral("#6B645B"); }

QString styleSheet()
{
    QString s = QString::fromLatin1(kSheet);
    for (const Token &t : kTokens)
        s.replace(QLatin1String(t.name), QLatin1String(t.value));
    return s;
}

QFont display(qreal pt, int weight, qreal tracking)
{
    QFont f;
    f.setFamilies({QStringLiteral("Bahnschrift"), QStringLiteral("Segoe UI")});
    f.setPointSizeF(pt);
    f.setWeight(QFont::Weight(weight));
    f.setLetterSpacing(QFont::AbsoluteSpacing, tracking);
    return f;
}

QFont mono(qreal pt, int weight)
{
    QFont f;
    f.setFamilies({QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas")});
    f.setPointSizeF(pt);
    f.setWeight(QFont::Weight(weight));
    return f;
}

void applyPalette(QApplication &app)
{
    app.setStyle(QStringLiteral("Fusion"));
    QPalette p;
    p.setColor(QPalette::Window, ink());
    p.setColor(QPalette::WindowText, paper());
    p.setColor(QPalette::Base, ink());
    p.setColor(QPalette::AlternateBase, panel());
    p.setColor(QPalette::Text, paper());
    p.setColor(QPalette::PlaceholderText, mute());
    p.setColor(QPalette::Button, panel());
    p.setColor(QPalette::ButtonText, paper());
    p.setColor(QPalette::ToolTipBase, panel());
    p.setColor(QPalette::ToolTipText, paper());
    p.setColor(QPalette::Highlight, ember());
    p.setColor(QPalette::HighlightedText, ink());
    p.setColor(QPalette::Link, ember());
    for (QPalette::ColorRole r : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, r, QColor(0x5e, 0x57, 0x4f));
    app.setPalette(p);
}

void setProperty(QWidget *w, const char *name, const QString &value)
{
    if (w->property(name).toString() == value)
        return;
    w->setProperty(name, value);
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

}  // namespace Theme
