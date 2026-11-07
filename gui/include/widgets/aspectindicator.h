#pragma once
// Aspect ratio comparison: both ratios as text, plus a logarithmic "shape scale" (1:3 … 1:1 … 3:1)
// with a marker per image so the distance between the shapes is visible. Identical ratios (within
// 0.5 %) are stated plainly; differences under 2 % are called near-identical, never alarming.
#include "widgets/insightcanvas.h"

class AspectIndicator : public InsightCanvas
{
    Q_OBJECT
public:
    explicit AspectIndicator(QWidget *parent = nullptr);

    enum class Relation { Unknown, Identical, NearlyIdentical, Different };
    Relation relation() const;
    // Position 0..1 of an aspect ratio on the log scale (clamped); exposed for tests.
    static double scalePosition(double aspect);

protected:
    bool hasData() const override { return m_.aspectComparable; }
    QString describe() const override;
    QString missingText() const override { return tr("Aspect ratio unavailable for at least one image."); }
    void paintContent(QPainter &p, const QRect &area) override;
};
