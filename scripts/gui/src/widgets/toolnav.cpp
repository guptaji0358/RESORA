#include "widgets/toolnav.h"

#include <QAbstractButton>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QPainter>

#include "ui/icons.h"
#include "ui/theme.h"

namespace {
const char *const kGlyph[4] = {"image", "scan", "swap", "images"};
constexpr int kIcon = 16;
constexpr int kGap = 7;
constexpr int kPad = 12;

class NavTab : public QAbstractButton
{
public:
    NavTab(int index, const QString &label, QWidget *p) : QAbstractButton(p), glyph_(QString::fromLatin1(kGlyph[index])), label_(label.toUpper())
    {
        setCheckable(true);
        setAutoExclusive(false);
        setFixedHeight(36);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(tr("Tool: %1").arg(label));
        setToolTip(tr("%1 (Ctrl+%2)").arg(label).arg(index + 1));
        setFont(Theme::display(8, QFont::DemiBold, 1.5));
        applySize();
    }
    void setCompact(bool c)
    {
        compact_ = c;
        applySize();
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const bool on = isChecked();
        const bool hot = underMouse() || hasFocus();
        const QColor c = on ? Theme::ember() : (hot ? Theme::paper() : Theme::mute());
        if (on)
            p.fillRect(rect(), QColor(255, 91, 46, 20));
        else if (hot)
            p.fillRect(rect(), QColor(255, 255, 255, 10));
        if (on)
            p.fillRect(QRect(0, height() - 3, width(), 3), Theme::ember());
        const int tw = compact_ ? 0 : textWidth();
        const int total = kIcon + (compact_ ? 0 : kGap + tw);
        const int x = (width() - total) / 2;
        p.drawPixmap(x, (height() - kIcon) / 2, Icons::pixmap(glyph_, kIcon, c, on ? Theme::ember() : c, devicePixelRatioF()));
        if (!compact_) {
            p.setPen(c);
            p.setFont(font());
            p.drawText(QRect(x + kIcon + kGap, 0, tw + 4, height()), Qt::AlignVCenter | Qt::AlignLeft, label_);
        }
    }
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }
private:
    int textWidth() const { return QFontMetrics(font()).horizontalAdvance(label_) + 2 * label_.size(); }
    void applySize() { setFixedWidth(compact_ ? kIcon + 2 * kPad : kIcon + kGap + textWidth() + 2 * kPad); }
    QString glyph_, label_;
    bool compact_ = false;
};
}  // namespace

QString ToolNav::toolName(Tool t)
{
    switch (t) {
    case Tool::Analyzer: return tr("Analyzer");
    case Tool::Analytics: return tr("Analytics");
    case Tool::Compare: return tr("Compare");
    case Tool::Duplicates: return tr("Duplicates");
    }
    return QString();
}

ToolNav::ToolNav(QWidget *parent) : QWidget(parent)
{
    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(2);
    for (int i = 0; i < 4; ++i) {
        tabs_[i] = new NavTab(i, toolName(Tool(i)), this);
        l->addWidget(tabs_[i]);
        connect(tabs_[i], &QAbstractButton::clicked, this, [this, i] {
            setCurrent(Tool(i));
            emit toolRequested(i);
        });
    }
    setCurrent(Tool::Analyzer);
}

void ToolNav::setCurrent(Tool t)
{
    current_ = t;
    for (int i = 0; i < 4; ++i)
        tabs_[i]->setChecked(i == int(t));
}

void ToolNav::setCompact(bool compact)
{
    if (compact == compact_)
        return;
    compact_ = compact;
    for (auto *t : tabs_)
        static_cast<NavTab *>(t)->setCompact(compact);
}
