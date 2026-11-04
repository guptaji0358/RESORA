#include "phaserail.h"

#include <QAbstractButton>
#include <QPainter>
#include <QVBoxLayout>

#include "ui/theme.h"

namespace {
class PhaseTab : public QAbstractButton
{
public:
    PhaseTab(int number, const QString &label, QWidget *p) : QAbstractButton(p), n_(number), label_(label)
    {
        setCheckable(true);
        setAutoExclusive(false);
        setFixedHeight(92);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(tr("Phase %1: %2").arg(number).arg(label));
        setToolTip(tr("%1 (Ctrl+%2)").arg(label).arg(number));
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const bool on = isChecked(), en = isEnabled();
        const QColor c = !en ? QColor(0x4a, 0x44, 0x3d) : (on ? Theme::ember() : (underMouse() ? Theme::paper() : Theme::mute()));
        if (on) {
            p.fillRect(QRect(0, 8, 3, height() - 16), Theme::ember());
            p.fillRect(QRect(3, 0, width() - 3, height()), QColor(255, 91, 46, 14));
        }
        p.setPen(c);
        p.setFont(Theme::display(24, QFont::DemiBold, 1));
        p.drawText(QRect(0, 14, width(), 36), Qt::AlignHCenter | Qt::AlignVCenter, QStringLiteral("%1").arg(n_, 2, 10, QLatin1Char('0')));
        p.setFont(Theme::display(7.5, QFont::DemiBold, 2.5));
        p.drawText(QRect(0, 52, width(), 20), Qt::AlignHCenter | Qt::AlignVCenter, label_.toUpper());
        if (hasFocus()) {
            p.setPen(QPen(Theme::ember(), 1, Qt::DotLine));
            p.drawRect(rect().adjusted(8, 4, -6, -4));
        }
    }
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }
private:
    int n_;
    QString label_;
};
}  // namespace

PhaseRail::PhaseRail(QWidget *parent) : QWidget(parent)
{
    setFixedWidth(92);
    const QString names[] = {tr("Source"), tr("Scan"), tr("Readout")};
    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(0, 18, 0, 0);
    l->setSpacing(0);
    for (int i = 0; i < 3; ++i) {
        tabs_[i] = new PhaseTab(i + 1, names[i], this);
        l->addWidget(tabs_[i]);
        connect(tabs_[i], &QAbstractButton::clicked, this, [this, i] {
            setCurrent(Phase(i));
            emit phaseRequested(i);
        });
    }
    l->addStretch(1);
    setAvailable(Scan, false);
    setAvailable(Readout, false);
    setCurrent(Source);
}

void PhaseRail::setCurrent(Phase p)
{
    current_ = p;
    for (int i = 0; i < 3; ++i)
        tabs_[i]->setChecked(i == p);
}

void PhaseRail::setAvailable(Phase p, bool available)
{
    tabs_[p]->setEnabled(available);
}

void PhaseRail::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Theme::ink());
    p.fillRect(QRect(width() - 1, 0, 1, height()), Theme::line());
}

bool PhaseRail::isPhaseAvailable(Phase p) const
{
    return tabs_[p]->isEnabled();
}
