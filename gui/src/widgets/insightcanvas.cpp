#include "widgets/insightcanvas.h"

#include <QPainter>

#include "ui/theme.h"

InsightCanvas::InsightCanvas(const QString &accessibleName, int minHeight, QWidget *parent) : QWidget(parent)
{
    setAccessibleName(accessibleName);
    setMinimumHeight(minHeight);
    setMinimumWidth(220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
}

void InsightCanvas::setMetrics(const InsightMetrics &m)
{
    m_ = m;
    setAccessibleDescription(hasData() ? describe() : missingText());
    update();
}

QString InsightCanvas::unavailableMessage() const
{
    return missingText();
}

QColor InsightCanvas::colorA()
{
    return Theme::ember();
}

QColor InsightCanvas::colorB()
{
    return QColor(0x6F, 0xA8, 0xC4);
}

QString InsightCanvas::sideName(Side s)
{
    switch (s) {
    case Side::A: return tr("Image A");
    case Side::B: return tr("Image B");
    case Side::Tie: return tr("Equal");
    default: return QString();
    }
}

void InsightCanvas::drawElided(QPainter &p, const QRect &r, int flags, const QString &text)
{
    p.drawText(r, flags | Qt::AlignVCenter, p.fontMetrics().elidedText(text, Qt::ElideRight, r.width()));
}

void InsightCanvas::drawBarBlock(QPainter &p, const QRect &r, const QColor &color, const QString &tag, const QString &name,
                                 const QString &value, double fraction, const QString &sub, bool emphasise)
{
    const bool valid = fraction >= 0;
    p.setFont(Theme::display(9.5, QFont::DemiBold, 0.3));
    const QRect line1(r.left(), r.top(), r.width(), 18);
    // Value right-aligned, tag + name take the rest.
    const int valueW = qMin(r.width() / 2, p.fontMetrics().horizontalAdvance(value) + 4);
    p.setPen(valid ? Theme::paper() : Theme::mute());
    drawElided(p, QRect(line1.right() - valueW + 1, line1.top(), valueW, line1.height()), Qt::AlignRight, value);
    p.setPen(color);
    const int tagW = p.fontMetrics().horizontalAdvance(tag) + 8;
    p.drawText(QRect(line1.left(), line1.top(), tagW, line1.height()), Qt::AlignLeft | Qt::AlignVCenter, tag);
    p.setPen(Theme::mute());
    p.setFont(Theme::display(9, QFont::Normal, 0.2));
    drawElided(p, QRect(line1.left() + tagW, line1.top(), qMax(0, line1.width() - tagW - valueW - 8), line1.height()), Qt::AlignLeft, name);

    const QRect track(r.left(), r.top() + 21, r.width(), 12);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x1F, 0x1C, 0x19));
    p.drawRoundedRect(track, 2, 2);
    p.setPen(QPen(Theme::line(), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(track.adjusted(0, 0, -1, -1), 2, 2);
    if (valid && fraction > 0) {
        QColor fill = color;
        if (!emphasise)
            fill.setAlphaF(0.55);
        const int w = qMax(3, int(track.width() * qBound(0.0, fraction, 1.0)));
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(QRect(track.left(), track.top(), w, track.height()), 2, 2);
    }
    p.setPen(Theme::mute());
    p.setFont(Theme::display(8.5, QFont::Normal, 0.2));
    drawElided(p, QRect(r.left(), r.top() + 35, r.width(), 15), Qt::AlignLeft, sub);
}

void InsightCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (!hasData()) {
        p.setPen(QPen(Theme::line(), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        p.setPen(Theme::mute());
        p.setFont(Theme::display(9.5, QFont::Normal, 0.3));
        p.drawText(rect().adjusted(14, 8, -14, -8), Qt::AlignCenter | Qt::TextWordWrap, missingText());
        return;
    }
    paintContent(p, rect());
}
