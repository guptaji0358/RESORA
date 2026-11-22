#pragma once
// Share chart: one painted row per class (frame glyph, name, share bar, "count · percent" in text, so
// nothing depends on colour). Optional second line for storage ("size · percent"), drawn as a thinner
// paper-coloured bar under the count bar. Zero rows stay visible (muted) so the full set of classes
// is always readable. Presentation only: all numbers arrive ready-made from the engine.
#include <QList>
#include <QSizeF>
#include <QString>
#include <QWidget>

class ShareChart : public QWidget
{
    Q_OBJECT
public:
    struct Row
    {
        QString name;
        qint64 count = 0;
        double percent = -1;      // 0..100, -1 = not available
        QSizeF frame;             // glyph aspect (w, h); empty = dashed neutral frame
        double glyphScale = 0.7;  // glyph width as a share of the glyph slot (0..1)
        bool hasBytes = false;
        QString bytesText;        // shown when hasBytes
        double bytesPercent = -1;
    };

    explicit ShareChart(const QString &accessibleName, QWidget *parent = nullptr);
    void setRows(const QList<Row> &rows);
    int rowCount() const { return rows_.size(); }
    QString rowText(int row) const;  // what a row says, e.g. "Full HD: 2 images, 50.0%"
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // Bar fill fraction 0..1 for a percentage (0 when not available). Pure, unit-tested.
    static double fraction(double percent);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    int rowHeight() const;
    QList<Row> rows_;
};
