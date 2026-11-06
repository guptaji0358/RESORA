#include "framelessdialog.h"

#include <QAbstractButton>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWindow>

#include "ui/theme.h"

namespace {
constexpr int kScreenMargin = 24;  // keep this much desktop visible around the dialog

class CloseButton : public QAbstractButton
{
public:
    explicit CloseButton(QWidget *p) : QAbstractButton(p)
    {
        setFixedSize(46, 36);
        setAccessibleName(QObject::tr("Close"));
        setToolTip(QObject::tr("Close (Esc)"));
        setFocusPolicy(Qt::TabFocus);
        setCursor(Qt::ArrowCursor);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const bool hot = underMouse() || hasFocus();
        if (hot)
            p.fillRect(rect(), Theme::alert());
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(hot ? Theme::ink() : Theme::paper(), 1.2));
        const QPoint c = rect().center();
        p.drawLine(c.x() - 5, c.y() - 5, c.x() + 5, c.y() + 5);
        p.drawLine(c.x() - 5, c.y() + 5, c.x() + 5, c.y() - 5);
    }
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }
};

// Drags the whole dialog; the Close button is a child, so it receives its own clicks.
class TitleBar : public QWidget
{
public:
    explicit TitleBar(QWidget *p) : QWidget(p) { setFixedHeight(36); }
protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && window()->windowHandle())
            window()->windowHandle()->startSystemMove();
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), Theme::ink());
        p.fillRect(QRect(0, height() - 1, width(), 1), Theme::line());
    }
};
}  // namespace

FramelessDialog::FramelessDialog(const QString &title, QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
{
    setWindowTitle(title);  // still used by the taskbar / accessibility
    setModal(true);
    // Neutral 1px focus anchor, first in the tab order: a freshly opened dialog focuses this instead of an
    // action button (Close, Done, a tile), so a stray Enter / Space activates nothing. Tab moves on.
    anchor_ = new QWidget(this);
    anchor_->setGeometry(0, 0, 1, 1);
    anchor_->setFocusPolicy(Qt::StrongFocus);
    anchor_->setAccessibleName(title);
    anchor_->setFocus(Qt::OtherFocusReason);
    setStyleSheet(QStringLiteral("QDialog{background:#0D0C0B;}"));

    titleBar_ = new TitleBar(this);
    auto *caption = new QLabel(title.toUpper(), titleBar_);
    caption->setFont(Theme::display(8, QFont::Bold, 3));
    caption->setStyleSheet(QStringLiteral("color:#9A9186;background:transparent;"));
    caption->setAttribute(Qt::WA_TransparentForMouseEvents);
    close_ = new CloseButton(titleBar_);
    connect(close_, &QAbstractButton::clicked, this, &QDialog::reject);
    auto *bar = new QHBoxLayout(titleBar_);
    bar->setContentsMargins(18, 0, 0, 0);
    bar->setSpacing(0);
    bar->addWidget(caption, 1);
    bar->addWidget(close_);

    auto *body = new QWidget(this);
    bodyLayout_ = new QVBoxLayout(body);
    bodyLayout_->setContentsMargins(32, 24, 32, 22);
    bodyLayout_->setSpacing(10);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(1, 1, 1, 1);  // 1px border painted in paintEvent
    outer->setSpacing(0);
    outer->addWidget(titleBar_);
    outer->addWidget(body, 1);
}

void FramelessDialog::paintEvent(QPaintEvent *e)
{
    QDialog::paintEvent(e);
    QPainter p(this);
    p.setPen(Theme::line());
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void FramelessDialog::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);
    placeOnScreen();
}

// QDialog focuses the first focusable child on show, which may be an action button. Opening a dialog
// must never leave a button armed, so button focus is dropped; text fields keep theirs.
void FramelessDialog::setVisible(bool visible)
{
    QDialog::setVisible(visible);
    if (visible && qobject_cast<QAbstractButton *>(focusWidget()))
        anchor_->setFocus(Qt::OtherFocusReason);
}

void FramelessDialog::placeOnScreen()
{
    QWidget *host = parentWidget() ? parentWidget()->window() : nullptr;
    const bool hostUsable = host && host->isVisible() && !host->isMinimized();
    QScreen *screen = hostUsable ? host->screen() : QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect avail = screen->availableGeometry().adjusted(kScreenMargin, kScreenMargin, -kScreenMargin, -kScreenMargin);

    // Never bigger than the usable desktop (small screens / high display scaling).
    const QSize cap = avail.size();
    setMinimumSize(minimumSize().boundedTo(cap));
    QSize s = size().expandedTo(minimumSizeHint().boundedTo(cap)).boundedTo(cap);
    if (s != size())
        resize(s);

    const QPoint centre = hostUsable ? host->frameGeometry().center() : screen->availableGeometry().center();
    QPoint tl(centre.x() - s.width() / 2, centre.y() - s.height() / 2);
    tl.setX(qBound(avail.left(), tl.x(), qMax(avail.left(), avail.right() - s.width() + 1)));
    tl.setY(qBound(avail.top(), tl.y(), qMax(avail.top(), avail.bottom() - s.height() + 1)));
    move(tl);
}
