#pragma once
// Analyzer step strip: the Image Analyzer's own steps (Source, Scan, Readout, Properties) as a
// horizontal row of custom-painted tabs. This is NOT app navigation (that is ToolNav); it only
// moves inside the Analyzer workspace. Scan and Readout are enabled once reachable.
#include <QWidget>

class QAbstractButton;

class PhaseRail : public QWidget
{
    Q_OBJECT
public:
    enum Phase { Source = 0, Scan = 1, Readout = 2, Properties = 3 };
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
