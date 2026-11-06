#pragma once
// One headline number with a caption (e.g. "Analyzed images").
#include <QFrame>

class QLabel;

class MetricCard : public QFrame
{
    Q_OBJECT
public:
    explicit MetricCard(const QString &caption, QWidget *parent = nullptr);

    enum class Tone { Neutral, Good, Bad };
    void setValue(qint64 value, Tone tone = Tone::Neutral);
    // Free-form value (e.g. "4.28 GB", "1,920 × 1,080"); `compact` uses the smaller numeral for long text.
    // `hint` becomes the tooltip (definitions); empty leaves it unchanged.
    void setText(const QString &text, Tone tone = Tone::Neutral, bool compact = false, const QString &hint = QString());
    QString valueText() const;

private:
    QLabel *value_;
};
