#pragma once
// Two-or-more option switch drawn in RESORA's style (caps labels, ember underline on the active one).
// Keyboard: Left/Right when focused. Presentation only.
#include <QStringList>
#include <QWidget>

class SegmentedSwitch : public QWidget
{
    Q_OBJECT
public:
    explicit SegmentedSwitch(const QStringList &labels, QWidget *parent = nullptr);
    int current() const { return current_; }
    void setCurrent(int index);
    int count() const { return labels_.size(); }
    QString label(int i) const { return labels_.value(i); }
    QSize sizeHint() const override;

signals:
    void currentChanged(int index);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    int segmentAt(int x) const;
    QStringList labels_;
    int current_ = 0;
};
