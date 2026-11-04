#include "sectioncard.h"

#include <QLabel>
#include <QVBoxLayout>

// No box: a caps caption with a hairline above it; content sits directly on the stage.
SectionCard::SectionCard(const QString &title, const QString &description, QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("card"));
    setStyleSheet(QStringLiteral("QFrame#card { border: none; border-top: 1px solid #2A2622; }"));
    auto *heading = new QLabel(title.toUpper(), this);
    heading->setProperty("role", QStringLiteral("caption"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 12, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(heading);
    if (!description.isEmpty()) {
        auto *desc = new QLabel(description, this);
        desc->setProperty("role", QStringLiteral("muted"));
        desc->setWordWrap(true);
        layout->addWidget(desc);
    }
    body_ = new QVBoxLayout;
    body_->setContentsMargins(0, 4, 0, 0);
    body_->setSpacing(10);
    layout->addLayout(body_);
}
