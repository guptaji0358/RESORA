#include "widgets/collapsiblesection.h"

#include <QToolButton>
#include <QVBoxLayout>

CollapsibleSection::CollapsibleSection(const QString &title, QWidget *parent) : QWidget(parent), title_(title)
{
    header_ = new QToolButton;
    header_->setCheckable(true);
    header_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    header_->setCursor(Qt::PointingHandCursor);
    header_->setFocusPolicy(Qt::TabFocus);
    header_->setStyleSheet(QStringLiteral(
        "QToolButton { border: none; background: transparent; color: #9A9186; text-align: left; padding: 4px 0; letter-spacing: 1px; }"
        "QToolButton:hover { color: #EFE9DD; }"
        "QToolButton:focus { color: #FF5B2E; }"));
    header_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    content_ = new QWidget;
    body_ = new QVBoxLayout(content_);
    body_->setContentsMargins(14, 2, 0, 4);
    body_->setSpacing(6);

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);
    col->addWidget(header_);
    col->addWidget(content_);

    connect(header_, &QToolButton::toggled, this, [this](bool on) { setExpanded(on); });
    setExpanded(false);
}

bool CollapsibleSection::isExpanded() const
{
    return header_->isChecked();
}

void CollapsibleSection::setExpanded(bool on)
{
    if (header_->isChecked() != on) {
        header_->setChecked(on);  // re-enters through toggled()
        return;
    }
    content_->setVisible(on);
    header_->setText((on ? QStringLiteral("▾  ") : QStringLiteral("▸  ")) + title_.toUpper());
    header_->setAccessibleName(tr("%1, %2").arg(title_, on ? tr("expanded") : tr("collapsed")));
}
