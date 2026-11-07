#include "hbarchart.h"

#include <QPainter>

#include "backend/analyticsmodel.h"
#include "ui/theme.h"

HBarChart::HBarChart(const QString &accessibleName, QWidget *parent) : ChartBase(accessibleName, parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(rowCountHeight());
}

void HBarChart::setItems(const QList<ChartItem> &items)
{
    ChartBase::setItems(items);
    setFixedHeight(items.isEmpty() ? 90 : rowCountHeight());
}

int HBarChart::indexAt(const QPoint &pos) const
{
    const int i = pos.y() / kRow;
    return pos.y() < 0 || i >= itemCount() ? -1 : i;
}

void HBarChart::paintChart(QPainter &p)
{
    const QList<ChartItem> &its = items();
    p.setRenderHint(QPainter::Antialiasing, false);
    const int labelW = 84, textW = 190;
    const int x0 = labelW + 10;
    const int barW = qMax(40, width() - x0 - textW - 8);
    for (int i = 0; i < its.size(); ++i) {
        const ChartItem &it = its[i];
        const int top = i * kRow;
        const bool active = i == hovered() || i == selected();
        const bool zero = it.value <= 0;
        if (active) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, i == selected() ? 22 : 12));
            p.drawRect(QRect(0, top + 1, width(), kRow - 2));
        }
        p.setPen(zero ? Theme::mute() : Theme::paper());
        p.setFont(Theme::display(10, QFont::DemiBold, 0.3));
        p.drawText(QRect(6, top, labelW, kRow), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(it.label, Qt::ElideRight, labelW));
        const QRect track(x0, top + kRow / 2 - 6, barW, 12);
        p.fillRect(track, Theme::line());
        const int fw = it.percent <= 0 ? 0 : qMax(2, int(track.width() * qMin(1.0, it.percent / 100.0)));
        p.fillRect(QRect(track.left(), track.top(), fw, track.height()), active ? seriesColor(0).lighter(118) : seriesColor(0));
        if (it.secondary >= 0) {
            const int tx = track.left() + int(track.width() * qMin(1.0, it.secondary / 100.0));
            p.setPen(QPen(Theme::paper(), 2));
            p.drawLine(tx, track.top() - 4, tx, track.bottom() + 4);
        }
        p.setPen(Theme::mute());
        p.setFont(Theme::mono(10));
        p.drawText(QRect(width() - textW - 6, top, textW, kRow), Qt::AlignRight | Qt::AlignVCenter, it.valueText);
    }
}
