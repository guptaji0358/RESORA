#include "analyticsdashboard.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "backend/dashboardmodel.h"
#include "barchart.h"
#include "donutchart.h"
#include "hbarchart.h"
#include "metriccard.h"
#include "pixelspan.h"
#include "sectioncard.h"
#include "ui/theme.h"
#include "ui/uiformat.h"

namespace {
QLabel *label(const char *role, QWidget *parent)
{
    auto *l = new QLabel(parent);
    l->setProperty("role", QString::fromLatin1(role));
    l->setWordWrap(true);
    return l;
}
}  // namespace

// Readout under a chart: the hint until a category is selected, then that category's statistics.
QLabel *AnalyticsDashboard::readoutFor(ChartBase *chart, const QString &hint)
{
    auto *l = label("muted", this);
    l->setText(hint);
    connect(chart, &ChartBase::selectionChanged, l, [chart, l, hint](int i) {
        l->setText(i >= 0 ? chart->items()[i].detail : hint);
    });
    return l;
}

AnalyticsDashboard::AnalyticsDashboard(QWidget *parent) : QWidget(parent)
{
    const QString hint = tr("Select a category to see its statistics.");
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(18);
    const char *keys[6] = {"dash.total", "dash.analyzed", "dash.failed", "dash.storage", "dash.avgmp", "dash.avgres"};
    const QString caps[6] = {tr("Total images"), tr("Analyzed"), tr("Failed"), tr("Storage used"), tr("Average megapixels"), tr("Average resolution")};
    for (int i = 0; i < 6; ++i) {
        tiles_[i] = new MetricCard(caps[i], this);
        tiles_[i]->setObjectName(QString::fromLatin1(keys[i]));
        grid->addWidget(tiles_[i], i / 3, i % 3);
    }
    health_ = label("muted", this);

    auto *resCard = new SectionCard(tr("Resolution distribution"), tr("Images per resolution class (bars) with their share of analyzed images."), this);
    categories_ = new BarChart(tr("Resolution distribution"), this);
    categories_->setEmptyText(tr("No analyzed images, so there is no resolution distribution."));
    resCard->body()->addWidget(categories_);
    resCard->body()->addWidget(readoutFor(categories_, hint));

    auto *orCard = new SectionCard(tr("Orientation"), QString(), this);
    orientation_ = new DonutChart(tr("Orientation"), this);
    orientation_->setEmptyText(tr("No analyzed images."));
    orCard->body()->addWidget(orientation_);
    orCard->body()->addWidget(readoutFor(orientation_, hint));
    auto *fmtCard = new SectionCard(tr("Image formats"), QString(), this);
    formats_ = new DonutChart(tr("Image formats"), this);
    formats_->setEmptyText(tr("No analyzed images."));
    fmtCard->body()->addWidget(formats_);
    fmtCard->body()->addWidget(readoutFor(formats_, hint));
    auto *donuts = new QHBoxLayout;
    donuts->setSpacing(24);
    donuts->addWidget(orCard, 1);
    donuts->addWidget(fmtCard, 1);

    auto *stCard = new SectionCard(tr("Storage by format"), tr("Bar: share of storage. Tick: share of images."), this);
    storage_ = new HBarChart(tr("Storage by format"), this);
    storage_->setEmptyText(tr("File sizes are not available for the analyzed images."));
    stCard->body()->addWidget(storage_);
    stCard->body()->addWidget(readoutFor(storage_, hint));

    auto *spanCard = new SectionCard(tr("Megapixel span"), tr("Lowest to highest image on a logarithmic ruler, with the average."), this);
    span_ = new PixelSpan(this);
    spanCard->body()->addWidget(span_);

    auto *inCard = new SectionCard(tr("Insights"), QString(), this);
    insights_ = label("", this);
    insights_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    inCard->body()->addWidget(insights_);

    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(18);
    l->addLayout(grid);
    l->addWidget(health_);
    l->addWidget(resCard);
    l->addLayout(donuts);
    l->addWidget(stCard);
    l->addWidget(spanCard);
    l->addWidget(inCard);
    l->addStretch(1);
    clear();
}

void AnalyticsDashboard::setData(const AnalyticsData &d)
{
    const QString na = AnalyticsModel::notAvailable();
    auto set = [this](int i, const QString &t, MetricCard::Tone tone = MetricCard::Tone::Neutral) {
        tiles_[i]->setText(t, tone, true);
    };
    set(0, UiFormat::count(d.total));
    set(1, UiFormat::count(d.analyzed), d.analyzed > 0 ? MetricCard::Tone::Good : MetricCard::Tone::Neutral);
    set(2, UiFormat::count(d.failed), d.failed > 0 ? MetricCard::Tone::Bad : MetricCard::Tone::Neutral);
    set(3, d.sizeKnown > 0 ? AnalyticsModel::bytesText(double(d.totalBytes)) : na);
    set(4, AnalyticsModel::megapixelsText(d.averageMegapixels));
    set(5, d.averageWidth < 0 ? na : QStringLiteral("%1 × %2").arg(d.averageWidth, 0, 'f', 0).arg(d.averageHeight, 0, 'f', 0));
    health_->setText(!d.valid ? QString()
                              : d.failed > 0 ? tr("%1 of %2 images analyzed, %3 failed. Charts cover the analyzed images only.")
                                                   .arg(UiFormat::count(d.analyzed), UiFormat::count(d.total), UiFormat::count(d.failed))
                                             : tr("All %1 images were analyzed.").arg(UiFormat::count(d.total)));
    categories_->setItems(DashboardModel::categoryItems(d));
    orientation_->setItems(DashboardModel::orientationItems(d));
    formats_->setItems(DashboardModel::formatItems(d));
    storage_->setItems(DashboardModel::storageItems(d));
    span_->setValues(d.lowest.valid ? d.lowest.megapixels : 0, d.averageMegapixels, d.highest.valid ? d.highest.megapixels : 0);
    insights_->setText(d.valid ? QStringLiteral("• ") + DashboardModel::insights(d).join(QStringLiteral("\n• ")) : QString());
}
