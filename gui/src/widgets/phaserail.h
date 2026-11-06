#pragma once
// Navigation rail: four numbered phases (Source, Scan, Readout, Analytics) as custom-painted tabs.
// The app's workspace is a single stage that moves through these phases; the rail shows where
// you are and lets you go back to any phase that is available.
#include <QWidget>

class QAbstractButton;

class PhaseRail : public QWidget
{
    Q_OBJECT
public:
    enum Phase { Source = 0, Scan = 1, Readout = 2, Analytics = 3 };
    explicit PhaseRail(QWidget *parent = nullptr);

    void setCurrent(Phase p);
    void setAvailable(Phase p, bool available);
    bool isPhaseAvailable(Phase p) const;
    Phase current() const { return current_; }

signals:
    void phaseRequested(int phase);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QAbstractButton *tabs_[4];
    Phase current_ = Source;
};
