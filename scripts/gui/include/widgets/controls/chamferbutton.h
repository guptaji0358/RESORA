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
    QSize minimumSizeHint() const override;
    // Icon family member (Assets/icons) drawn left of the label in the label's colour; empty = none.
    void setGlyph(const QString &name);
    QString glyph() const { return glyph_; }
    // Compact: 28px row-action size (smaller type, no 120px minimum). IconOnly: glyph alone (needs a
    // glyph); the label stays as text()/accessible name and should also be set as the tooltip.
    void setCompact(bool on);
    void setIconOnly(bool on);
    bool isIconOnly() const { return iconOnly_; }

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }
    // Enter/Return activate only a button the user reached with the keyboard (Tab / Shift+Tab / mnemonic).
    // A button focused by mouse click or automatically never reacts to a stray Enter; Space always works.
    void focusInEvent(QFocusEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    QFont labelFont() const;
    bool keyboardFocused_ = false;
    QString glyph_;
    bool compact_ = false;
    bool iconOnly_ = false;
};
