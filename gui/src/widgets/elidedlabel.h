#pragma once
// Single-line label that middle-elides long text (paths) and keeps the full text as tooltip.
#include <QLabel>

class ElidedLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ElidedLabel(QWidget *parent = nullptr);

    void setFullText(const QString &text);
    QString fullText() const { return full_; }

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void applyElision();

    QString full_;
};
