#pragma once
// Header button + content that expands/collapses. Used for secondary explanations so the visual
// comparison stays primary. Keyboard accessible (the header is a checkable tool button).
#include <QWidget>

class QToolButton;
class QVBoxLayout;

class CollapsibleSection : public QWidget
{
    Q_OBJECT
public:
    explicit CollapsibleSection(const QString &title, QWidget *parent = nullptr);

    QVBoxLayout *body() const { return body_; }
    bool isExpanded() const;
    void setExpanded(bool on);

private:
    QToolButton *header_;
    QWidget *content_;
    QVBoxLayout *body_;
    QString title_;
};
