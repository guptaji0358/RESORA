#include "chamferbutton.h"

#include <QPainter>
#include <QPainterPath>

#include "ui/theme.h"

ChamferButton::ChamferButton(const QString &text, QWidget *parent) : QPushButton(text, parent)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFont(Theme::display(9, QFont::DemiBold, 2));
}

QSize ChamferButton::sizeHint() const
{
    const int w = fontMetrics().horizontalAdvance(text().toUpper()) + text().size() * 2 + 44;
    return {qMax(w, 120), 38};
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
    const qreal c = 11;  // clipped bottom-right corner
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

    p.setFont(font());
    p.setPen(text);
    p.drawText(rect().adjusted(14, 0, -14, 0), Qt::AlignCenter, this->text().toUpper());
}
