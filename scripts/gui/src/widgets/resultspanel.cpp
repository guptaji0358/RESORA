#include "widgets/resultspanel.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonValue>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include "widgets/chamferbutton.h"
#include <QUrl>
#include <QVBoxLayout>

#include "widgets/distributionbar.h"
#include "widgets/elidedlabel.h"
#include "widgets/metriccard.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/uiformat.h"

ResultsPanel::ResultsPanel(QWidget *parent)
    : QWidget(parent),
      emptyArt_(new QLabel(this)),
      empty_(new QLabel(tr("No scan results yet. Results appear here after a scan."), this)),
      content_(new QWidget(this)),
      total_(new MetricCard(tr("Files discovered"), content_)),
      analyzed_(new MetricCard(tr("Analyzed images"), content_)),
      failed_(new MetricCard(tr("Failed or unsupported"), content_)),
      skipped_(new QLabel(content_)),
      tableHeading_(new QLabel(tr("RESOLUTION LADDER"), content_)),
      byResolution_(new DistributionBar(content_))
{
    emptyArt_->setPixmap(Icons::artwork(QStringLiteral("illustrations/empty-results"), QSize(200, 125), devicePixelRatioF()));
    emptyArt_->setFixedSize(200, 125);
    emptyArt_->setAccessibleName(tr("Empty results illustration"));
    empty_->setProperty("role", QStringLiteral("muted"));
    empty_->setWordWrap(true);
    skipped_->setProperty("role", QStringLiteral("muted"));
    tableHeading_->setProperty("role", QStringLiteral("caption"));

    auto *cards = new QHBoxLayout;
    cards->setSpacing(10);
    cards->addWidget(total_);
    cards->addWidget(analyzed_);
    cards->addWidget(failed_);

    auto *contentLayout = new QVBoxLayout(content_);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(14);
    contentLayout->addLayout(cards);
    contentLayout->addWidget(skipped_);
    contentLayout->addWidget(tableHeading_);
    contentLayout->addWidget(byResolution_, 1);
    addReportRow(csv_, tr("CSV report"), contentLayout);
    addReportRow(txt_, tr("TXT report"), contentLayout);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(emptyArt_);
    layout->addWidget(empty_);
    layout->setAlignment(emptyArt_, Qt::AlignLeft | Qt::AlignTop);
    layout->setAlignment(empty_, Qt::AlignTop);
    layout->addWidget(content_, 1);
    clear();
}

void ResultsPanel::addReportRow(ReportRow &row, const QString &title, QVBoxLayout *into)
{
    row.box = new QWidget(content_);
    row.caption = new QLabel(title.toUpper(), row.box);
    row.caption->setProperty("role", QStringLiteral("caption"));
    row.path = new ElidedLabel(row.box);
    row.path->setProperty("role", QStringLiteral("mono"));
    row.open = new ChamferButton(tr("Open Report"), row.box);
    static_cast<ChamferButton *>(row.open)->setGlyph(QStringLiteral("open-report"));
    row.open->setToolTip(tr("Open the %1 in its default application").arg(title));
    row.open->setAccessibleName(tr("Open %1").arg(title));
    row.folder = new ChamferButton(tr("Show Containing Folder"), row.box);
    static_cast<ChamferButton *>(row.folder)->setGlyph(QStringLiteral("folder-open"));
    row.folder->setToolTip(tr("Open the folder that contains the %1").arg(title));
    row.folder->setAccessibleName(tr("Show folder of %1").arg(title));

    auto *line = new QHBoxLayout;
    line->setSpacing(8);
    line->addWidget(row.open);
    line->addWidget(row.folder);
    line->addStretch(1);
    auto *v = new QVBoxLayout(row.box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    auto *capRow = new QHBoxLayout;
    capRow->setSpacing(8);
    auto *fmt = new QLabel(row.box);
    fmt->setFixedSize(18, 18);
    fmt->setPixmap(Icons::pixmap(title.startsWith(QLatin1String("CSV")) ? QStringLiteral("csv") : QStringLiteral("txt"),
                                 18, Theme::mute(), Theme::ember(), devicePixelRatioF()));
    capRow->addWidget(fmt);
    capRow->addWidget(row.caption, 1);
    v->addLayout(capRow);
    v->addWidget(row.path);
    v->addLayout(line);
    into->addWidget(row.box);

    ReportRow *self = &row;
    connect(row.open, &QPushButton::clicked, this, [this, self] { openPath(self->file); });
    connect(row.folder, &QPushButton::clicked, this, [this, self] {
        openPath(QFileInfo(self->file).absolutePath());
    });
}

void ResultsPanel::clear()
{
    empty_->show();
    emptyArt_->show();
    content_->hide();
    for (ReportRow *r : {&csv_, &txt_}) {
        r->file.clear();
        r->box->hide();
    }
    byResolution_->setEntries({});
}

void ResultsPanel::setReport(ReportRow &row, bool wanted, const QString &confirmedPath)
{
    row.file.clear();
    row.box->setVisible(wanted);
    if (!wanted)
        return;
    if (confirmedPath.isEmpty()) {  // requested, but the backend did not confirm a file
        row.path->setFullText(tr("Not generated (the backend did not confirm a file)."));
        row.open->setEnabled(false);
        row.folder->setEnabled(false);
        return;
    }
    row.file = confirmedPath;
    const bool exists = QFileInfo::exists(confirmedPath);
    row.path->setFullText(QDir::toNativeSeparators(confirmedPath)
                          + (exists ? QString() : tr("  (file no longer exists)")));
    row.open->setEnabled(exists);
    row.folder->setEnabled(QFileInfo(confirmedPath).dir().exists());
}

void ResultsPanel::showResult(const QJsonObject &data, bool wantCsv, bool wantTxt)
{
    const QJsonObject summary = data.value(QStringLiteral("summary")).toObject();
    const int failed = summary.value(QStringLiteral("failed")).toInt();
    total_->setValue(summary.value(QStringLiteral("total")).toInt());
    analyzed_->setValue(summary.value(QStringLiteral("analyzed")).toInt(), MetricCard::Tone::Good);
    failed_->setValue(failed, failed > 0 ? MetricCard::Tone::Bad : MetricCard::Tone::Neutral);
    if (data.value(QStringLiteral("source")).toString() == QLatin1String("files"))
        skipped_->setText(tr("Selected images: %1").arg(UiFormat::count(data.value(QStringLiteral("selected")).toInt())));
    else
        skipped_->setText(tr("Non-image files skipped: %1")
                              .arg(UiFormat::count(data.value(QStringLiteral("skipped_non_images")).toInt())));

    QList<DistributionBar::Entry> entries;
    const QJsonObject byRes = summary.value(QStringLiteral("by_resolution")).toObject();
    for (auto it = byRes.begin(); it != byRes.end(); ++it)
        entries.append({it.key(), it.value().toInt()});
    byResolution_->setEntries(entries);
    tableHeading_->setVisible(byResolution_->rowCount() > 0);
    byResolution_->setVisible(byResolution_->rowCount() > 0);

    const QJsonObject reports = data.value(QStringLiteral("reports")).toObject();
    setReport(csv_, wantCsv, reports.value(QStringLiteral("csv")).toString());
    setReport(txt_, wantTxt, reports.value(QStringLiteral("txt")).toString());
    empty_->hide();
    emptyArt_->hide();
    content_->show();
}

void ResultsPanel::openPath(const QString &path) const
{
    if (path.isEmpty())
        return;
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        QMessageBox::warning(const_cast<ResultsPanel *>(this), tr("Open"),
                             tr("Could not open %1.").arg(QDir::toNativeSeparators(path)));
}
