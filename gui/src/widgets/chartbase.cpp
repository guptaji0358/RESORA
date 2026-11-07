#include "chartbase.h"

#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

#include "ui/theme.h"

ChartBase::ChartBase(const QString &accessibleName, QWidget *parent) : QWidget(parent)
{
    setAccessibleName(accessibleName);
    setMouseTracking(true);
    setFocusPolicy(Qt::TabFocus);
    emptyText_ = tr("Nothing to show.");
}

QColor ChartBase::seriesColor(int i)
{
    // Ember first, then hues that stay apart in lightness too (not colour alone: labels always shown).
    static const QColor ramp[] = {QColor(0xFF, 0x5B, 0x2E), QColor(0xF2, 0xB1, 0x4A), QColor(0xE9, 0xE3, 0xD8),
                                  QColor(0x6F, 0xA8, 0xC4), QColor(0x8F, 0xBF, 0x8A), QColor(0xB0, 0x8F, 0xC9),
                                  QColor(0xA8, 0x6A, 0x4A), QColor(0x8A, 0x84, 0x7A)};
    return ramp[((i % 8) + 8) % 8];
}

void ChartBase::setItems(const QList<ChartItem> &items)
{
    items_ = items;
    hover_ = -1;
    const bool hadSelection = selected_ >= 0;
    selected_ = -1;  // new data: a stale selection would show another collection's detail
    refreshAccessibility();
    updateGeometry();
    update();
    if (hadSelection)
        emit selectionChanged(-1);
}

void ChartBase::setEmptyText(const QString &text)
{
    emptyText_ = text;
    refreshAccessibility();
    update();
}

void ChartBase::refreshAccessibility()
{
    QStringList lines;
    for (const ChartItem &i : items_)
        lines << i.tooltip;
    setAccessibleDescription(items_.isEmpty() ? emptyText_ : lines.join(QStringLiteral("; ")));
}

void ChartBase::setSelected(int index)
{
    if (index < 0 || index >= items_.size())
        index = -1;
    if (index == selected_)
        return;
    selected_ = index;
    update();
    emit selectionChanged(selected_);
}

void ChartBase::setHover(int index)
{
    if (index == hover_)
        return;
    hover_ = index;
    setCursor(index >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
}

void ChartBase::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (items_.isEmpty()) {
        p.setPen(QPen(Theme::line(), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        p.setPen(Theme::mute());
        p.setFont(Theme::display(10, QFont::Normal, 0.3));
        p.drawText(rect().adjusted(16, 8, -16, -8), Qt::AlignCenter | Qt::TextWordWrap, emptyText_);
    } else {
        paintChart(p);
    }
    if (hasFocus())
        drawFocusRing(p);
}

void ChartBase::drawFocusRing(QPainter &p) const
{
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Theme::ember(), 1, Qt::DotLine));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void ChartBase::mouseMoveEvent(QMouseEvent *e)
{
    setHover(items_.isEmpty() ? -1 : indexAt(e->pos()));
}

void ChartBase::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || items_.isEmpty())
        return;
    const int i = indexAt(e->pos());
    setSelected(i < 0 || i == selected_ ? -1 : i);  // click again to clear
}

void ChartBase::leaveEvent(QEvent *)
{
    setHover(-1);
}

void ChartBase::keyPressEvent(QKeyEvent *e)
{
    const int n = items_.size();
    if (n == 0) {
        QWidget::keyPressEvent(e);
        return;
    }
    switch (e->key()) {
    case Qt::Key_Right:
    case Qt::Key_Down:
        setSelected(selected_ < 0 ? 0 : qMin(n - 1, selected_ + 1));
        break;
    case Qt::Key_Left:
    case Qt::Key_Up:
        setSelected(selected_ < 0 ? n - 1 : qMax(0, selected_ - 1));
        break;
    case Qt::Key_Escape:
        setSelected(-1);
        break;
    default:
        QWidget::keyPressEvent(e);
    }
}

bool ChartBase::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        auto *h = static_cast<QHelpEvent *>(e);
        const int i = items_.isEmpty() ? -1 : indexAt(h->pos());
        if (i >= 0)
            QToolTip::showText(h->globalPos(), items_[i].tooltip, this);
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event(e);
}
