#include "widgets/confirmdialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/theme.h"
#include "widgets/chamferbutton.h"

ConfirmDialog::ConfirmDialog(const QString &title, const QString &text, const QString &confirmLabel, bool danger,
                             QWidget *parent)
    : FramelessDialog(title, parent)
{
    setObjectName(QStringLiteral("confirmDialog"));
    setMinimumWidth(440);
    auto *head = new QLabel(title);
    head->setFont(Theme::display(16, QFont::DemiBold));
    head->setStyleSheet(QStringLiteral("color:#EFE9DD;"));
    auto *body = new QLabel(text);
    body->setWordWrap(true);
    body->setProperty("role", QStringLiteral("muted"));
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *cancel = new ChamferButton(tr("Cancel"));
    cancel->setObjectName(QStringLiteral("confirmCancel"));
    cancel->setAutoDefault(false);
    auto *ok = new ChamferButton(confirmLabel);
    ok->setObjectName(QStringLiteral("confirmOk"));
    ok->setAutoDefault(false);
    Theme::setProperty(ok, "variant", danger ? QStringLiteral("danger") : QStringLiteral("primary"));
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    auto *row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(cancel);
    row->addWidget(ok);
    bodyLayout()->addWidget(head);
    bodyLayout()->addWidget(body);
    bodyLayout()->addSpacing(8);
    bodyLayout()->addLayout(row);
    cancel->setFocus();  // the safe answer is the one that is focused
}

bool ConfirmDialog::ask(QWidget *parent, const QString &title, const QString &text, const QString &confirmLabel,
                        bool danger)
{
    ConfirmDialog d(title, text, confirmLabel, danger, parent);
    return d.exec() == QDialog::Accepted;
}
