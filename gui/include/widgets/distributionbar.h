#pragma once
// Resolution distribution: one painted row per class (frame glyph at true proportion, share bar, count),
// from real backend counts.
#include <QList>
#include <QPair>
#include <QSize>
#include <QString>
#include <QWidget>

class QVBoxLayout;

class DistributionBar : public QWidget
{
    Q_OBJECT
public:
    explicit DistributionBar(QWidget *parent = nullptr);

    using Entry = QPair<QString, qint64>;
    void setEntries(const QList<Entry> &entries);   // zero/negative counts are dropped
    int rowCount() const { return rows_; }
    // Share of `count` in `total` as 0..1 (0 when total <= 0). Pure, unit-tested.
    // True pixel size of a standard class name (e.g. "Full HD"); empty size for custom/panorama/above/below.
    static QSize standardSize(const QString &label);
    static double share(qint64 count, qint64 total);

private:
    class Row;
    QVBoxLayout *legend_;
    int rows_ = 0;
};
