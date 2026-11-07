#include "donutchart.h"

#include <QPainter>
#include <QPainterPath>
#include <cmath>

#include "ui/theme.h"
#include "ui/uiformat.h"

namespace { constexpr int kRing = 168; }

DonutChart::DonutChart(const QString &accessibleName, QWidget *parent) : ChartBase(accessibleName, parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
}

double DonutChart::sweep(double value, double total)
{
    return total <= 0 || value <= 0 ? 0.0 : 360.0 * qMin(1.0, value / total);
}

int DonutChart::heightForWidth(int w) const
{
    const int rows = qMax(1, itemCount());
    return w < 380 ? kRing + 12 + rows * legendRowHeight() + 8 : qMax(kRing + 16, rows * legendRowHeight() + 16);
}

QRectF DonutChart::ringRect() const
{
    const int d = qMin(kRing, qMin(width(), stacked() ? width() : width() / 2));
    const double x = stacked() ? (width() - d) / 2.0 : 8.0;
    return QRectF(x, 8, d - 16, d - 16);
}

QRect DonutChart::legendRect() const
{
    if (stacked())
        return QRect(0, kRing + 8, width(), height() - kRing - 8);
    const int x = int(ringRect().right()) + 28;
    const int h = itemCount() * legendRowHeight();
    return QRect(x, qMax(8, (height() - h) / 2), qMax(80, width() - x - 4), h);
}

int DonutChart::indexAt(const QPoint &pos) const
{
    const QRect lg = legendRect();
    if (lg.contains(pos))
        return qBound(0, (pos.y() - lg.top()) / legendRowHeight(), itemCount() - 1);
    const QRectF rr = ringRect();
    const QPointF d = QPointF(pos) - rr.center();
    const double rad = std::hypot(d.x(), d.y());
    if (rad > rr.width() / 2 || rad < rr.width() * 0.32)
        return -1;
    double deg = std::fmod(90.0 - std::atan2(-d.y(), d.x()) * 180.0 / M_PI + 360.0, 360.0);  // clockwise from 12
    const double total = DashboardModel::totalValue(items());
    double acc = 0;
    for (int i = 0; i < itemCount(); ++i) {
        acc += sweep(items()[i].value, total);
        if (deg < acc)
            return i;
    }
    return -1;
}

void DonutChart::paintChart(QPainter &p)
{
    const QList<ChartItem> &its = items();
    const double total = DashboardModel::totalValue(its);
    const QRectF rr = ringRect();
    const QRectF inner = rr.adjusted(rr.width() * 0.18, rr.width() * 0.18, -rr.width() * 0.18, -rr.width() * 0.18);
    double start = 90.0;  // 12 o'clock, clockwise (Qt angles are counter-clockwise)
    p.setPen(Qt::NoPen);
    for (int i = 0; i < its.size(); ++i) {
        const double sw = sweep(its[i].value, total);
        if (sw <= 0)
            continue;
        const bool active = i == hovered() || i == selected();
        const double gap = sw > 4 ? 1.2 : 0.0;
        QPainterPath path;
        path.arcMoveTo(rr, start - gap / 2);
        path.arcTo(rr, start - gap / 2, -(sw - gap));
        path.arcTo(inner, start - sw + gap / 2, sw - gap);
        path.closeSubpath();
        p.setBrush(active ? seriesColor(i).lighter(118) : seriesColor(i));
        p.drawPath(path);
        start -= sw;
    }
    // Centre: selected/hovered share, otherwise the total.
    const int focus = hovered() >= 0 ? hovered() : selected();
    p.setPen(Theme::paper());
    p.setFont(Theme::display(17, QFont::DemiBold));
    const QString big = focus >= 0 ? AnalyticsModel::percentText(its[focus].percent) : UiFormat::count(qint64(std::llround(total)));
    p.drawText(inner.adjusted(0, -8, 0, -8), Qt::AlignCenter, big);
    p.setPen(Theme::mute());
    p.setFont(Theme::display(8, QFont::DemiBold, 1.2));
    p.drawText(inner.adjusted(0, 24, 0, 24), Qt::AlignCenter,
               (focus >= 0 ? its[focus].label : tr("images")).toUpper());

    // Legend: swatch, name, count · percent.
    const QRect lg = legendRect();
    for (int i = 0; i < its.size(); ++i) {
        const QRect row(lg.left(), lg.top() + i * legendRowHeight(), lg.width(), legendRowHeight());
        const bool zero = its[i].count == 0;
        if (i == selected() || i == hovered()) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, i == selected() ? 22 : 12));
            p.drawRect(row);
        }
        p.setPen(zero ? QPen(Theme::mute(), 1) : QPen(Qt::NoPen));
        p.setBrush(zero ? Qt::NoBrush : QBrush(seriesColor(i)));
        p.drawRect(QRect(row.left() + 6, row.center().y() - 5, 10, 10));
        p.setPen(zero ? Theme::mute() : Theme::paper());
        p.setFont(Theme::display(10, QFont::DemiBold, 0.3));
        const QString val = QStringLiteral("%1  ·  %2").arg(its[i].valueText, AnalyticsModel::percentText(its[i].percent));
        p.setFont(Theme::mono(10));
        const int vw = p.fontMetrics().horizontalAdvance(val);
        p.setPen(Theme::mute());
        p.drawText(QRect(row.right() - vw - 6, row.top(), vw + 6, row.height()), Qt::AlignRight | Qt::AlignVCenter, val);
        p.setPen(zero ? Theme::mute() : Theme::paper());
        p.setFont(Theme::display(10, QFont::DemiBold, 0.3));
        p.drawText(QRect(row.left() + 24, row.top(), qMax(10, row.width() - vw - 40), row.height()), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(its[i].label, Qt::ElideRight, qMax(10, row.width() - vw - 40)));
    }
}
