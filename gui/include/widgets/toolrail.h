#pragma once
// Tool rail: the four RESORA tools (Analyzer, Analytics, Compare, Duplicates) as equal, always
// reachable, custom-painted entries (Ctrl+1..4). Nothing is ever disabled: each tool is usable first.
#include <QWidget>

#include "ui/toolrequest.h"

class QAbstractButton;

class ToolRail : public QWidget
{
    Q_OBJECT
public:
    explicit ToolRail(QWidget *parent = nullptr);

    void setCurrent(Tool t);
    Tool current() const { return current_; }
    static QString toolName(Tool t);

signals:
    void toolRequested(int tool);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QAbstractButton *tabs_[4];
    Tool current_ = Tool::Analyzer;
};
