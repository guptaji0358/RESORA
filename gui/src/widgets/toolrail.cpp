#include "widgets/toolrail.h"

#include <QAbstractButton>
#include <QPainter>
#include <QVBoxLayout>

#include "ui/icons.h"
#include "ui/theme.h"

namespace {
const char *const kGlyph[4] = {"image", "scan", "swap", "images"};

class ToolTab : public QAbstractButton
{
public:
    ToolTab(int index, const QString &label, QWidget *p) : QAbstractButton(p), glyph_(QString::fromLatin1(kGlyph[index])), label_(label)
    {
        setCheckable(true);
        setAutoExclusive(false);
        setFixedHeight(88);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(tr("Tool: %1").arg(label));
        setToolTip(tr("%1 (Ctrl+%2)").arg(label).arg(index + 1));
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const bool on = isChecked();
        const QColor c = on ? Theme::ember() : (underMouse() ? Theme::paper() : Theme::mute());
        if (on) {
            p.fillRect(QRect(0, 8, 3, height() - 16), Theme::ember());
            p.fillRect(QRect(3, 0, width() - 3, height()), QColor(255, 91, 46, 14));
        }
        const qreal dpr = devicePixelRatioF();
        const QPixmap icon = Icons::pixmap(glyph_, 26, c, on ? Theme::ember() : c, dpr);
        p.drawPixmap((width() - 26) / 2, 16, icon);
        p.setPen(c);
        p.setFont(Theme::display(7, QFont::DemiBold, 1.5));
        p.drawText(QRect(0, 52, width(), 20), Qt::AlignHCenter | Qt::AlignVCenter, label_.toUpper());
        if (hasFocus()) {
            p.setPen(QPen(Theme::ember(), 1, Qt::DotLine));
            p.drawRect(rect().adjusted(8, 4, -6, -4));
        }
    }
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }
private:
    QString glyph_, label_;
};
}  // namespace

QString ToolRail::toolName(Tool t)
{
    switch (t) {
    case Tool::Analyzer: return tr("Analyzer");
    case Tool::Analytics: return tr("Analytics");
    case Tool::Compare: return tr("Compare");
    case Tool::Duplicates: return tr("Duplicates");
    }
    return QString();
}

ToolRail::ToolRail(QWidget *parent) : QWidget(parent)
{
    setFixedWidth(100);
    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(0, 18, 0, 0);
    l->setSpacing(0);
    for (int i = 0; i < 4; ++i) {
        tabs_[i] = new ToolTab(i, toolName(Tool(i)), this);
        l->addWidget(tabs_[i]);
        connect(tabs_[i], &QAbstractButton::clicked, this, [this, i] {
            setCurrent(Tool(i));
            emit toolRequested(i);
        });
    }
    l->addStretch(1);
    setCurrent(Tool::Analyzer);
}

void ToolRail::setCurrent(Tool t)
{
    current_ = t;
    for (int i = 0; i < 4; ++i)
        tabs_[i]->setChecked(i == int(t));
}

void ToolRail::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Theme::ink());
    p.fillRect(QRect(width() - 1, 0, 1, height()), Theme::line());
}
