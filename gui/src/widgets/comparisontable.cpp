#include "comparisontable.h"

#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "insightsview.h"
#include "sectioncard.h"
#include "ui/theme.h"

namespace {
QLabel *text(const QString &t, const QString &style, bool wrap = true)
{
    auto *l = new QLabel(t);
    l->setWordWrap(wrap);
    l->setStyleSheet(style);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    if (t.size() > 40)
        l->setToolTip(t);
    return l;
}
}  // namespace

ComparisonTable::ComparisonTable(QWidget *parent) : QWidget(parent)
{
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(14);
    setMessage(tr("Choose two images to compare their properties."));
}

void ComparisonTable::clear()
{
    rows_ = highlighted_ = 0;
    insights_ = nullptr;
    while (QLayoutItem *item = layout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

void ComparisonTable::setMessage(const QString &t, bool error)
{
    clear();
    auto *l = new QLabel(t);
    l->setWordWrap(true);
    l->setProperty("role", error ? QStringLiteral("error") : QStringLiteral("muted"));
    layout_->addWidget(l);
    layout_->addStretch(1);
}

void ComparisonTable::setComparison(const Comparison &c)
{
    clear();
    if (!c.complete) {
        QStringList problems;
        for (const ImageProperties *p : {&c.left, &c.right})
            if (!p->ok())
                problems << tr("%1: %2 (%3)").arg(p->name.isEmpty() ? tr("Image") : p->name, p->status, p->error);
        setMessage(tr("Cannot compare properties.\n%1").arg(problems.join(QLatin1Char('\n'))), true);
        return;
    }
    const QString paper = QStringLiteral("color:#EFE9DD;"), muted = QStringLiteral("color:#9A9186; font-style:italic;"),
                  hot = QStringLiteral("color:#FF5B2E; font-weight:600;"), dim = QStringLiteral("color:#9A9186;");

    insights_ = new InsightsView(c);
    layout_->addWidget(insights_);

    auto *card = new SectionCard(tr("PROPERTIES"),
                                 tr("%n difference(s) highlighted. Missing metadata is not a difference between two images that both lack it.", nullptr,
                                    c.differenceCount()));
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(7);
    grid->setColumnMinimumWidth(0, 130);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    grid->addWidget(text(QString(), dim), 0, 0);
    grid->addWidget(text(tr("A  ·  %1").arg(c.left.name), QStringLiteral("color:#EFE9DD; font-weight:600;")), 0, 1);
    grid->addWidget(text(tr("B  ·  %1").arg(c.right.name), QStringLiteral("color:#EFE9DD; font-weight:600;")), 0, 2);
    int r = 1;
    QString section;
    for (const CompareRow &row : c.rows) {
        if (row.section != section) {
            section = row.section;
            auto *h = text(section, QStringLiteral("color:#9A9186; letter-spacing:2px; font-size:10px;"), false);
            grid->addWidget(h, r++, 0, 1, 3);
        }
        const QString mark = row.differs ? QStringLiteral("color:#FF5B2E;") : dim;
        grid->addWidget(text(row.differs ? QStringLiteral("▸ ") + row.label : row.label, mark), r, 0);
        grid->addWidget(text(row.left, row.leftAvailable ? (row.differs ? QStringLiteral("color:#FF5B2E; font-weight:600;") : paper) : muted), r, 1);
        grid->addWidget(text(row.right, row.rightAvailable ? (row.differs ? QStringLiteral("color:#FF5B2E; font-weight:600;") : paper) : muted), r, 2);
        ++rows_;
        highlighted_ += row.differs ? 1 : 0;
        ++r;
    }
    card->body()->addLayout(grid);
    layout_->addWidget(card);
    layout_->addStretch(1);
}
