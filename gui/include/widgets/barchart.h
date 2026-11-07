#pragma once
// Vertical bar chart (resolution categories): one slot per class, count above the bar, name and
// percentage below, three gridlines on a "nice" axis. Zero classes keep their slot.
#include "widgets/chartbase.h"

class BarChart : public ChartBase
{
    Q_OBJECT
public:
    explicit BarChart(const QString &accessibleName, QWidget *parent = nullptr);
    QSize sizeHint() const override { return {560, 250}; }
    QSize minimumSizeHint() const override { return {320, 220}; }
    // Bar height in pixels for a value on an axis (pure, unit-tested).
    static int barHeight(double value, double axisMax, int plotHeight);

protected:
    int indexAt(const QPoint &pos) const override;
    void paintChart(QPainter &p) override;

private:
    QRect plot() const;
};
