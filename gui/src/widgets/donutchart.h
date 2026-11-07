#pragma once
// Donut chart with a text legend (name, count, percent per row, so colour is never the only cue).
// The legend sits beside the ring, or under it when the widget is narrow. Zero-count classes stay in
// the legend (muted) but get no segment.
#include "chartbase.h"

class DonutChart : public ChartBase
{
    Q_OBJECT
public:
    explicit DonutChart(const QString &accessibleName, QWidget *parent = nullptr);
    QSize sizeHint() const override { return {420, 190}; }
    QSize minimumSizeHint() const override { return {240, 190}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override;
    // Sweep in degrees of a value out of a total (pure, unit-tested); 0 when total <= 0.
    static double sweep(double value, double total);

protected:
    int indexAt(const QPoint &pos) const override;
    void paintChart(QPainter &p) override;
    void resizeEvent(QResizeEvent *) override { setMinimumHeight(heightForWidth(width())); }

private:
    bool stacked() const { return width() < 380; }
    QRectF ringRect() const;
    QRect legendRect() const;
    int legendRowHeight() const { return 24; }
};
