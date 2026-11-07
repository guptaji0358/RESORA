#include "aspectindicator.h"

#include <QLocale>
#include <QPainter>
#include <cmath>

#include "ui/theme.h"

namespace {
constexpr double kScaleLimit = 3.0;   // scale spans 1:3 .. 3:1
constexpr double kNearly = 0.02;      // below 2 % relative difference: "nearly identical"

QString n(double v, int d)
{
    return QLocale(QLocale::English).toString(v, 'f', d);
}
}  // namespace

AspectIndicator::AspectIndicator(QWidget *parent) : InsightCanvas(tr("Aspect ratio comparison"), 170, parent) {}

double AspectIndicator::scalePosition(double aspect)
{
    if (aspect <= 0)
        return 0.5;
    const double t = std::log(aspect) / std::log(kScaleLimit);  // -1 .. +1
    return (qBound(-1.0, t, 1.0) + 1.0) / 2.0;
}

AspectIndicator::Relation AspectIndicator::relation() const
{
    if (!m_.aspectComparable)
        return Relation::Unknown;
    if (m_.aspectSame)
        return Relation::Identical;
    return m_.aspectRelDiff < kNearly ? Relation::NearlyIdentical : Relation::Different;
}

QString AspectIndicator::describe() const
{
    const QString base = tr("Image A: %1. Image B: %2. ").arg(m_.a.aspectLabel, m_.b.aspectLabel);
    switch (relation()) {
    case Relation::Identical: return base + tr("The aspect ratios are identical.");
    case Relation::NearlyIdentical: return base + tr("Nearly identical, differing by %1%.").arg(n(m_.aspectRelDiff * 100, 1));
    default: return base + tr("They differ by %1%.").arg(n(m_.aspectRelDiff * 100, 0));
    }
}

void AspectIndicator::paintContent(QPainter &p, const QRect &area)
{
    // Row 1: a small proportional shape + label per image.
    struct Item { const SideFacts *s; QColor c; QString tag; };
    const Item items[] = {{&m_.a, colorA(), QStringLiteral("A")}, {&m_.b, colorB(), QStringLiteral("B")}};
    const int half = area.width() / 2;
    for (int i = 0; i < 2; ++i) {
        const Item &it = items[i];
        const QRect cell(area.left() + i * half, area.top() + 2, half - 8, 28);
        const double shapeH = 16, shapeW = qBound(8.0, shapeH * it.s->aspect, 40.0);
        const QRectF shape(cell.left(), cell.top() + (cell.height() - shapeH) / 2, shapeW, shapeH);
        QColor fill = it.c;
        fill.setAlphaF(0.2);
        p.setPen(QPen(it.c, 1.5));
        p.setBrush(fill);
        p.drawRect(shape);
        p.setPen(it.c);
        p.setFont(Theme::display(10, QFont::DemiBold, 0.3));
        drawElided(p, QRect(int(shape.right()) + 8, cell.top(), cell.width() - int(shapeW) - 8, cell.height()), Qt::AlignLeft,
                   QStringLiteral("%1  %2  (%3)").arg(it.tag, it.s->aspectLabel, n(it.s->aspect, 3)));
    }

    // Row 2: logarithmic shape scale.
    const int margin = 22;
    const int x0 = area.left() + margin, x1 = area.right() - margin, y = area.top() + 76;
    auto xAt = [&](double aspect) { return x0 + (x1 - x0) * scalePosition(aspect); };
    p.setPen(QPen(Theme::lineHi(), 2));
    p.drawLine(x0, y, x1, y);
    p.setFont(Theme::display(8, QFont::Normal, 0.3));
    struct Tick { double aspect; QString text; };
    for (const Tick &t : {Tick{1.0 / kScaleLimit, tr("1:3 tall")}, Tick{1.0, tr("1:1 square")}, Tick{kScaleLimit, tr("3:1 wide")}}) {
        const int x = int(xAt(t.aspect));
        p.setPen(QPen(Theme::lineHi(), 1));
        p.drawLine(x, y - 4, x, y + 4);
        p.setPen(Theme::mute());
        const int w = 70;
        const int left = qBound(area.left(), x - w / 2, area.right() - w);
        p.drawText(QRect(left, y + 22, w, 14), Qt::AlignHCenter | Qt::AlignVCenter, t.text);
    }
    const double xa = xAt(m_.a.aspect), xb = xAt(m_.b.aspect);
    if (std::abs(xa - xb) > 1.0) {  // distance between the two shapes
        p.setPen(QPen(Theme::mute(), 2));
        p.drawLine(QPointF(xa, y), QPointF(xb, y));
    }
    auto marker = [&](double x, double cy, const QColor &c, const QString &tag) {
        p.setPen(QPen(Theme::ink(), 1.5));
        p.setBrush(c);
        p.drawEllipse(QPointF(x, cy), 8, 8);
        p.setPen(Theme::ink());
        p.setFont(Theme::display(8.5, QFont::Bold));
        p.drawText(QRectF(x - 8, cy - 8, 16, 16), Qt::AlignCenter, tag);
    };
    marker(xa, y - 12, colorA(), QStringLiteral("A"));
    marker(xb, y + 0 + 12 - 0, colorB(), QStringLiteral("B"));

    // Conclusion, in plain words; only a real difference gets the accent colour.
    QString text;
    QColor colour = Theme::paper();
    switch (relation()) {
    case Relation::Identical:
        text = tr("Identical aspect ratio (%1).").arg(m_.a.aspectLabel);
        colour = Theme::signal();
        break;
    case Relation::NearlyIdentical:
        text = tr("Nearly identical: %1 vs %2, differing by only %3%.").arg(m_.a.aspectLabel, m_.b.aspectLabel, n(m_.aspectRelDiff * 100, 1));
        break;
    default: {
        const bool aWider = m_.a.aspect > m_.b.aspect;
        text = tr("Different shapes: %1 vs %2. %3 is relatively wider (by %4%).")
                   .arg(m_.a.aspectLabel, m_.b.aspectLabel, aWider ? tr("Image A") : tr("Image B"), n(m_.aspectRelDiff * 100, 0));
        colour = Theme::ember();
    }
    }
    p.setPen(colour);
    p.setFont(Theme::display(10, QFont::DemiBold, 0.2));
    p.drawText(QRect(area.left(), y + 44, area.width(), 40), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text);
}
