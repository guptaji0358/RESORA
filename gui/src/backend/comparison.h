#pragma once
// Image Comparison model (Qt Core only, unit-testable). Takes two entries of the backend
// `properties` result (reused as-is: no metadata is recalculated here) and produces aligned
// table rows plus plain-language insights. Absent metadata is "Not available", never 0, and
// never counts as a difference between two images that both lack it.
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include "insightmetrics.h"
#include "propertiesmodel.h"

struct InsightNote
{
    QString section, text;
};

struct CompareRow
{
    QString section;
    QString label;
    QString left, right;
    bool leftAvailable = true, rightAvailable = true;
    bool differs = false;  // meaningful difference (values differ, or only one side has the value)
};

struct Comparison
{
    ImageProperties left, right;
    QList<CompareRow> rows;
    QStringList insights;               // flat list of the detailed explanations (unchanged)
    QList<InsightNote> notes;           // the same explanations, grouped into short sections
    InsightMetrics metrics;             // data behind the visual comparison (valid when complete)
    bool complete = false;  // both images readable

    int differenceCount() const;
};

namespace ComparisonModel {

// Either side may be an unreadable entry (status != ok): rows/insights are then empty and
// `complete` is false.
Comparison compare(const QJsonObject &leftProps, const QJsonObject &rightProps);

// Signed percentage change "+25.0%" of b relative to a; empty when a is not positive.
QString percentChange(double a, double b);

}  // namespace ComparisonModel
