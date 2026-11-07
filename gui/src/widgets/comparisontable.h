#pragma once
// Display of a Comparison: insights card + aligned property table (A | B) where meaningful
// differences are marked with the ember accent. Missing metadata is shown muted as "Not available".
#include <QWidget>

#include "backend/comparison.h"

class InsightsView;
class QVBoxLayout;

class ComparisonTable : public QWidget
{
    Q_OBJECT
public:
    explicit ComparisonTable(QWidget *parent = nullptr);

    void setMessage(const QString &text, bool error = false);  // placeholder / failure text
    void setComparison(const Comparison &c);
    int rowCount() const { return rows_; }
    int highlightedRows() const { return highlighted_; }
    InsightsView *insights() const { return insights_; }  // null until a comparison is shown

private:
    void clear();

    InsightsView *insights_ = nullptr;
    QVBoxLayout *layout_;
    int rows_ = 0;
    int highlighted_ = 0;
};
