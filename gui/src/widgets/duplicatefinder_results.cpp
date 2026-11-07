// Results half of the Duplicate Finder: statistics, group list, member rows. Read-only actions only.
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPointer>
#include <QProcess>
#include <QThreadPool>
#include <QUrl>
#include <QVBoxLayout>

#include "backend/previewloader.h"
#include "widgets/chamferbutton.h"
#include "widgets/duplicatefinder.h"
#include "widgets/elidedlabel.h"
#include "widgets/metriccard.h"
#include "ui/theme.h"
#include "ui/uiformat.h"

namespace {
constexpr int kMaxRows = 100;  // members rendered per group (bounded widget count)
constexpr int kThumb = 96;

void revealInFolder(const QString &path)
{
    const QString native = QDir::toNativeSeparators(path);
#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,") + native});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

QString facts(const DupFile &f)
{
    const QString dims = f.width > 0 ? QStringLiteral("%1 × %2").arg(f.width).arg(f.height) : QObject::tr("size unknown");
    return QStringLiteral("%1  ·  %2  ·  %3").arg(dims, DuplicatesModel::bytesText(f.size),
                                                   f.format.isEmpty() ? QObject::tr("unknown format") : f.format);
}
}  // namespace

void DuplicateFinder::showResult(const DupResult &r)
{
    result_ = r;
    const DupStats &s = r.stats;
    cards_[0]->setValue(s.total);
    cards_[1]->setValue(s.exactGroups, s.exactGroups ? MetricCard::Tone::Bad : MetricCard::Tone::Good);
    cards_[2]->setValue(s.exactFiles);
    cards_[3]->setText(DuplicatesModel::bytesText(s.exactSavings), MetricCard::Tone::Neutral, true);
    cards_[4]->setText(DuplicatesModel::bytesText(s.exactBytes), MetricCard::Tone::Neutral, true);
    cards_[5]->setValue(s.similarGroups);
    cards_[6]->setValue(s.similarFiles);
    cards_[7]->setText(DuplicatesModel::bytesText(s.similarBytes), MetricCard::Tone::Neutral, true);
    similarTab_->setEnabled(r.similarEnabled);
    exactTab_->setText(tr("Exact duplicates (%1)").arg(r.exact.size()));
    similarTab_->setText(tr("Visually similar (%1)").arg(r.similar.size()));
    QString extra;
    if (s.skipped > 0)
        extra = tr("%1 file(s) could not be read or decoded and were skipped. ").arg(s.skipped);
    if (!r.cacheWarning.isEmpty())
        extra += r.cacheWarning;
    if (!extra.isEmpty())
        status_->setText(status_->text() + QLatin1Char(' ') + extra);
    resultsBox_->show();
    placeholder_->hide();
    showKind(!r.exact.isEmpty() || r.similar.isEmpty());
}

void DuplicateFinder::showKind(bool exact)
{
    showExact_ = exact;
    Theme::setProperty(exactTab_, "variant", exact ? QStringLiteral("primary") : QString());
    Theme::setProperty(similarTab_, "variant", exact ? QString() : QStringLiteral("primary"));
    kindNote_->setText(exact ? tr("EXACT: byte-for-byte identical content (SHA-256), whatever the names or folders. "
                                  "Keep one copy; the others are redundant.")
                             : tr("SIMILAR: look alike (resized, recompressed, other format). NOT proven identical: "
                                  "compare them before deciding anything. The score is a visual match, not a guarantee."));
    fillGroups();
}

int DuplicateFinder::groupCount() const { return groups_->count(); }
int DuplicateFinder::memberRowCount() const { return membersLayout_->count() - 1; }  // minus the stretch
void DuplicateFinder::selectGroup(int row) { groups_->setCurrentRow(row); }

void DuplicateFinder::fillGroups()
{
    groups_->blockSignals(true);
    groups_->clear();
    const QList<DupGroup> &list = showExact_ ? result_.exact : result_.similar;
    for (const DupGroup &g : list) {
        const QString detail = g.exact ? tr("saves %1").arg(DuplicatesModel::bytesText(g.savingsBytes))
                                       : tr("%1 · ≥ %2").arg(g.category, DuplicatesModel::similarityText(g.lowest));
        groups_->addItem(QStringLiteral("%1\n%2 · %3").arg(DuplicatesModel::groupTitle(g), g.files.value(0).name, detail));
    }
    groups_->blockSignals(false);
    groups_->setCurrentRow(list.isEmpty() ? -1 : 0);
    fillMembers(list.isEmpty() ? -1 : 0);
}

void DuplicateFinder::fillMembers(int row)
{
    while (QLayoutItem *item = membersLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const QList<DupGroup> &list = showExact_ ? result_.exact : result_.similar;
    if (row < 0 || row >= list.size()) {
        auto *empty = new QLabel(showExact_ ? tr("No exact duplicates found.") : tr("No visually similar images found."));
        empty->setProperty("role", QStringLiteral("muted"));
        membersLayout_->addWidget(empty);
    } else {
        const DupGroup &g = list.at(row);
        for (int i = 0; i < g.files.size() && i < kMaxRows; ++i)
            membersLayout_->addWidget(memberRow(g, i));
        if (g.files.size() > kMaxRows)
            membersLayout_->addWidget(new QLabel(tr("… and %1 more files in this group.").arg(g.files.size() - kMaxRows)));
    }
    membersLayout_->addStretch(1);
}

QWidget *DuplicateFinder::memberRow(const DupGroup &g, int index)
{
    const DupFile &f = g.files.at(index);
    auto *row = new QFrame;
    row->setObjectName(QStringLiteral("dupRow"));
    row->setStyleSheet(QStringLiteral("#dupRow{border:1px solid %1; border-left:3px solid %2;}")
                           .arg(Theme::line().name(), g.exact ? Theme::alert().name() : Theme::ember().name()));
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(12, 10, 12, 10);
    h->setSpacing(14);
    auto *thumb = new QLabel(tr("…"));
    thumb->setFixedSize(kThumb, kThumb);
    thumb->setAlignment(Qt::AlignCenter);
    h->addWidget(thumb);
    // Thumbnails are decoded off the GUI thread, one small scaled read each; the file is only read.
    QPointer<QLabel> target(thumb);
    const QString path = f.path;
    QThreadPool::globalInstance()->start([target, path] {
        const PreviewResult p = PreviewLoader::load(path, kThumb * 2);
        if (!target)
            return;
        const QImage img = p.ok() ? p.image.scaled(kThumb, kThumb, Qt::KeepAspectRatio, Qt::SmoothTransformation) : QImage();
        QMetaObject::invokeMethod(target.data(), [target, img] {
            if (!target)
                return;
            if (img.isNull())
                target->setText(QObject::tr("No preview"));
            else
                target->setPixmap(QPixmap::fromImage(img));
        }, Qt::QueuedConnection);
    });

    auto *col = new QVBoxLayout;
    auto *name = new ElidedLabel;
    name->setFullText(f.name);
    name->setFont(Theme::display(11, QFont::DemiBold));
    auto *full = new ElidedLabel;
    full->setFullText(QDir::toNativeSeparators(f.path));
    full->setProperty("role", QStringLiteral("muted"));
    auto *info = new QLabel(facts(f));
    info->setWordWrap(true);
    auto *badge = new QLabel;
    badge->setWordWrap(true);
    if (g.exact) {
        badge->setText(tr("EXACT DUPLICATE · identical content"));
        badge->setStyleSheet(QStringLiteral("color:%1; font-weight:600;").arg(Theme::alert().name()));
    } else {
        badge->setText(f.anchor ? tr("VISUALLY SIMILAR · comparison base (not a preferred copy)")
                                : tr("VISUALLY SIMILAR · %1 · %2 match").arg(f.category, DuplicatesModel::similarityText(f.similarity)));
        badge->setStyleSheet(QStringLiteral("color:%1; font-weight:600;").arg(Theme::ember().name()));
        if (f.identicalCopies > 0)
            info->setText(info->text() + tr("  ·  %1 more identical cop%2 listed under Exact")
                                              .arg(f.identicalCopies).arg(f.identicalCopies == 1 ? tr("y") : tr("ies")));
    }
    col->addWidget(name);
    col->addWidget(full);
    col->addWidget(info);
    col->addWidget(badge);
    h->addLayout(col, 1);

    auto *btns = new QVBoxLayout;
    btns->setSpacing(6);
    auto *open = new ChamferButton(tr("Open"));
    auto *folder = new ChamferButton(tr("Folder"));
    auto *cmp = new ChamferButton(tr("Compare"));
    for (auto *b : {open, folder, cmp}) {
        b->setCompact(true);
        b->setAutoDefault(false);
        btns->addWidget(b);
    }
    open->setToolTip(tr("Open this image in the default viewer"));
    folder->setToolTip(tr("Show this file in its folder"));
    cmp->setToolTip(tr("Compare with %1").arg(g.files.size() > 1 ? g.files.at(index == 0 ? 1 : 0).name : QString()));
    cmp->setEnabled(g.files.size() > 1);
    connect(open, &QPushButton::clicked, this, [path] { QDesktopServices::openUrl(QUrl::fromLocalFile(path)); });
    connect(folder, &QPushButton::clicked, this, [path] { revealInFolder(path); });
    const QString other = g.files.size() > 1 ? g.files.at(index == 0 ? 1 : 0).path : QString();
    connect(cmp, &QPushButton::clicked, this, [this, path, other] { emit compareRequested(path, other); });
    h->addLayout(btns);
    h->setAlignment(btns, Qt::AlignTop);
    return row;
}
