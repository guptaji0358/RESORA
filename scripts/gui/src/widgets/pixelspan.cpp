#include "widgets/pixelspan.h"

#include <QPainter>
#include <QPolygon>
#include <cmath>

#include "backend/analyticsmodel.h"
#include "ui/theme.h"

PixelSpan::PixelSpan(QWidget *parent) : QWidget(parent)
{
    setAccessibleName(tr("Megapixel span"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(96);
}

double PixelSpan::position(double value, double lo, double hi)
{
    if (lo <= 0 || hi <= 0 || hi <= lo || value <= 0)
        return 0.5;
    return qBound(0.0, std::log(value / lo) / std::log(hi / lo), 1.0);
}

void PixelSpan::setValues(double lowest, double average, double highest)
{
    lo_ = lowest;
    avg_ = average;
    hi_ = highest;
    setAccessibleDescription(hasData() ? tr("Lowest %1, average %2, highest %3")
                                             .arg(AnalyticsModel::megapixelsText(lo_), AnalyticsModel::megapixelsText(avg_),
                                                  AnalyticsModel::megapixelsText(hi_))
                                       : tr("Not available"));
    update();
}

void PixelSpan::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setFont(Theme::mono(9));
    const int left = 8, right = width() - 8, axisY = 48;
    p.setPen(QPen(Theme::line(), 2));
    p.drawLine(left, axisY, right, axisY);
    if (!hasData()) {
        p.setPen(Theme::mute());
        p.drawText(QRect(left, axisY + 10, right - left, 20), Qt::AlignLeft | Qt::AlignVCenter, AnalyticsModel::notAvailable());
        return;
    }
    const bool flat = hi_ <= lo_;
    const auto x = [&](double v) { return flat ? (left + right) / 2 : left + int((right - left) * position(v, lo_, hi_)); };
    for (double mp : {0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 25.0, 50.0, 100.0, 250.0, 500.0, 1000.0}) {  // log ticks
        if (flat || mp < lo_ || mp > hi_)
            continue;
        p.setPen(Theme::line());
        p.drawLine(x(mp), axisY - 4, x(mp), axisY + 4);
    }
    struct Mark { double v; QString name; Qt::Alignment align; bool above; QColor color; };
    const Mark marks[] = {{lo_, tr("LOWEST"), Qt::AlignLeft, true, Theme::paper()},
                          {hi_, tr("HIGHEST"), Qt::AlignRight, true, Theme::paper()},
                          {avg_, tr("AVERAGE"), Qt::AlignHCenter, false, Theme::ember()}};
    for (const Mark &m : marks) {
        const int mx = x(m.v);
        p.setPen(Qt::NoPen);
        p.setBrush(m.color);
        QPolygon tri;
        if (m.above)
            tri << QPoint(mx - 5, axisY - 12) << QPoint(mx + 5, axisY - 12) << QPoint(mx, axisY - 3);
        else
            tri << QPoint(mx - 5, axisY + 12) << QPoint(mx + 5, axisY + 12) << QPoint(mx, axisY + 3);
        p.drawPolygon(tri);
        const QString text = QStringLiteral("%1  %2").arg(m.name, AnalyticsModel::megapixelsText(m.v));
        const int tw = p.fontMetrics().horizontalAdvance(text) + 4;
        int tx = m.align == Qt::AlignLeft ? mx - 6 : m.align == Qt::AlignRight ? mx - tw + 6 : mx - tw / 2;
        tx = qBound(0, tx, qMax(0, width() - tw));
        p.setPen(m.color);
        p.drawText(QRect(tx, m.above ? axisY - 34 : axisY + 16, tw, 18), Qt::AlignVCenter | Qt::AlignLeft, text);
    }
}
