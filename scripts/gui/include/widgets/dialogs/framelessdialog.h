#pragma once
// Common window for every RESORA dialog: frameless (dark-only identity), but with the window
// behaviour a native frame would give: a visible Close button, a title bar that drags the window
// (QWindow::startSystemMove), Esc closes, and it always opens fully on screen, centred over its
// parent window (or the screen when the parent is missing/minimised). Subclasses add their content
// to bodyLayout() and never touch the frame.
#include <QDialog>

class QAbstractButton;
class QVBoxLayout;
class QWidget;

class FramelessDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FramelessDialog(const QString &title, QWidget *parent = nullptr);

    QVBoxLayout *bodyLayout() const { return bodyLayout_; }
    QWidget *titleBar() const { return titleBar_; }
    QAbstractButton *closeButton() const { return close_; }

    // Centre over the parent window and clamp into the screen's available area (also run on show).
    void placeOnScreen();

    // Embedded mode: the dialog becomes an ordinary child page of a tool workspace (no title bar,
    // not modal, never closes, never repositions itself). Call before adding it to a layout.
    void setEmbedded(bool embedded);
    bool isEmbedded() const { return embedded_; }

protected:
    void done(int result) override;
    void showEvent(QShowEvent *e) override;
    void setVisible(bool visible) override;
    void paintEvent(QPaintEvent *e) override;

private:
    bool embedded_ = false;
    QWidget *anchor_ = nullptr;
    QWidget *titleBar_ = nullptr;
    QAbstractButton *close_ = nullptr;
    QVBoxLayout *bodyLayout_ = nullptr;
};
