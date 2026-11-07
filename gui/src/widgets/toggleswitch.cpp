#include "widgets/toggleswitch.h"

#include <QPainter>

#include "ui/theme.h"

namespace { constexpr int kSlotW = 38, kSlotH = 18, kGap = 12; }

ToggleSwitch::ToggleSwitch(const QString &text, QWidget *parent) : QCheckBox(text, parent)
{
    setCursor(Qt::PointingHandCursor);
    setFont(Theme::display(10, QFont::Normal, 0.3));
}

QSize ToggleSwitch::sizeHint() const
{
    return {kSlotW + kGap + fontMetrics().horizontalAdvance(text()) + 4, 28};
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    const bool on = isChecked(), en = isEnabled();
    const QColor tone = !en ? QColor(0x5e, 0x57, 0x4f) : (on ? Theme::ember() : Theme::lineHi());
    const QRect slot(1, (height() - kSlotH) / 2, kSlotW, kSlotH);
    p.setPen(QPen(tone, 1));
    p.setBrush(on && en ? QColor(255, 91, 46, 40) : Qt::NoBrush);
    p.drawRect(slot);
    const int barW = 14;  // the sliding bar
    const int x = on ? slot.right() - barW - 2 : slot.left() + 3;
    p.setPen(Qt::NoPen);
    p.setBrush(tone);
    p.drawRect(QRect(x, slot.top() + 3, barW, kSlotH - 5));
    if (hasFocus()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Theme::ember(), 1, Qt::DotLine));
        p.drawRect(slot.adjusted(-3, -3, 3, 3));
    }
    p.setFont(font());
    p.setPen(en ? Theme::paper() : QColor(0x5e, 0x57, 0x4f));
    p.drawText(QRect(kSlotW + kGap, 0, width() - kSlotW - kGap, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
}
