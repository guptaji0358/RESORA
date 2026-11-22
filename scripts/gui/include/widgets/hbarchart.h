#pragma once
// Horizontal bar chart (storage by format): label, bar of the share of storage, "size · percent" in
// text. A paper tick on each bar marks that class's share of *images*, so heavy-but-rare formats stand
// out. Zero rows stay visible.
#include "widgets/chartbase.h"

class HBarChart : public ChartBase
{
    Q_OBJECT
public:
    explicit HBarChart(const QString &accessibleName, QWidget *parent = nullptr);
    QSize sizeHint() const override { return {520, rowCountHeight()}; }
    QSize minimumSizeHint() const override { return {320, rowCountHeight()}; }
    void setItems(const QList<ChartItem> &items);

protected:
    int indexAt(const QPoint &pos) const override;
    void paintChart(QPainter &p) override;

private:
    static constexpr int kRow = 40;
    int rowCountHeight() const { return qMax(1, itemCount()) * kRow + 20; }
};
