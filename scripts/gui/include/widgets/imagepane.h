#pragma once
// One inspection pane of the comparison viewer: custom-painted, dark, with a pixel-grid backdrop.
// Zoom is expressed as screen pixels per ORIGINAL image pixel (1.0 = actual size), so a downscaled
// preview still reports true zoom. Never stretches: the image keeps its own aspect ratio.
// Wheel = zoom at cursor, drag = pan, double-click = fit. All state changes are also available as
// methods (used by the sync logic and tests).
#include <QImage>
#include <QPointF>
#include <QWidget>

class ImagePane : public QWidget
{
    Q_OBJECT
public:
    explicit ImagePane(QWidget *parent = nullptr);

    void setPreview(const QImage &preview, const QSize &fullSize);
    void setMessage(const QString &text, bool error = false);  // shown instead of an image
    void clear();
    bool hasImage() const { return !preview_.isNull(); }
    QSize fullSize() const { return full_; }

    double zoom() const { return fit_ ? fitZoom() : zoom_; }
    double fitZoom() const;
    bool isFit() const { return fit_; }
    QPointF pan() const { return pan_; }  // screen-pixel offset of the image centre from the pane centre

    void fit();
    void actualSize();                                   // 100 %, centred
    void setZoom(double zoom);                           // about the pane centre
    void zoomBy(double factor, const QPointF &anchor);   // anchor in pane coordinates
    void panBy(const QPointF &delta);
    // Same operations without emitting the user* signals (sync and tests).
    void applyZoomFactor(double factor);
    void applyPanDelta(const QPointF &delta);

    static constexpr double kMinZoom = 0.02, kMaxZoom = 64.0;

signals:
    void userZoomed(double factor);       // multiplicative change requested by the user
    void userPanned(const QPointF &delta);
    void userFit();
    void viewChanged();

protected:
    void paintEvent(QPaintEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void clampPan();
    void setZoomAround(double newZoom, const QPointF &anchor);

    QImage preview_;
    QSize full_;
    QString message_;
    bool messageError_ = false;
    bool fit_ = true;
    double zoom_ = 1.0;
    QPointF pan_;
    QPoint dragFrom_;
    bool dragging_ = false;
};
