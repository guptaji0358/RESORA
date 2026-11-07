#pragma once
// Dashboard view of the analytics: stat tiles, collection health, resolution bar chart, orientation
// and format donuts, storage bars, megapixel span and insights. Fed with the same AnalyticsData as
// the Detailed view; presentation only.
#include <QWidget>

#include "backend/analyticsmodel.h"

class BarChart;
class ChartBase;
class DonutChart;
class HBarChart;
class MetricCard;
class PixelSpan;
class QLabel;

class AnalyticsDashboard : public QWidget
{
    Q_OBJECT
public:
    explicit AnalyticsDashboard(QWidget *parent = nullptr);
    void setData(const AnalyticsData &d);
    void clear() { setData(AnalyticsData()); }

private:
    QLabel *readoutFor(ChartBase *chart, const QString &hint);
    MetricCard *tiles_[6] = {};
    QLabel *health_, *insights_;
    BarChart *categories_;
    DonutChart *orientation_, *formats_;
    HBarChart *storage_;
    PixelSpan *span_;
};
