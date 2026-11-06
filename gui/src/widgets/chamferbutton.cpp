#include "chamferbutton.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>

#include "ui/icons.h"
#include "ui/theme.h"

namespace {
constexpr int kGlyph = 16;
constexpr int kGlyphGap = 9;
}  // namespace

ChamferButton::ChamferButton(const QString &text, QWidget *parent) : QPushButton(text, parent)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAutoDefault(false);  // never an implicit dialog default: a stray Enter must not click it
    setFont(Theme::display(9, QFont::DemiBold, 2));
}

void ChamferButton::focusInEvent(QFocusEvent *e)
{
    const Qt::FocusReason r = e->reason();
    keyboardFocused_ = r == Qt::TabFocusReason || r == Qt::BacktabFocusReason || r == Qt::ShortcutFocusReason;
    QPushButton::focusInEvent(e);
}

void ChamferButton::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (keyboardFocused_ && !e->isAutoRepeat())
            click();
        e->accept();  // consumed either way, so it cannot reach a dialog default
        return;
    }
    QPushButton::keyPressEvent(e);
}

QFont ChamferButton::labelFont() const
{
    // Fixed by design: the application stylesheet re-fonts every QWidget on polish, which made
    // sizeHint (measured early) disagree with paintEvent (measured late) and squeezed labels.
    return compact_ ? Theme::display(8, QFont::DemiBold, 1) : Theme::display(9, QFont::DemiBold, 2);
}

QSize ChamferButton::sizeHint() const
{
    const int g = compact_ ? 14 : kGlyph, gap = compact_ ? 7 : kGlyphGap;
    if (iconOnly_ && !glyph_.isEmpty())
        return {compact_ ? 36 : 44, compact_ ? 28 : 38};
    const int pad = compact_ ? 30 : 44;  // total horizontal padding, so text never touches the edge
    const int w = QFontMetrics(labelFont()).horizontalAdvance(text().toUpper()) + text().size() * (compact_ ? 1 : 2) + pad;
    return {qMax(w + (glyph_.isEmpty() ? 0 : g + gap), compact_ ? 0 : 120), compact_ ? 28 : 38};
}

QSize ChamferButton::minimumSizeHint() const
{
    return sizeHint();  // never let a layout squeeze the label against the border
}

void ChamferButton::setCompact(bool on)
{
    compact_ = on;
    setFont(on ? Theme::display(8, QFont::DemiBold, 1) : Theme::display(9, QFont::DemiBold, 2));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    updateGeometry();
    update();
}

void ChamferButton::setIconOnly(bool on)
{
    iconOnly_ = on;
    updateGeometry();
    update();
}

void ChamferButton::setGlyph(const QString &name)
{
    glyph_ = name;
    updateGeometry();
    update();
}

void ChamferButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QString variant = property("variant").toString();
    const bool primary = variant == QLatin1String("primary");
    const QColor tone = variant == QLatin1String("danger") ? Theme::alert() : Theme::ember();
    const bool on = isEnabled(), hot = on && (underMouse() || hasFocus()), down = on && isDown();

    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    const qreal c = compact_ ? 7 : 11;  // clipped bottom-right corner
    const int kGlyph = compact_ ? 14 : ::kGlyph, kGlyphGap = compact_ ? 7 : ::kGlyphGap;
    QPainterPath shape;
    shape.moveTo(r.topLeft());
    shape.lineTo(r.topRight());
    shape.lineTo(r.right(), r.bottom() - c);
    shape.lineTo(r.right() - c, r.bottom());
    shape.lineTo(r.bottomLeft());
    shape.closeSubpath();

    QColor fill = Qt::transparent, edge, text;
    if (!on) {
        edge = Theme::line();
        text = QColor(0x5e, 0x57, 0x4f);
    } else if (primary) {
        fill = down ? tone.darker(130) : (hot ? tone.lighter(115) : tone);
        edge = fill;
        text = Theme::ink();
    } else {
        fill = down ? QColor(tone.red(), tone.green(), tone.blue(), 60)
                    : (hot ? QColor(tone.red(), tone.green(), tone.blue(), 28) : Qt::transparent);
        edge = hot ? tone : (variant == QLatin1String("danger") ? tone : Theme::lineHi());
        text = hot || variant == QLatin1String("danger") ? tone : Theme::paper();
    }
    p.fillPath(shape, fill);
    p.setPen(QPen(edge, hasFocus() ? 2 : 1.2));
    p.drawPath(shape);

    p.setFont(labelFont());
    p.setPen(text);
    const QString label = this->text().toUpper();
    if (iconOnly_ && !glyph_.isEmpty()) {
        const QColor acc = !on ? text : (primary || hot ? text : Theme::ember());
        p.drawPixmap((width() - kGlyph) / 2, (height() - kGlyph) / 2,
                     Icons::pixmap(glyph_, kGlyph, text, acc, devicePixelRatioF()));
        return;
    }
    if (glyph_.isEmpty()) {
        p.drawText(rect().adjusted(14, 0, -14, 0), Qt::AlignCenter, label);
        return;
    }
    // Glyph + label are centred as one unit. On the solid primary fill the ember accent pixel would
    // vanish, so the whole glyph takes the label colour there.
    const int tw = p.fontMetrics().horizontalAdvance(label) + label.size() * 2;
    int x = (width() - (kGlyph + kGlyphGap + tw)) / 2;
    const QColor accent = !on ? text : (primary ? text : (hot ? text : Theme::ember()));
    p.drawPixmap(x, (height() - kGlyph) / 2, Icons::pixmap(glyph_, kGlyph, text, accent, devicePixelRatioF()));
    p.drawText(QRect(x + kGlyph + kGlyphGap, 0, tw + 4, height()), Qt::AlignVCenter | Qt::AlignLeft, label);
}
