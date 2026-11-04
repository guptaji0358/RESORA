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
    QString valueText() const;

private:
    QLabel *value_;
};
