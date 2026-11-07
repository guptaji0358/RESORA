#include "widgets/appheader.h"

#include <QAbstractButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>

#include "ui/icons.h"
#include "ui/theme.h"
#include "widgets/brandlockup.h"

namespace {
// Painted caption button: kind 0 minimise, 1 maximise/restore, 2 close.
class CaptionButton : public QAbstractButton
{
public:
    CaptionButton(int kind, const QString &name, QWidget *p) : QAbstractButton(p), kind_(kind)
    {
        setFixedSize(46, 36);
        setAccessibleName(name);
        setToolTip(name);
        setFocusPolicy(Qt::TabFocus);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        if (underMouse() || hasFocus())
            p.fillRect(rect(), kind_ == 2 ? Theme::alert() : QColor(255, 255, 255, 22));
        p.setPen(QPen(underMouse() && kind_ == 2 ? Theme::ink() : Theme::paper(), 1.2));
        const QPoint c = rect().center();
        if (kind_ == 0) p.drawLine(c.x() - 5, c.y() + 2, c.x() + 5, c.y() + 2);
        else if (kind_ == 1) {
            if (window()->isMaximized()) {
                p.drawRect(QRect(c.x() - 4, c.y() - 2, 7, 7));
                p.drawLine(c.x() - 2, c.y() - 4, c.x() + 5, c.y() - 4);
                p.drawLine(c.x() + 5, c.y() - 4, c.x() + 5, c.y() + 3);
            } else p.drawRect(QRect(c.x() - 5, c.y() - 5, 10, 10));
        } else {
            p.setRenderHint(QPainter::Antialiasing);
            p.drawLine(c.x() - 5, c.y() - 5, c.x() + 5, c.y() + 5);
            p.drawLine(c.x() - 5, c.y() + 5, c.x() + 5, c.y() - 5);
        }
    }
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }
private:
    int kind_;
};

// Letter-spaced caps text button ("SETTINGS"); ember underline on hover / focus.
class HeaderLink : public QAbstractButton
{
public:
    HeaderLink(const QString &text, const QString &glyph, QWidget *p) : QAbstractButton(p), glyph_(glyph)
    {
        setText(text);
        setAccessibleName(text);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
        setFixedHeight(36);
        const QFont f = Theme::display(8, QFont::Bold, 3);
        setFont(f);
        setFixedWidth(QFontMetrics(f).horizontalAdvance(text) + 3 * text.size() + 28 + kIcon + kGap);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const bool hot = underMouse() || hasFocus();
        p.setFont(font());
        const QColor tone = hot ? Theme::paper() : Theme::mute();
        p.setPen(tone);
        const int tw = p.fontMetrics().horizontalAdvance(text()) + 3 * text().size();
        const int x = (width() - (kIcon + kGap + tw)) / 2;
        p.drawPixmap(x, (height() - kIcon) / 2 - 1, Icons::pixmap(glyph_, kIcon, tone, Theme::ember(), devicePixelRatioF()));
        p.drawText(QRect(x + kIcon + kGap, 0, tw + 4, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
        if (hot)
            p.fillRect(QRect(14, height() - 7, width() - 28, 2), Theme::ember());
    }
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }
private:
    static constexpr int kIcon = 16;
    static constexpr int kGap = 8;
    QString glyph_;
};

}  // namespace

AppHeader::AppHeader(QWidget *parent) : QWidget(parent), chip_(new QLabel(this))
{
    setObjectName(QStringLiteral("header"));
    setFixedHeight(48);
    chip_->setProperty("role", QStringLiteral("state"));
    chip_->setAccessibleName(tr("Application state"));
    auto *settings = new HeaderLink(tr("SETTINGS"), QStringLiteral("settings"), this);
    settings->setToolTip(tr("Settings: Saved Locations (Ctrl+,)"));
    connect(settings, &QAbstractButton::clicked, this, &AppHeader::settingsRequested);
    auto *min = new CaptionButton(0, tr("Minimize"), this);
    auto *max = new CaptionButton(1, tr("Maximize"), this);
    auto *close = new CaptionButton(2, tr("Close"), this);
    connect(min, &QAbstractButton::clicked, this, [this] { window()->showMinimized(); });
    connect(max, &QAbstractButton::clicked, this, [this, max] { toggleMaximized(); max->update(); });
    connect(close, &QAbstractButton::clicked, this, [this] { window()->close(); });

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(18, 0, 0, 0);
    row->setSpacing(12);
    row->addWidget(new BrandLockup(this));
    row->addStretch(1);
    row->addWidget(settings);
    row->addSpacing(8);
    row->addWidget(chip_);
    row->addSpacing(18);
    row->addWidget(min);
    row->addWidget(max);
    row->addWidget(close);
    setState(tr("Ready"), QStringLiteral("idle"));
}

void AppHeader::setState(const QString &text, const QString &state)
{
    chip_->setText(QStringLiteral("● ") + text.toUpper());
    chip_->setProperty("plain", text);
    Theme::setProperty(chip_, "state", state);
}

QString AppHeader::stateText() const
{
    return chip_->property("plain").toString();
}

void AppHeader::toggleMaximized()
{
    window()->isMaximized() ? window()->showNormal() : window()->showMaximized();
}

void AppHeader::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && window()->windowHandle())
        window()->windowHandle()->startSystemMove();
}

void AppHeader::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        toggleMaximized();
}

void AppHeader::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Theme::ink());
    p.fillRect(QRect(0, height() - 1, width(), 1), Theme::line());
}
