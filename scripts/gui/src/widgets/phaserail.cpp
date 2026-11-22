#include "widgets/phaserail.h"

#include <QAbstractButton>
#include <QPainter>
#include <QHBoxLayout>

#include "ui/theme.h"

namespace {
class PhaseTab : public QAbstractButton
{
public:
    PhaseTab(int number, const QString &label, QWidget *p) : QAbstractButton(p), n_(number), label_(label)
    {
        setCheckable(true);
        setAutoExclusive(false);
        setFixedSize(118, 52);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(tr("Step %1: %2").arg(number).arg(label));
        setToolTip(tr("%1").arg(label));
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const bool on = isChecked(), en = isEnabled();
        const QColor c = !en ? QColor(0x4a, 0x44, 0x3d) : (on ? Theme::ember() : (underMouse() ? Theme::paper() : Theme::mute()));
        if (on) {
            p.fillRect(QRect(8, height() - 3, width() - 16, 3), Theme::ember());
            p.fillRect(QRect(0, 0, width(), height() - 3), QColor(255, 91, 46, 14));
        }
        p.setPen(c);
        p.setFont(Theme::display(15, QFont::DemiBold, 1));
        p.drawText(QRect(8, 0, 30, height() - 3), Qt::AlignHCenter | Qt::AlignVCenter, QStringLiteral("%1").arg(n_, 2, 10, QLatin1Char('0')));
        p.setFont(Theme::display(7, QFont::DemiBold, 2));
        p.drawText(QRect(40, 0, width() - 44, height() - 3), Qt::AlignLeft | Qt::AlignVCenter, label_.toUpper());
        if (hasFocus()) {
            p.setPen(QPen(Theme::ember(), 1, Qt::DotLine));
            p.drawRect(rect().adjusted(4, 4, -4, -6));
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
    setFixedHeight(54);
    const QString names[] = {tr("Source"), tr("Scan"), tr("Readout"), tr("Properties")};
    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(24, 0, 0, 0);
    l->setSpacing(0);
    for (int i = 0; i < 4; ++i) {
        tabs_[i] = new PhaseTab(i + 1, names[i], this);
        l->addWidget(tabs_[i]);
        connect(tabs_[i], &QAbstractButton::clicked, this, [this, i] {
            setCurrent(Phase(i));
            emit phaseRequested(i);
        });
    }
    l->addStretch(1);
    setAvailable(Scan, false);
    setAvailable(Readout, false);  // Properties is always available: it works on any image
    setCurrent(Source);
}

void PhaseRail::setCurrent(Phase p)
{
    current_ = p;
    for (int i = 0; i < 4; ++i)
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
    p.fillRect(QRect(0, height() - 1, width(), 1), Theme::line());
}

bool PhaseRail::isPhaseAvailable(Phase p) const
{
    return tabs_[p]->isEnabled();
}
