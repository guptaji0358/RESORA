#pragma once
// Two ImagePanes side by side plus the zoom/pan synchronisation between them.
// Sync is relative: zooming one pane by x1.2 zooms the other by x1.2 (so two images of different
// sizes keep their scale ratio), and panning moves both by the same screen distance. With sync
// off each pane is fully independent.
#include <QWidget>

#include "imagepane.h"

class ComparisonViewer : public QWidget
{
    Q_OBJECT
public:
    explicit ComparisonViewer(QWidget *parent = nullptr);

    ImagePane *left() const { return left_; }
    ImagePane *right() const { return right_; }
    ImagePane *pane(int side) const { return side == 0 ? left_ : right_; }

    bool syncZoom() const { return syncZoom_; }
    bool syncPan() const { return syncPan_; }
    void setSyncZoom(bool on) { syncZoom_ = on; }
    void setSyncPan(bool on) { syncPan_ = on; }

    void fitBoth();       // each image fits its own pane (aspect ratio kept)
    void actualSizeBoth();  // 100 % in both panes
    void resetView() { fitBoth(); }

    QWidget *leftHolder() const { return leftHolder_; }
    QWidget *rightHolder() const { return rightHolder_; }

private:
    ImagePane *left_;
    ImagePane *right_;
    QWidget *leftHolder_;
    QWidget *rightHolder_;
    bool syncZoom_ = true;
    bool syncPan_ = true;
};
