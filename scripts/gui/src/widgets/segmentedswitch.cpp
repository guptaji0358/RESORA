#include "widgets/segmentedswitch.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include "ui/theme.h"

namespace { constexpr int kPad = 18, kH = 32; }

SegmentedSwitch::SegmentedSwitch(const QStringList &labels, QWidget *parent) : QWidget(parent), labels_(labels)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(tr("View"));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize SegmentedSwitch::sizeHint() const
{
    const QFontMetrics fm(Theme::display(9, QFont::DemiBold, 1.5));
    int w = 0;
    for (const QString &l : labels_)
        w += fm.horizontalAdvance(l.toUpper()) + 2 * kPad + 12;
    return {w, kH};
}

int SegmentedSwitch::segmentAt(int x) const
{
    const int n = labels_.size();
    return n == 0 ? -1 : qBound(0, x * n / qMax(1, width()), n - 1);
}

void SegmentedSwitch::setCurrent(int index)
{
    if (index < 0 || index >= labels_.size() || index == current_)
        return;
    current_ = index;
    update();
    emit currentChanged(current_);
}

void SegmentedSwitch::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        setCurrent(segmentAt(e->pos().x()));
}

void SegmentedSwitch::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Left)
        setCurrent(current_ - 1);
    else if (e->key() == Qt::Key_Right)
        setCurrent(current_ + 1);
    else
        QWidget::keyPressEvent(e);
}

void SegmentedSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const int n = labels_.size();
    p.setPen(Theme::line());
    p.drawLine(0, height() - 1, width(), height() - 1);
    for (int i = 0; i < n; ++i) {
        const QRect r(width() * i / n, 0, width() / n, height());
        const bool on = i == current_;
        p.setFont(Theme::display(9, QFont::DemiBold, 1.5));
        p.setPen(on ? Theme::paper() : Theme::mute());
        p.drawText(r, Qt::AlignCenter, labels_[i].toUpper());
        if (on) {
            p.fillRect(QRect(r.left() + 6, height() - 3, r.width() - 12, 3), Theme::ember());
        }
    }
    if (hasFocus()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Theme::ember(), 1, Qt::DotLine));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }
}
