#include "analyticspanel.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>

#include "chamferbutton.h"
#include "distributionbar.h"
#include "elidedlabel.h"
#include "metriccard.h"
#include "pixelspan.h"
#include "sectioncard.h"
#include "sharechart.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/uiformat.h"

// One extreme: caption, what it is, where it is, and shortcuts to the image.
class ExtremeRow : public QWidget
{
public:
    ExtremeRow(const QString &caption, QWidget *parent) : QWidget(parent)
    {
        caption_ = new QLabel(caption.toUpper(), this);
        caption_->setProperty("role", QStringLiteral("caption"));
        detail_ = new QLabel(this);
        detail_->setWordWrap(true);
        path_ = new ElidedLabel(this);
        path_->setProperty("role", QStringLiteral("mono"));
        props = new ChamferButton(QObject::tr("Properties"), this);
        props->setGlyph(QStringLiteral("image"));
        props->setCompact(true);
        folder = new ChamferButton(QObject::tr("Folder"), this);
        folder->setGlyph(QStringLiteral("folder-open"));
        folder->setCompact(true);
        auto *text = new QVBoxLayout;
        text->setSpacing(2);
        text->addWidget(caption_);
        text->addWidget(detail_);
        text->addWidget(path_);
        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(0, 4, 0, 4);
        row->setSpacing(8);
        row->addLayout(text, 1);
        row->addWidget(props, 0, Qt::AlignVCenter);
        row->addWidget(folder, 0, Qt::AlignVCenter);
        setObjectName(caption);
    }
    void set(const AnalyticsImage &i, const QString &detail)
    {
        path = i.valid ? i.path : QString();
        detail_->setText(i.valid ? detail : AnalyticsModel::notAvailable());
        path_->setFullText(QDir::toNativeSeparators(path));
        path_->setVisible(i.valid);
        props->setEnabled(i.valid);
        folder->setEnabled(i.valid);
        setAccessibleName(caption_->text());
        setAccessibleDescription(i.valid ? detail + QStringLiteral(" ") + i.path : detail_->text());
    }
    QString detailText() const { return detail_->text(); }
    QString path;
    ChamferButton *props, *folder;

private:
    QLabel *caption_, *detail_;
    ElidedLabel *path_;
};

namespace {
QLabel *makeLabel(const QString &text, const char *role, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setProperty("role", QString::fromLatin1(role));
    l->setWordWrap(true);
    return l;
}

MetricCard *addTile(QGridLayout *grid, QWidget *parent, const char *key, const QString &caption, int row, int col, int span = 1)
{
    auto *card = new MetricCard(caption, parent);
    card->setObjectName(QString::fromLatin1(key));
    card->setText(AnalyticsModel::notAvailable(), MetricCard::Tone::Neutral, true);
    grid->addWidget(card, row, col, 1, span);
    return card;
}

QString imagesText(qint64 n)
{
    return n == 1 ? QObject::tr("1 image") : QObject::tr("%1 images").arg(UiFormat::count(n));
}
}  // namespace

AnalyticsPanel::AnalyticsPanel(QWidget *parent)
    : QWidget(parent),
      emptyArt_(new QLabel(this)),
      empty_(makeLabel(tr("No analytics yet. Import images (Ctrl+O) and run a scan; the statistics of that "
                          "collection appear here. Nothing is scanned just by opening this view."), "muted", this)),
      sourceLabel_(makeLabel(QString(), "muted", this)),
      staleLabel_(makeLabel(tr("A different collection is imported now. Refresh to analyze it."), "muted", this)),
      basis_(makeLabel(QString(), "muted", this)),
      content_(new QWidget(this))
{
    emptyArt_->setPixmap(Icons::artwork(QStringLiteral("illustrations/empty-results"), QSize(200, 125), devicePixelRatioF()));
    emptyArt_->setFixedSize(200, 125);
    emptyArt_->setAccessibleName(tr("Empty analytics illustration"));
    staleLabel_->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::accentColor()));
    staleLabel_->hide();

    refresh_ = new ChamferButton(tr("Refresh Analytics"), this);
    refresh_->setGlyph(QStringLiteral("refresh"));
    refresh_->setToolTip(tr("Re-read the headers of the currently imported collection (no reports are written)"));
    connect(refresh_, &QPushButton::clicked, this, &AnalyticsPanel::refreshRequested);

    auto *top = new QHBoxLayout;
    top->addWidget(sourceLabel_, 1);
    top->addWidget(refresh_, 0, Qt::AlignTop);

    // Failure banner
    failBanner_ = new QFrame(content_);
    failBanner_->setStyleSheet(QStringLiteral("QFrame { border: none; border-left: 3px solid %1; }").arg(Theme::dangerColor()));
    failText_ = makeLabel(QString(), "error", failBanner_);
    failList_ = makeLabel(QString(), "mono", failBanner_);
    failList_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    failList_->hide();
    failToggle_ = new ChamferButton(tr("Show files"), failBanner_);
    failToggle_->setCompact(true);
    connect(failToggle_, &QPushButton::clicked, this, [this] {
        const bool show = !failList_->isVisible();
        failList_->setVisible(show);
        failToggle_->setText(show ? tr("Hide files") : tr("Show files"));
    });
    auto *fb = new QVBoxLayout(failBanner_);
    fb->setContentsMargins(12, 4, 0, 4);
    fb->addWidget(failText_);
    fb->addWidget(failList_);
    auto *tg = new QHBoxLayout;
    tg->addWidget(failToggle_);
    tg->addStretch(1);
    fb->addLayout(tg);

    // Collection
    auto *collection = new SectionCard(tr("Collection"), QString(), content_);
    auto *g1 = new QGridLayout;
    g1->setSpacing(10);
    addTile(g1, content_, "tile.total", tr("Total images"), 0, 0);
    addTile(g1, content_, "tile.analyzed", tr("Analyzed"), 0, 1);
    addTile(g1, content_, "tile.failed", tr("Failed"), 0, 2);
    addTile(g1, content_, "tile.storage", tr("Storage used"), 0, 3);
    addTile(g1, content_, "tile.avgsize", tr("Average file size"), 1, 0, 2);
    collection->body()->addLayout(g1);
    largest_ = new ExtremeRow(tr("Largest file"), content_);
    smallest_ = new ExtremeRow(tr("Smallest file"), content_);
    collection->body()->addWidget(largest_);
    collection->body()->addWidget(smallest_);

    // Resolution
    auto *resolution = new SectionCard(tr("Resolution"), QString(), content_);
    auto *g2 = new QGridLayout;
    g2->setSpacing(10);
    addTile(g2, content_, "tile.avgwidth", tr("Average width"), 0, 0);
    addTile(g2, content_, "tile.avgheight", tr("Average height"), 0, 1);
    addTile(g2, content_, "tile.avgmp", tr("Average megapixels"), 0, 2);
    addTile(g2, content_, "tile.totalmp", tr("Total megapixels"), 0, 3);
    addTile(g2, content_, "tile.common", tr("Most common size"), 1, 0, 2);
    resolution->body()->addLayout(g2);
    span_ = new PixelSpan(content_);
    resolution->body()->addWidget(span_);
    highest_ = new ExtremeRow(tr("Highest resolution"), content_);
    lowest_ = new ExtremeRow(tr("Lowest resolution"), content_);
    resolution->body()->addWidget(highest_);
    resolution->body()->addWidget(lowest_);

    auto *catCard = new SectionCard(tr("Resolution categories"), QString(), content_);
    categories_ = new ShareChart(tr("Resolution categories"), content_);
    catCard->body()->addWidget(categories_);
    auto *orCard = new SectionCard(tr("Orientation"), QString(), content_);
    orientation_ = new ShareChart(tr("Orientation"), content_);
    orCard->body()->addWidget(orientation_);
    auto *fmtCard = new SectionCard(tr("Formats and storage"),
                                    tr("Top bar: share of images. Thin bar: share of storage."), content_);
    formats_ = new ShareChart(tr("Formats and storage"), content_);
    otherFormats_ = makeLabel(QString(), "muted", content_);
    fmtCard->body()->addWidget(formats_);
    fmtCard->body()->addWidget(otherFormats_);

    auto *cl = new QVBoxLayout(content_);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(18);
    cl->addWidget(basis_);
    cl->addWidget(failBanner_);
    for (QWidget *w : {static_cast<QWidget *>(collection), static_cast<QWidget *>(resolution), static_cast<QWidget *>(catCard),
                       static_cast<QWidget *>(orCard), static_cast<QWidget *>(fmtCard)})
        cl->addWidget(w);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    layout->addLayout(top);
    layout->addWidget(staleLabel_);
    layout->addWidget(emptyArt_, 0, Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(empty_);
    layout->addWidget(content_);

    auto reveal = [this](ExtremeRow *row) {
        connect(row->props, &QPushButton::clicked, this, [this, row] { emit propertiesRequested(row->path); });
        connect(row->folder, &QPushButton::clicked, this, [this, row] { revealFolderOf(row->path); });
    };
    for (ExtremeRow *r : {largest_, smallest_, highest_, lowest_})
        reveal(r);
    clear();
}

void AnalyticsPanel::clear()
{
    data_ = AnalyticsData();
    source_.clear();
    sourceLabel_->setText(tr("No collection analyzed."));
    staleLabel_->hide();
    content_->hide();
    empty_->show();
    emptyArt_->show();
}

QString AnalyticsPanel::sourceText() const { return source_; }
bool AnalyticsPanel::isStale() const { return staleLabel_->isVisible() || staleLabel_->property("stale").toBool(); }
void AnalyticsPanel::setRefreshEnabled(bool enabled) { refresh_->setEnabled(enabled); }

void AnalyticsPanel::setStale(bool stale)
{
    staleLabel_->setProperty("stale", stale);
    staleLabel_->setVisible(stale && data_.valid);
}

QString AnalyticsPanel::describeSource(const QJsonObject &r) const
{
    QString s;
    if (r.value(QLatin1String("source")).toString() == QLatin1String("folder"))
        s = tr("Folder %1 · %2").arg(QDir::toNativeSeparators(r.value(QLatin1String("folder")).toString()),
                                      r.value(QLatin1String("recursive")).toBool() ? tr("subfolders included") : tr("top level only"));
    else
        s = tr("%1 selected").arg(imagesText(r.value(QLatin1String("selected")).toInteger()));
    return s;
}

void AnalyticsPanel::showAnalytics(const QJsonObject &result)
{
    const AnalyticsData d = AnalyticsModel::fromJson(result.value(QLatin1String("analytics")).toObject());
    if (!d.valid) {
        clear();
        return;
    }
    data_ = d;
    source_ = describeSource(result);
    sourceLabel_->setText(tr("Based on: %1").arg(source_));
    staleLabel_->setProperty("stale", false);
    staleLabel_->hide();
    populate();
    empty_->hide();
    emptyArt_->hide();
    content_->show();
}

void AnalyticsPanel::revealFolderOf(const QString &path) const
{
    if (path.isEmpty())
        return;
#ifdef Q_OS_WIN
    // Explorer's own /select verb: reuses an open window and highlights the file.
    QProcess explorer;
    explorer.setProgram(QStringLiteral("explorer.exe"));
    explorer.setNativeArguments(QStringLiteral("/select,\"%1\"").arg(QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath())));
    if (explorer.startDetached())
        return;
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

void AnalyticsPanel::showFailures()
{
    const AnalyticsData &d = data_;
    failBanner_->setVisible(d.failed > 0);
    if (d.failed <= 0)
        return;
    QStringList kinds;
    for (const auto &k : d.failedByStatus)
        kinds << QStringLiteral("%1 %2").arg(AnalyticsModel::statusText(k.first).toLower(), UiFormat::count(k.second));
    failText_->setText(tr("%1 of %2 could not be analyzed (%3). They are excluded from every figure below; "
                          "percentages cover the %4 analyzed.")
                           .arg(UiFormat::count(d.failed), imagesText(d.total), kinds.join(QStringLiteral(", ")),
                                UiFormat::count(d.analyzed)));
    QStringList lines;
    for (const AnalyticsFailure &f : d.failures)
        lines << QStringLiteral("[%1] %2%3").arg(AnalyticsModel::statusText(f.status), QDir::toNativeSeparators(f.path),
                                                 f.error.isEmpty() ? QString() : QStringLiteral(" — ") + f.error);
    if (d.failuresTruncated)
        lines << tr("… and %1 more (first %2 shown)").arg(UiFormat::count(d.failed - d.failures.size())).arg(d.failures.size());
    failList_->setText(lines.join(QLatin1Char('\n')));
}

void AnalyticsPanel::populate()
{
    const auto AM = [] { return AnalyticsModel::notAvailable(); };
    const AnalyticsData &d = data_;
    auto tile = [this](const char *key, const QString &text, const QString &hint,
                       MetricCard::Tone tone = MetricCard::Tone::Neutral) {
        findChild<MetricCard *>(QString::fromLatin1(key))->setText(text, tone, true, hint);
    };
    basis_->setText(tr("Averages and percentages are taken over the %1 analyzed image(s) (valid header and dimensions); "
                       "failed files are never counted as analyzed.").arg(UiFormat::count(d.analyzed)));
    showFailures();

    tile("tile.total", UiFormat::count(d.total), tr("Unique files in the collection; duplicate paths are counted once."));
    tile("tile.analyzed", UiFormat::count(d.analyzed), tr("Header read and dimensions valid."),
         d.analyzed > 0 ? MetricCard::Tone::Good : MetricCard::Tone::Neutral);
    tile("tile.failed", UiFormat::count(d.failed), tr("Corrupted, unsupported, unreadable or invalid-dimension files."),
         d.failed > 0 ? MetricCard::Tone::Bad : MetricCard::Tone::Neutral);
    tile("tile.storage", d.sizeKnown > 0 ? AnalyticsModel::bytesText(double(d.totalBytes)) : AM(),
         tr("Sum of file sizes of analyzed images (%1 without a known size are left out). Failed files: %2.")
             .arg(UiFormat::count(d.sizeUnknown), AnalyticsModel::bytesText(double(d.failedBytes))));
    tile("tile.avgsize", AnalyticsModel::bytesText(d.averageBytes), tr("Storage divided by analyzed images with a known file size."));
    tile("tile.avgwidth", d.averageWidth < 0 ? AM() : QStringLiteral("%1 px").arg(d.averageWidth, 0, 'f', 1),
         tr("Mean displayed width of analyzed images."));
    tile("tile.avgheight", d.averageHeight < 0 ? AM() : QStringLiteral("%1 px").arg(d.averageHeight, 0, 'f', 1),
         tr("Mean displayed height of analyzed images."));
    tile("tile.avgmp", AnalyticsModel::megapixelsText(d.averageMegapixels), tr("Total megapixels divided by analyzed images."));
    tile("tile.totalmp", AnalyticsModel::megapixelsText(d.totalMegapixels), tr("Sum of width × height of all analyzed images."));
    tile("tile.common",
         d.commonCount > 0 ? tr("%1 (%2)").arg(AnalyticsModel::sizeText(d.commonWidth, d.commonHeight), imagesText(d.commonCount)) : AM(),
         d.commonTied > 1 ? tr("%1 sizes tie for most common; the larger one is shown.").arg(d.commonTied)
                          : tr("Exact displayed width × height that occurs most often."));

    auto sizeDetail = [](const AnalyticsImage &i) {
        return QStringLiteral("%1 · %2 · %3").arg(i.name, AnalyticsModel::bytesText(double(i.fileSize)), AnalyticsModel::sizeText(i.width, i.height));
    };
    largest_->set(d.largest, sizeDetail(d.largest));
    smallest_->set(d.smallest, sizeDetail(d.smallest));
    auto resDetail = [](const AnalyticsImage &i) {
        QString t = QStringLiteral("%1 — %2").arg(i.name, AnalyticsModel::imageSummary(i));
        if (i.tied > 1)
            t += QObject::tr("  (+%1 more with the same pixel count)").arg(UiFormat::count(i.tied - 1));
        return t;
    };
    highest_->set(d.highest, resDetail(d.highest));
    lowest_->set(d.lowest, resDetail(d.lowest));
    span_->setValues(d.lowest.valid ? d.lowest.megapixels : 0, d.averageMegapixels, d.highest.valid ? d.highest.megapixels : 0);

    QList<ShareChart::Row> rows;
    for (const AnalyticsShare &s : d.categories) {
        ShareChart::Row r{s.name, s.count, s.percent};
        const QSize sz = DistributionBar::standardSize(s.name);
        if (sz.width() > 0) {
            r.frame = sz;
            r.glyphScale = std::pow(double(sz.width()) / 7680.0, 0.55);
        }
        rows << r;
    }
    categories_->setRows(rows);
    rows.clear();
    for (const AnalyticsShare &s : d.orientation) {
        ShareChart::Row r{s.name, s.count, s.percent};
        r.name = s.name.left(1).toUpper() + s.name.mid(1);
        r.frame = s.name == QLatin1String("portrait") ? QSizeF(10, 16) : s.name == QLatin1String("square") ? QSizeF(1, 1) : QSizeF(16, 10);
        rows << r;
    }
    orientation_->setRows(rows);
    rows.clear();
    for (const AnalyticsShare &s : d.formats) {
        ShareChart::Row r{s.name, s.count, s.percent};
        r.hasBytes = true;
        r.bytesText = d.sizeKnown > 0 ? AnalyticsModel::bytesText(double(s.bytes)) : AM();
        r.bytesPercent = s.bytesPercent;
        rows << r;
    }
    formats_->setRows(rows);
    QStringList other;
    for (const auto &o : d.otherFormats)
        other << QStringLiteral("%1 %2").arg(o.first, UiFormat::count(o.second));
    otherFormats_->setText(other.isEmpty() ? QString() : tr("Other formats: %1").arg(other.join(QStringLiteral(", "))));
    otherFormats_->setVisible(!other.isEmpty());
    formats_->setVisible(!rows.isEmpty());
}
