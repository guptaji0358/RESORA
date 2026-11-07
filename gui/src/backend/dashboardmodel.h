#pragma once
// Turns AnalyticsData (already parsed from the engine) into chart-ready items and insight lines for
// the Dashboard. Qt Core only, so it is unit-testable. Nothing is recomputed: every count, percentage
// and byte figure is the engine's; this layer only picks, orders and words them. Missing values stay
// "not available" (-1), never 0.
#include <QList>
#include <QString>
#include <QStringList>

#include "analyticsmodel.h"

struct ChartItem
{
    QString label;
    qint64 count = 0;
    double percent = -1;        // share of analyzed images, 0..100, -1 = not available
    double value = 0;           // what bar length / segment angle is proportional to (>= 0)
    double secondary = -1;      // optional second share (storage chart: share of images), -1 = none
    QString valueText;          // short text shown on the chart ("1,204", "8.38 GB · 88.9%")
    QString tooltip;            // exact values for hover
    QString detail;             // statistics revealed when the item is selected
};

namespace DashboardModel {

QList<ChartItem> categoryItems(const AnalyticsData &d);     // resolution ladder, engine order, zeros kept
QList<ChartItem> orientationItems(const AnalyticsData &d);
QList<ChartItem> formatItems(const AnalyticsData &d);       // by image count
QList<ChartItem> storageItems(const AnalyticsData &d);      // by bytes; empty when no file size is known
QStringList insights(const AnalyticsData &d);               // plain sentences, only from engine figures

// Sum of item values (the total a ring or bar set is proportional to).
double totalValue(const QList<ChartItem> &items);
// Upper end of a bar axis: the smallest "nice" value (1, 2, 5 x 10^n) >= max; 1 for max <= 1.
qint64 niceAxisMax(qint64 max);

}  // namespace DashboardModel
