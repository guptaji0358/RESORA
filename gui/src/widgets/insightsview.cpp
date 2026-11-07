#include "widgets/insightsview.h"

#include <QLabel>
#include <QVBoxLayout>

#include "widgets/aspectindicator.h"
#include "widgets/collapsiblesection.h"
#include "widgets/dimensiondiagram.h"
#include "widgets/filesizebars.h"
#include "widgets/metadatacompare.h"
#include "widgets/resolutionchart.h"
#include "widgets/sectioncard.h"

namespace {
QLabel *label(const QString &t, const QString &style)
{
    auto *l = new QLabel(t);
    l->setWordWrap(true);
    l->setStyleSheet(style);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return l;
}
}  // namespace

InsightsView::InsightsView(const Comparison &c, QWidget *parent) : QWidget(parent)
{
    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(14);
    const InsightMetrics &m = c.metrics;

    auto *verdict = new SectionCard(tr("INSIGHTS"), tr("Quick verdict from the measured differences; not a quality judgement."));
    for (const QString &s : m.verdict) {
        verdict->body()->addWidget(label(QStringLiteral("•  ") + s, QStringLiteral("color:#EFE9DD; font-size:13px;")));
        ++verdicts_;
    }
    col->addWidget(verdict);

    auto addVisual = [&](const QString &title, const QString &hint, QWidget *w) {
        auto *card = new SectionCard(title, hint);
        card->body()->addWidget(w);
        col->addWidget(card);
    };
    resolution_ = new ResolutionChart;
    resolution_->setMetrics(m);
    addVisual(tr("Resolution"), tr("Megapixels, side by side."), resolution_);

    fileSize_ = new FileSizeBars;
    fileSize_->setMetrics(m);
    addVisual(tr("File size"), tr("Bytes on disk, side by side."), fileSize_);

    dimensions_ = new DimensionDiagram;
    dimensions_->setMetrics(m);
    addVisual(tr("Dimensions"), tr("Width × height at true proportions."), dimensions_);

    aspect_ = new AspectIndicator;
    aspect_->setMetrics(m);
    addVisual(tr("Aspect ratio"), tr("Shape of each image."), aspect_);

    metadata_ = new MetadataCompare;
    metadata_->setMetrics(m);
    addVisual(tr("Technical metadata"), tr("%n difference(s) highlighted. Unavailable metadata is not a difference when both lack it.", nullptr,
                                           [&] { int d = 0; for (const MetaRow &r : m.meta) d += r.differs ? 1 : 0; return d; }()),
              metadata_);

    // Existing written explanations: secondary, grouped, collapsed by default.
    explanations_ = new CollapsibleSection(tr("Detailed explanations"));
    QString section;
    for (const InsightNote &n : c.notes) {
        if (n.section != section) {
            section = n.section;
            explanations_->body()->addWidget(label(section.toUpper(), QStringLiteral("color:#9A9186; letter-spacing:2px; font-size:10px;")));
        }
        explanations_->body()->addWidget(label(n.text, QStringLiteral("color:#EFE9DD;")));
    }
    col->addWidget(explanations_);
}
