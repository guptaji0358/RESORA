#pragma once
// Tool navigation: the four RESORA tools (Analyzer, Analytics, Compare, Duplicates) as a compact
// horizontal strip that lives in the title bar (Ctrl+1..4 are owned by MainWindow). Nothing is ever
// disabled. setCompact(true) drops the labels (icon only) for narrow windows.
#include <QWidget>

#include "ui/toolrequest.h"

class QAbstractButton;

class ToolNav : public QWidget
{
    Q_OBJECT
public:
    explicit ToolNav(QWidget *parent = nullptr);

    void setCurrent(Tool t);
    Tool current() const { return current_; }
    void setCompact(bool compact);
    bool isCompact() const { return compact_; }
    static QString toolName(Tool t);

signals:
    void toolRequested(int tool);

private:
    QAbstractButton *tabs_[4];
    Tool current_ = Tool::Analyzer;
    bool compact_ = false;
};
