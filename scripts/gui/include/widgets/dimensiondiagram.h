#pragma once
// Dimension comparison: two rectangles with the true width:height proportions of the images,
// drawn at ONE common scale so size differences are visible, bottom-aligned, never stretched.
// When the size gap is so large that the smaller rectangle would be unreadably tiny, each
// rectangle gets its own scale and the diagram says so.
#include <QRectF>

#include "widgets/insightcanvas.h"

class DimensionDiagram : public InsightCanvas
{
    Q_OBJECT
public:
    explicit DimensionDiagram(QWidget *parent = nullptr);

    struct Layout
    {
        QRectF a, b;               // rectangles in widget coordinates
        bool commonScale = true;
    };
    Layout layoutRects() const;    // exposed for tests

protected:
    bool hasData() const override { return m_.pixelsComparable; }
    QString describe() const override;
    QString missingText() const override;
    void paintContent(QPainter &p, const QRect &area) override;

private:
    QRect diagramArea(const QRect &area) const;
};
