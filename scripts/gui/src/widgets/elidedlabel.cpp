#include "widgets/elidedlabel.h"

#include <QFontMetrics>

ElidedLabel::ElidedLabel(QWidget *parent) : QLabel(parent)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);  // never forces the window wider
    setTextInteractionFlags(Qt::TextSelectableByMouse);
}

void ElidedLabel::setFullText(const QString &text)
{
    full_ = text;
    setToolTip(text);
    applyElision();
}

void ElidedLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    applyElision();
}

void ElidedLabel::applyElision()
{
    setText(fontMetrics().elidedText(full_, Qt::ElideMiddle, qMax(width(), 20)));
}
