#include "metadatacompare.h"

#include <QGridLayout>
#include <QLabel>

#include "insightcanvas.h"
#include "ui/theme.h"

namespace {
QLabel *cell(const QString &t, const QString &style)
{
    auto *l = new QLabel(t);
    l->setWordWrap(true);
    l->setStyleSheet(style);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    l->setToolTip(t);
    return l;
}
}  // namespace

MetadataCompare::MetadataCompare(QWidget *parent) : QWidget(parent)
{
    setAccessibleName(tr("Technical metadata comparison"));
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(6);
    grid->setColumnMinimumWidth(0, 96);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
}

void MetadataCompare::setMetrics(const InsightMetrics &m)
{
    auto *grid = static_cast<QGridLayout *>(layout());
    while (QLayoutItem *item = grid->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    rows_ = differing_ = 0;
    const QString dim = QStringLiteral("color:#9A9186;"), paper = QStringLiteral("color:#EFE9DD;"),
                  hot = QStringLiteral("color:#FF5B2E; font-weight:600;"), muted = QStringLiteral("color:#9A9186; font-style:italic;");
    grid->addWidget(cell(QString(), dim), 0, 0);
    grid->addWidget(cell(tr("A"), QStringLiteral("color:%1; font-weight:700;").arg(InsightCanvas::colorA().name())), 0, 1);
    grid->addWidget(cell(tr("B"), QStringLiteral("color:%1; font-weight:700;").arg(InsightCanvas::colorB().name())), 0, 2);
    int r = 1;
    for (const MetaRow &row : m.meta) {
        grid->addWidget(cell(row.differs ? QStringLiteral("▸ ") + row.label : row.label, row.differs ? hot : dim), r, 0);
        grid->addWidget(cell(row.a, !row.aAvailable ? muted : (row.differs ? hot : paper)), r, 1);
        grid->addWidget(cell(row.b, !row.bAvailable ? muted : (row.differs ? hot : paper)), r, 2);
        ++rows_;
        differing_ += row.differs ? 1 : 0;
        ++r;
    }
}
