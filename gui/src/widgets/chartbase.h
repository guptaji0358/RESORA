#pragma once
// Shared behaviour of the Dashboard's painted charts: items, hover (with exact-value tooltip),
// click/keyboard selection, an empty-state message and the RESORA colour ramp. Subclasses only
// implement geometry (indexAt) and drawing (paintChart). Presentation only.
#include <QColor>
#include <QList>
#include <QWidget>

#include "backend/dashboardmodel.h"

class ChartBase : public QWidget
{
    Q_OBJECT
public:
    explicit ChartBase(const QString &accessibleName, QWidget *parent = nullptr);

    void setItems(const QList<ChartItem> &items);
    void setEmptyText(const QString &text);
    const QList<ChartItem> &items() const { return items_; }
    int itemCount() const { return items_.size(); }
    bool isEmptyState() const { return items_.isEmpty(); }
    int selected() const { return selected_; }
    void setSelected(int index);   // -1 clears; emits selectionChanged on change
    int hovered() const { return hover_; }
    QString emptyText() const { return emptyText_; }

    // Series colour i (stable ramp, cycles). Legends always repeat the label as text.
    static QColor seriesColor(int i);

signals:
    void selectionChanged(int index);  // -1 = cleared

protected:
    virtual int indexAt(const QPoint &pos) const = 0;
    virtual void paintChart(QPainter &p) = 0;

    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override;
    void keyPressEvent(QKeyEvent *e) override;
    bool event(QEvent *e) override;

    void drawFocusRing(QPainter &p) const;

private:
    void setHover(int index);
    void refreshAccessibility();
    QList<ChartItem> items_;
    QString emptyText_;
    int hover_ = -1, selected_ = -1;
};
