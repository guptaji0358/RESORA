#include "imagepane.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "ui/theme.h"

ImagePane::ImagePane(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(160, 160);
    setFocusPolicy(Qt::ClickFocus);
    setCursor(Qt::OpenHandCursor);
    setAccessibleName(tr("Image preview"));
}

void ImagePane::setPreview(const QImage &preview, const QSize &fullSize)
{
    preview_ = preview;
    full_ = fullSize.isValid() ? fullSize : preview.size();
    message_.clear();
    fit_ = true;
    pan_ = QPointF();
    update();
    emit viewChanged();
}

void ImagePane::setMessage(const QString &text, bool error)
{
    preview_ = QImage();
    full_ = QSize();
    message_ = text;
    messageError_ = error;
    fit_ = true;
    pan_ = QPointF();
    update();
    emit viewChanged();
}

void ImagePane::clear()
{
    setMessage(QString());
}

double ImagePane::fitZoom() const
{
    if (full_.isEmpty() || width() < 2 || height() < 2)
        return 1.0;
    const double margin = 24;
    const double z = std::min((width() - margin) / full_.width(), (height() - margin) / full_.height());
    return std::clamp(z, kMinZoom, kMaxZoom);
}

void ImagePane::fit()
{
    fit_ = true;
    pan_ = QPointF();
    update();
    emit viewChanged();
}

void ImagePane::actualSize()
{
    if (!hasImage())
        return;
    fit_ = false;
    zoom_ = 1.0;
    pan_ = QPointF();
    update();
    emit viewChanged();
}

void ImagePane::setZoom(double z)
{
    if (!hasImage())
        return;
    fit_ = false;
    zoom_ = std::clamp(z, kMinZoom, kMaxZoom);
    clampPan();
    update();
    emit viewChanged();
}

void ImagePane::setZoomAround(double newZoom, const QPointF &anchor)
{
    const double old = zoom();
    newZoom = std::clamp(newZoom, kMinZoom, kMaxZoom);
    if (!hasImage() || newZoom == old)
        return;
    // Keep the image point under `anchor` fixed: pan' = a - (a - pan) * (new/old), a relative to centre.
    const QPointF a = anchor - QPointF(width() / 2.0, height() / 2.0);
    pan_ = a - (a - pan_) * (newZoom / old);
    fit_ = false;
    zoom_ = newZoom;
    clampPan();
    update();
    emit viewChanged();
}

void ImagePane::zoomBy(double factor, const QPointF &anchor)
{
    const double before = zoom();
    setZoomAround(before * factor, anchor);
    const double applied = zoom() / before;
    if (applied != 1.0)
        emit userZoomed(applied);
}

void ImagePane::applyZoomFactor(double factor)
{
    setZoomAround(zoom() * factor, QPointF(width() / 2.0, height() / 2.0));
}

void ImagePane::panBy(const QPointF &delta)
{
    const QPointF before = pan_;
    applyPanDelta(delta);
    if (pan_ != before)
        emit userPanned(pan_ - before);
}

void ImagePane::applyPanDelta(const QPointF &delta)
{
    if (!hasImage())
        return;
    if (fit_) {  // panning leaves fit mode at the current scale
        zoom_ = fitZoom();
        fit_ = false;
    }
    pan_ += delta;
    clampPan();
    update();
    emit viewChanged();
}

void ImagePane::clampPan()
{
    // Keep at least a quarter of the pane covered so the image can never be lost off-screen.
    const double z = zoom();
    const double hw = full_.width() * z / 2.0, hh = full_.height() * z / 2.0;
    const double limX = hw + width() / 2.0 - width() / 4.0, limY = hh + height() / 2.0 - height() / 4.0;
    pan_.setX(std::clamp(pan_.x(), -limX, limX));
    pan_.setY(std::clamp(pan_.y(), -limY, limY));
}

void ImagePane::resizeEvent(QResizeEvent *)
{
    if (!fit_ && hasImage())
        clampPan();
}

void ImagePane::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0B, 0x0A, 0x09));
    p.setPen(QColor(255, 255, 255, 14));
    for (int y = 10; y < height(); y += 24)
        for (int x = 10; x < width(); x += 24)
            p.drawPoint(x, y);

    if (!hasImage()) {
        p.setPen(messageError_ ? Theme::alert() : Theme::mute());
        p.setFont(Theme::display(11, QFont::Normal));
        p.drawText(rect().adjusted(24, 24, -24, -24), Qt::AlignCenter | Qt::TextWordWrap,
                   message_.isEmpty() ? tr("No image selected") : message_);
        return;
    }
    const double z = zoom();
    const QSizeF shown(full_.width() * z, full_.height() * z);
    const QRectF target(QPointF(width() / 2.0, height() / 2.0) + pan_ - QPointF(shown.width() / 2, shown.height() / 2), shown);
    p.setRenderHint(QPainter::SmoothPixmapTransform, z < 2.0);  // crisp pixels when inspecting closely
    p.drawImage(target, preview_);
    p.setPen(QPen(QColor(255, 255, 255, 40), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(target.adjusted(-0.5, -0.5, 0.5, 0.5));
    // Zoom readout.
    p.setFont(Theme::mono(8.5));
    const QString txt = QStringLiteral("%1%  %2").arg(qRound(z * 100)).arg(fit_ ? tr("FIT") : QString());
    p.setPen(Theme::mute());
    p.drawText(rect().adjusted(10, 0, -10, -8), Qt::AlignBottom | Qt::AlignRight, txt.trimmed());
}

void ImagePane::wheelEvent(QWheelEvent *e)
{
    const double steps = e->angleDelta().y() / 120.0;
    if (steps == 0 || !hasImage())
        return;
    zoomBy(std::pow(1.2, steps), e->position());
    e->accept();
}

void ImagePane::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && hasImage()) {
        dragging_ = true;
        dragFrom_ = e->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    setFocus();
}

void ImagePane::mouseMoveEvent(QMouseEvent *e)
{
    if (!dragging_)
        return;
    const QPoint d = e->pos() - dragFrom_;
    dragFrom_ = e->pos();
    panBy(QPointF(d));
}

void ImagePane::mouseReleaseEvent(QMouseEvent *)
{
    dragging_ = false;
    setCursor(Qt::OpenHandCursor);
}

void ImagePane::mouseDoubleClickEvent(QMouseEvent *)
{
    fit();
    emit userFit();
}
