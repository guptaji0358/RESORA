#include "widgets/flowlayout.h"

#include <QWidget>

FlowLayout::FlowLayout(QWidget *parent, int hSpacing, int vSpacing) : QLayout(parent), h_(hSpacing), v_(vSpacing)
{
    setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout()
{
    qDeleteAll(items_);
}

void FlowLayout::addItem(QLayoutItem *item)
{
    items_.append(item);
}

QLayoutItem *FlowLayout::takeAt(int index)
{
    return index >= 0 && index < items_.size() ? items_.takeAt(index) : nullptr;
}

void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    arrange(rect, true);
}

QSize FlowLayout::minimumSize() const
{
    QSize s;
    for (const QLayoutItem *i : items_)
        s = s.expandedTo(i->minimumSize());
    const QMargins m = contentsMargins();
    return s + QSize(m.left() + m.right(), m.top() + m.bottom());
}

int FlowLayout::arrange(const QRect &rect, bool apply) const
{
    const QMargins m = contentsMargins();
    const QRect area = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
    int x = area.x(), y = area.y(), rowH = 0;
    for (QLayoutItem *i : items_) {
        const QSize s = i->sizeHint();
        if (x > area.x() && x + s.width() > area.right() + 1) {
            x = area.x();
            y += rowH + v_;
            rowH = 0;
        }
        if (apply)
            i->setGeometry(QRect(QPoint(x, y), s));
        x += s.width() + h_;
        rowH = qMax(rowH, s.height());
    }
    return y + rowH - rect.y() + m.bottom();
}
