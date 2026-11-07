#include "widgets/dimensiondiagram.h"

#include <QLocale>
#include <QPainter>

#include "ui/theme.h"

namespace {
constexpr int kCaptionHeight = 38;   // caption block under the rectangles
constexpr int kFooterHeight = 50;    // relative-dimensions text
constexpr double kMinSide = 16.0;    // smaller rectangle must keep at least this on its short side

QString pct(double ratio)
{
    return QLocale(QLocale::English).toString(ratio * 100.0, 'f', ratio >= 0.995 && ratio <= 1.005 ? 0 : 1) + QLatin1Char('%');
}
}  // namespace

DimensionDiagram::DimensionDiagram(QWidget *parent) : InsightCanvas(tr("Dimension comparison"), 270, parent) {}

QString DimensionDiagram::missingText() const
{
    return tr("Dimension diagram unavailable: width or height is missing or invalid for at least one image.");
}

QString DimensionDiagram::describe() const
{
    return tr("Image A: %1 px, aspect ratio %2. Image B: %3 px, aspect ratio %4. Image B is %5 of the width and %6 of the height of Image A.")
        .arg(InsightMetricsBuilder::sizeText(m_.a.w, m_.a.h), m_.a.aspectLabel, InsightMetricsBuilder::sizeText(m_.b.w, m_.b.h), m_.b.aspectLabel,
             pct(m_.widthRatio), pct(m_.heightRatio));
}

QRect DimensionDiagram::diagramArea(const QRect &area) const
{
    return area.adjusted(0, 6, 0, -(kCaptionHeight + kFooterHeight));
}

DimensionDiagram::Layout DimensionDiagram::layoutRects() const
{
    Layout l;
    if (!m_.pixelsComparable)
        return l;
    const QRect box = diagramArea(rect());
    const double zoneW = qMax(1, box.width() / 2 - 8), zoneH = qMax(1, box.height());
    const double wa = double(m_.a.w), ha = double(m_.a.h), wb = double(m_.b.w), hb = double(m_.b.h);

    auto fit = [&](double w, double h, double scale) { return QSizeF(w * scale, h * scale); };
    const double common = qMin(zoneW / qMax(wa, wb), zoneH / qMax(ha, hb));
    QSizeF sa = fit(wa, ha, common), sb = fit(wb, hb, common);
    if (qMin(qMin(sa.width(), sa.height()), qMin(sb.width(), sb.height())) < kMinSide) {
        l.commonScale = false;  // too different in size: one scale would hide the smaller image
        sa = fit(wa, ha, qMin(zoneW / wa, zoneH / ha));
        sb = fit(wb, hb, qMin(zoneW / wb, zoneH / hb));
    }
    const double cxA = box.left() + box.width() * 0.25, cxB = box.left() + box.width() * 0.75, base = box.bottom();
    l.a = QRectF(cxA - sa.width() / 2, base - sa.height(), sa.width(), sa.height());
    l.b = QRectF(cxB - sb.width() / 2, base - sb.height(), sb.width(), sb.height());
    return l;
}

void DimensionDiagram::paintContent(QPainter &p, const QRect &area)
{
    const Layout l = layoutRects();
    const QRect box = diagramArea(area);

    // Baseline the rectangles stand on.
    p.setPen(QPen(Theme::line(), 1));
    p.drawLine(area.left(), box.bottom() + 1, area.right(), box.bottom() + 1);

    struct Item { const SideFacts *s; QRectF r; QColor c; QString tag; };
    for (const Item &it : {Item{&m_.a, l.a, colorA(), QStringLiteral("A")}, Item{&m_.b, l.b, colorB(), QStringLiteral("B")}}) {
        QColor fill = it.c;
        fill.setAlphaF(0.18);
        p.setPen(QPen(it.c, 1.5));
        p.setBrush(fill);
        p.drawRect(it.r);
        if (qMin(it.r.width(), it.r.height()) >= 26) {
            p.setFont(Theme::display(14, QFont::DemiBold));
            p.setPen(it.c);
            p.drawText(it.r, Qt::AlignCenter, it.tag);
        }
        // Caption centred under its rectangle, clipped to its half so labels never collide.
        const double zoneL = it.tag == QLatin1String("A") ? area.left() : area.center().x() + 4;
        const QRect cap(int(zoneL), box.bottom() + 6, area.width() / 2 - 4, kCaptionHeight);
        p.setPen(it.c);
        p.setFont(Theme::display(9.5, QFont::DemiBold, 0.3));
        drawElided(p, QRect(cap.left(), cap.top(), cap.width(), 18), Qt::AlignHCenter,
                   QStringLiteral("%1  ·  %2 px").arg(it.tag, InsightMetricsBuilder::sizeText(it.s->w, it.s->h)));
        p.setPen(Theme::mute());
        p.setFont(Theme::display(8.5, QFont::Normal, 0.2));
        drawElided(p, QRect(cap.left(), cap.top() + 18, cap.width(), 16), Qt::AlignHCenter,
                   tr("%1  ·  aspect %2").arg(it.s->name, it.s->aspectLabel));
    }

    const int y = box.bottom() + kCaptionHeight + 8;
    p.setPen(Theme::paper());
    p.setFont(Theme::display(9.5, QFont::Normal, 0.2));
    QString rel = tr("B relative to A:  width %1  ·  height %2").arg(pct(m_.widthRatio), pct(m_.heightRatio));
    p.drawText(QRect(area.left(), y, area.width(), 18), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(rel, Qt::ElideRight, area.width()));
    p.setPen(Theme::mute());
    p.setFont(Theme::display(8.5, QFont::Normal, 0.2));
    p.drawText(QRect(area.left(), y + 19, area.width(), 28), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
               l.commonScale ? tr("Both rectangles use the same scale and keep each image's true proportions.")
                             : tr("The size gap is very large, so each rectangle is scaled separately here; proportions are still exact."));
}
