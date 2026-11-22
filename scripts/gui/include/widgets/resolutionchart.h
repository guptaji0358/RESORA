#pragma once
// Resolution comparison: paired horizontal bars (megapixels, one shared scale from 0) with
// dimensions, relative pixel count, absolute MP difference and percentage difference.
#include "widgets/insightcanvas.h"

class ResolutionChart : public InsightCanvas
{
    Q_OBJECT
public:
    explicit ResolutionChart(QWidget *parent = nullptr);

protected:
    bool hasData() const override { return m_.pixelsComparable; }
    QString describe() const override;
    QString missingText() const override;
    void paintContent(QPainter &p, const QRect &area) override;
};
