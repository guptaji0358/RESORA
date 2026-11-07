#pragma once
// Left-to-right layout that wraps to a new row when the width runs out (action button rows), so a
// narrow window never squeezes a button label. Reports height-for-width to its parent.
#include <QLayout>
#include <QList>

class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget *parent = nullptr, int hSpacing = 12, int vSpacing = 12);
    ~FlowLayout() override;

    void addItem(QLayoutItem *item) override;
    int count() const override { return items_.size(); }
    QLayoutItem *itemAt(int index) const override { return items_.value(index); }
    QLayoutItem *takeAt(int index) override;
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return arrange(QRect(0, 0, width, 0), false); }
    void setGeometry(const QRect &rect) override;
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override;

private:
    int arrange(const QRect &rect, bool apply) const;
    QList<QLayoutItem *> items_;
    int h_, v_;
};
