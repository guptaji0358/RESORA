#include "viewfinder.h"

#include <QPainter>
#include <QVariantAnimation>

#include "ui/theme.h"

Viewfinder::Viewfinder(QWidget *parent)
    : QWidget(parent), color_(Theme::ember()), anim_(new QVariantAnimation(this))
{
    setAccessibleName(tr("Scan progress frame"));
    anim_->setStartValue(0.0);
    anim_->setEndValue(1.0);
    anim_->setDuration(1800);
    anim_->setLoopCount(-1);
    connect(anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        sweep_ = v.toDouble();
        update();
    });
}

void Viewfinder::setFraction(double f)
{
    fraction_ = f < 0 ? -1.0 : qBound(0.0, f, 1.0);
    syncAnimation();
    update();
}

void Viewfinder::setTexts(const QString &center, const QString &sub)
{
    center_ = center;
    sub_ = sub;
    update();
}

void Viewfinder::setColor(const QColor &color)
{
    color_ = color;
    update();
}

void Viewfinder::syncAnimation()
{
    const bool run = fraction_ < 0 && isVisible();
    if (run && anim_->state() != QAbstractAnimation::Running)
        anim_->start();
    else if (!run && anim_->state() == QAbstractAnimation::Running)
        anim_->stop();
}

void Viewfinder::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    // Largest 16:9 rectangle that fits, centred.
    int w = width() - 4, h = w * 9 / 16;
    if (h > height() - 4) { h = height() - 4; w = h * 16 / 9; }
    const QRect f((width() - w) / 2, (height() - h) / 2, w, h);

    p.fillRect(f, Theme::panel());
    // Exposure: rises from the bottom edge with the real fraction.
    if (fraction_ >= 0) {
        const int eh = int(h * fraction_);
        QColor c = color_;
        c.setAlpha(70);
        p.fillRect(QRect(f.left(), f.bottom() + 1 - eh, w, eh), c);
        if (eh > 0 && eh < h)
            p.fillRect(QRect(f.left(), f.bottom() + 1 - eh, w, 2), color_);
    } else {
        const double t = sweep_ < 0.5 ? sweep_ * 2 : (1 - sweep_) * 2;   // ping-pong
        const int y = f.top() + int(t * (h - 2));
        QColor c = color_;
        c.setAlpha(40);
        p.fillRect(QRect(f.left(), qMax(f.top(), y - 22), w, qMin(22, y - f.top())), c);
        p.fillRect(QRect(f.left(), y, w, 2), color_);
    }
    // Rule-of-thirds guides.
    p.setPen(QPen(QColor(255, 255, 255, 14), 1));
    for (int i = 1; i < 3; ++i) {
        p.drawLine(f.left() + w * i / 3, f.top(), f.left() + w * i / 3, f.bottom());
        p.drawLine(f.left(), f.top() + h * i / 3, f.right(), f.top() + h * i / 3);
    }
    // Corner brackets.
    p.setPen(QPen(color_, 2));
    const int b = 18;
    for (int sx : {0, 1})
        for (int sy : {0, 1}) {
            const int x = sx ? f.right() : f.left(), y = sy ? f.bottom() : f.top();
            const int dx = sx ? -b : b, dy = sy ? -b : b;
            p.drawLine(x, y, x + dx, y);
            p.drawLine(x, y, x, y + dy);
        }
    // Centre crosshair + readout.
    p.setPen(QPen(QColor(255, 255, 255, 60), 1));
    const QPoint c = f.center();
    p.drawLine(c.x() - 7, c.y() - h / 5 - 20, c.x() + 7, c.y() - h / 5 - 20);
    p.drawLine(c.x(), c.y() - h / 5 - 27, c.x(), c.y() - h / 5 - 13);
    p.setPen(Theme::paper());
    p.setFont(Theme::display(qBound(18.0, h / 5.0, 44.0), QFont::DemiBold, 1));
    p.drawText(QRect(f.left(), c.y() - h / 5 - 8, w, h / 3), Qt::AlignHCenter | Qt::AlignTop, center_);
    p.setPen(Theme::mute());
    p.setFont(Theme::display(8, QFont::DemiBold, 3));
    p.drawText(QRect(f.left(), f.bottom() - 30, w, 22), Qt::AlignHCenter | Qt::AlignVCenter, sub_.toUpper());
}
