#pragma once
// Custom title strip for the frameless window: drag area, painted mark + wordmark, state readout
// and the tool navigation (Analyzer/Analytics/Compare/Duplicates) and custom-painted minimise / maximise / close buttons.
#include <QWidget>

class QLabel;
class ToolNav;

class AppHeader : public QWidget
{
    Q_OBJECT
public:
    explicit AppHeader(QWidget *parent = nullptr);
    void setState(const QString &text, const QString &state);   // state: idle/running/succeeded/failed/...
    QString stateText() const;
    ToolNav *toolNav() const { return nav_; }

signals:
    void settingsRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    void toggleMaximized();
    QLabel *chip_;
    ToolNav *nav_;
};
