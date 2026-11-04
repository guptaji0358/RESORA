#pragma once
// Custom title strip for the frameless window: drag area, painted mark + wordmark, state readout
// and custom-painted minimise / maximise / close buttons.
#include <QWidget>

class QLabel;

class AppHeader : public QWidget
{
    Q_OBJECT
public:
    explicit AppHeader(QWidget *parent = nullptr);
    void setState(const QString &text, const QString &state);   // state: idle/running/succeeded/failed/...
    QString stateText() const;

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    void toggleMaximized();
    QLabel *chip_;
};
