#pragma once
// Camera-viewfinder progress display painted from real values: a 16:9 frame with corner brackets
// that "exposes" from the bottom as the fraction grows. fraction < 0 = indeterminate (a scan line
// sweeps; the animation runs only while indeterminate and visible).
#include <QColor>
#include <QWidget>

class QVariantAnimation;

class Viewfinder : public QWidget
{
    Q_OBJECT
public:
    explicit Viewfinder(QWidget *parent = nullptr);

    void setFraction(double fraction);   // clamped to 0..1, or any negative = indeterminate
    void setTexts(const QString &center, const QString &sub);
    void setColor(const QColor &color);
    double fraction() const { return fraction_; }
    QSize sizeHint() const override { return {420, 236}; }
    QSize minimumSizeHint() const override { return {240, 135}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w * 9 / 16; }

protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *) override { syncAnimation(); }
    void hideEvent(QHideEvent *) override { syncAnimation(); }

private:
    void syncAnimation();
    double fraction_ = 0.0;
    double sweep_ = 0.0;
    QColor color_;
    QString center_, sub_;
    QVariantAnimation *anim_;
};
