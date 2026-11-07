#include "barchart.h"

#include <QPainter>

#include "ui/theme.h"
#include "ui/uiformat.h"

BarChart::BarChart(const QString &accessibleName, QWidget *parent) : ChartBase(accessibleName, parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(250);
}

int BarChart::barHeight(double value, double axisMax, int plotHeight)
{
    if (value <= 0 || axisMax <= 0 || plotHeight <= 0)
        return 0;
    return qMax(2, int(plotHeight * qMin(1.0, value / axisMax)));
}

QRect BarChart::plot() const
{
    return QRect(44, 22, qMax(40, width() - 52), height() - 22 - 56);
}

int BarChart::indexAt(const QPoint &pos) const
{
    const int n = itemCount();
    const QRect r = plot();
    if (n == 0 || pos.x() < r.left() || pos.x() >= r.right() || pos.y() < 0)
        return -1;
    return qBound(0, (pos.x() - r.left()) * n / r.width(), n - 1);
}

void BarChart::paintChart(QPainter &p)
{
    const QList<ChartItem> &its = items();
    const int n = its.size();
    const QRect r = plot();
    qint64 maxCount = 0;
    for (const ChartItem &i : its)
        maxCount = qMax(maxCount, i.count);
    const qint64 axis = DashboardModel::niceAxisMax(maxCount);

    p.setFont(Theme::mono(9));
    const int steps = axis >= 2 && axis % 2 == 0 ? 2 : 1;  // no fractional tick labels on tiny axes
    for (int g = 0; g <= steps; ++g) {
        const int y = r.bottom() - r.height() * g / steps;
        p.setPen(g == 0 ? Theme::lineHi() : Theme::line());
        p.drawLine(r.left(), y, r.right(), y);
        p.setPen(Theme::mute());
        p.drawText(QRect(0, y - 8, r.left() - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                   UiFormat::count(axis * g / steps));
    }
    const double slot = double(r.width()) / n;
    for (int i = 0; i < n; ++i) {
        const ChartItem &it = its[i];
        const bool active = i == hovered() || i == selected();
        const QRectF cell(r.left() + i * slot, r.top() - 18, slot, r.height() + 18 + 54);
        if (active) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, i == selected() ? 22 : 12));
            p.drawRect(cell);
        }
        const int bh = barHeight(double(it.count), double(axis), r.height());
        const double bw = qMin(56.0, slot * 0.58);
        const QRectF bar(r.left() + i * slot + (slot - bw) / 2, r.bottom() - bh, bw, bh);
        const QColor c = it.count == 0 ? Theme::mute() : seriesColor(0);
        p.setPen(i == selected() ? QPen(Theme::paper(), 1.5) : Qt::NoPen);
        p.setBrush(active ? c.lighter(120) : c);
        if (bh > 0)
            p.drawRect(bar);
        p.setPen(it.count == 0 ? Theme::mute() : Theme::paper());
        p.setFont(Theme::mono(10, QFont::DemiBold));
        p.drawText(QRectF(cell.left(), bar.top() - 18, slot, 16), Qt::AlignCenter, it.valueText);
        p.setFont(Theme::display(9, QFont::DemiBold, 0.2));
        p.setPen(it.count == 0 ? Theme::mute() : Theme::paper());
        p.drawText(QRectF(cell.left() + 2, r.bottom() + 6, slot - 4, 30), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, it.label);
        p.setFont(Theme::mono(9));
        p.setPen(Theme::mute());
        p.drawText(QRectF(cell.left(), r.bottom() + 38, slot, 14), Qt::AlignCenter, AnalyticsModel::percentText(it.percent));
    }
}
