#include "widgets/comparisonviewer.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

ComparisonViewer::ComparisonViewer(QWidget *parent) : QWidget(parent)
{
    left_ = new ImagePane;
    right_ = new ImagePane;
    left_->setObjectName(QStringLiteral("paneLeft"));
    right_->setObjectName(QStringLiteral("paneRight"));
    leftHolder_ = new QWidget;
    rightHolder_ = new QWidget;
    for (auto [holder, pane] : {std::pair<QWidget *, ImagePane *>{leftHolder_, left_}, {rightHolder_, right_}}) {
        auto *l = new QVBoxLayout(holder);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(6);
        l->addWidget(pane, 1);
    }
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);
    row->addWidget(leftHolder_, 1);
    row->addWidget(rightHolder_, 1);

    auto wire = [this](ImagePane *from, ImagePane *to) {
        connect(from, &ImagePane::userZoomed, this, [this, to](double f) {
            if (syncZoom_)
                to->applyZoomFactor(f);
        });
        connect(from, &ImagePane::userPanned, this, [this, to](const QPointF &d) {
            if (syncPan_)
                to->applyPanDelta(d);
        });
        connect(from, &ImagePane::userFit, this, [this, to] {
            if (syncZoom_)
                to->fit();
        });
    };
    wire(left_, right_);
    wire(right_, left_);
}

void ComparisonViewer::fitBoth()
{
    left_->fit();
    right_->fit();
}

void ComparisonViewer::actualSizeBoth()
{
    left_->actualSize();
    right_->actualSize();
}
