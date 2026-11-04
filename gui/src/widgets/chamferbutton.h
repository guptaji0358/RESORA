#pragma once
// Custom-painted button: a rectangle with one clipped corner and a letter-spaced caps label.
// Still a QPushButton (keyboard, focus, accessibility, text()), only the painting differs.
// Variants via the "variant" property: "primary" (solid ember), "danger", default ghost outline.
#include <QPushButton>

class ChamferButton : public QPushButton
{
    Q_OBJECT
public:
    explicit ChamferButton(const QString &text, QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }
};
