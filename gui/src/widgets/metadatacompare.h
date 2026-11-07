#pragma once
// Compact technical-metadata table with A/B columns. Meaningful differences get the ember accent
// and a marker; identical values stay quiet; unavailable metadata is muted text, never "0".
#include <QWidget>

#include "backend/insightmetrics.h"

class MetadataCompare : public QWidget
{
    Q_OBJECT
public:
    explicit MetadataCompare(QWidget *parent = nullptr);

    void setMetrics(const InsightMetrics &m);
    int rowCount() const { return rows_; }
    int differingRows() const { return differing_; }

private:
    int rows_ = 0, differing_ = 0;
};
