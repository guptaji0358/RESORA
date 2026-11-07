#pragma once
// File size comparison: two proportional horizontal bars on one shared scale from 0, with the
// absolute and percentage difference. Zero sizes draw an empty bar; missing sizes are labelled
// "Not available" and excluded from the scale.
#include "insightcanvas.h"

class FileSizeBars : public InsightCanvas
{
    Q_OBJECT
public:
    explicit FileSizeBars(QWidget *parent = nullptr);

protected:
    bool hasData() const override { return m_.a.hasBytes || m_.b.hasBytes; }
    QString describe() const override;
    QString missingText() const override { return tr("File sizes are not available for either image."); }
    void paintContent(QPainter &p, const QRect &area) override;
};
