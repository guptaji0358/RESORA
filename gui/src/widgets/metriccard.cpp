#include "metriccard.h"

#include <QLabel>
#include <QVBoxLayout>

#include "ui/theme.h"
#include "ui/uiformat.h"

// Flat readout: a big numeral over a caps caption; separation comes from a left rule, not a box.
MetricCard::MetricCard(const QString &caption, QWidget *parent)
    : QFrame(parent), value_(new QLabel(QStringLiteral("0"), this))
{
    setObjectName(QStringLiteral("metric"));
    setStyleSheet(QStringLiteral("QFrame#metric { border: none; border-left: 1px solid #2A2622; }"));
    value_->setProperty("role", QStringLiteral("metricValue"));
    auto *cap = new QLabel(caption.toUpper(), this);
    cap->setProperty("role", QStringLiteral("caption"));
    cap->setWordWrap(true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 0, 8, 0);
    layout->setSpacing(0);
    layout->addWidget(value_);
    layout->addWidget(cap);
    setMinimumWidth(110);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void MetricCard::setValue(qint64 value, Tone tone)
{
    value_->setText(UiFormat::count(value));
    Theme::setProperty(value_, "tone",
                       tone == Tone::Good ? QStringLiteral("good")
                       : tone == Tone::Bad ? QStringLiteral("bad") : QStringLiteral("neutral"));
}

void MetricCard::setText(const QString &text, Tone tone, bool compact, const QString &hint)
{
    value_->setText(text);
    Theme::setProperty(value_, "role", compact ? QStringLiteral("metricValueSm") : QStringLiteral("metricValue"));
    Theme::setProperty(value_, "tone",
                       tone == Tone::Good ? QStringLiteral("good")
                       : tone == Tone::Bad ? QStringLiteral("bad") : QStringLiteral("neutral"));
    if (!hint.isEmpty())
        setToolTip(hint);
    setAccessibleDescription(text);
}

QString MetricCard::valueText() const
{
    return value_->text();
}
