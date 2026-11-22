#pragma once
// Custom-painted switch: a square-ended slot with a sliding bar. A real QCheckBox underneath
// (isChecked/toggled/keyboard/accessibility); the label is drawn to the right of the slot.
#include <QCheckBox>

class ToggleSwitch : public QCheckBox
{
    Q_OBJECT
public:
    explicit ToggleSwitch(const QString &text, QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *) override;
    bool hitButton(const QPoint &pos) const override { return rect().contains(pos); }
};
