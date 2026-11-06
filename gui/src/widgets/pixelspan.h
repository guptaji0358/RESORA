#pragma once
// Megapixel span: a logarithmic ruler from the smallest to the largest image with labelled markers
// for LOWEST, AVERAGE and HIGHEST (position + text, not colour, tell them apart). Shows how far apart
// the collection's extremes are and where the mean sits. Values come from the engine; nothing is
// computed here except screen positions.
#include <QWidget>

class PixelSpan : public QWidget
{
    Q_OBJECT
public:
    explicit PixelSpan(QWidget *parent = nullptr);
    // Megapixels; any value <= 0 means "not available" and clears the ruler.
    void setValues(double lowest, double average, double highest);
    bool hasData() const { return lo_ > 0 && avg_ > 0 && hi_ > 0; }
    // Position 0..1 of `value` on a log axis from `lo` to `hi` (0.5 when lo == hi). Pure, unit-tested.
    static double position(double value, double lo, double hi);
    QSize sizeHint() const override { return {420, 96}; }
    QSize minimumSizeHint() const override { return {320, 96}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double lo_ = 0, avg_ = 0, hi_ = 0;
};
