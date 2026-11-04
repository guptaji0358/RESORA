#pragma once
// Titled card: heading + optional description + a body layout for the section's content.
#include <QFrame>

class QVBoxLayout;

class SectionCard : public QFrame
{
    Q_OBJECT
public:
    explicit SectionCard(const QString &title, const QString &description = QString(),
                         QWidget *parent = nullptr);

    QVBoxLayout *body() const { return body_; }

private:
    QVBoxLayout *body_;
};
