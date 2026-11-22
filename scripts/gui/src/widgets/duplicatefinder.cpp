#include "widgets/duplicatefinder.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTemporaryFile>
#include <QVBoxLayout>

#include "backend/backendlocator.h"
#include "backend/backendprocess.h"
#include "widgets/chamferbutton.h"
#include "widgets/metriccard.h"
#include "widgets/sectioncard.h"
#include "widgets/toggleswitch.h"
#include "ui/uiformat.h"

DuplicateFinder::DuplicateFinder(const QStringList &collection, const QString &startFolder, const QString &cachePath,
                                 QWidget *parent)
    : FramelessDialog(tr("Duplicate Finder"), parent),
      collection_(collection), startFolder_(startFolder), cachePath_(cachePath)
{
    setObjectName(QStringLiteral("duplicateFinder"));
    setMinimumSize(980, 660);
    resize(1100, 720);
    buildUi();
    refreshControls();
}

DuplicateFinder::~DuplicateFinder()
{
    if (job_) {
        job_->disconnect(this);
        job_->cancel();
    }
    delete paramsFile_;
}

void DuplicateFinder::buildUi()
{
    auto *eyebrow = new QLabel(tr("DUPLICATE FINDER"));
    eyebrow->setProperty("role", QStringLiteral("eyebrow"));
    auto *note = new QLabel(tr("Finds identical files (SHA-256) and visually similar images. Read-only: nothing is "
                               "ever deleted, moved or changed. Only the folders you add are scanned."));
    note->setProperty("role", QStringLiteral("muted"));
    note->setWordWrap(true);

    // --- sources ---
    auto *src = new SectionCard(tr("Sources"), tr("Folders to search, and any to leave out."));
    folders_ = new QListWidget;
    folders_->setMinimumHeight(60);
    folders_->setMaximumHeight(90);
    excludes_ = new QListWidget;
    for (auto *l : {folders_, excludes_}) {  // long paths elide in the middle instead of a horizontal scrollbar
        l->setTextElideMode(Qt::ElideMiddle);
        l->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    excludes_->setMinimumHeight(48);
    excludes_->setMaximumHeight(72);
    auto *add = new ChamferButton(tr("Add folder…"));
    add->setGlyph(QStringLiteral("add"));
    add->setCompact(true);
    auto *rem = new ChamferButton(tr("Remove"));
    rem->setGlyph(QStringLiteral("remove"));
    rem->setCompact(true);
    auto *excl = new ChamferButton(tr("Exclude folder…"));
    excl->setCompact(true);
    auto *exclRem = new ChamferButton(tr("Remove"));
    exclRem->setCompact(true);
    for (auto *b : {add, rem, excl, exclRem})
        b->setAutoDefault(false);
    connect(add, &QPushButton::clicked, this, &DuplicateFinder::pickFolder);
    connect(excl, &QPushButton::clicked, this, &DuplicateFinder::pickExclusion);
    connect(rem, &QPushButton::clicked, this, [this] { qDeleteAll(folders_->selectedItems()); refreshControls(); });
    connect(exclRem, &QPushButton::clicked, this, [this] { qDeleteAll(excludes_->selectedItems()); });
    auto *r1 = new QHBoxLayout;
    r1->addWidget(add);
    r1->addWidget(rem);
    r1->addStretch(1);
    auto *r2 = new QHBoxLayout;
    r2->addWidget(excl);
    r2->addWidget(exclRem);
    r2->addStretch(1);
    useCollection_ = new ToggleSwitch(tr("Use imported collection (%1)").arg(collection_.size()));
    useCollection_->setEnabled(!collection_.isEmpty());
    recursive_ = new ToggleSwitch(tr("Include subfolders"));
    recursive_->setChecked(true);
    similar_ = new ToggleSwitch(tr("Also find visually similar images"));
    similar_->setChecked(true);
    threshold_ = new QSpinBox;
    threshold_->setRange(50, 100);
    threshold_->setValue(90);
    threshold_->setSuffix(tr(" %"));
    threshold_->setToolTip(tr("Minimum visual similarity. Higher = stricter (fewer, safer matches); 100 = same fingerprint only."));
    connect(similar_, &QCheckBox::toggled, threshold_, &QWidget::setEnabled);
    connect(useCollection_, &QCheckBox::toggled, this, [this] { refreshControls(); });
    auto *thr = new QHBoxLayout;
    thr->addWidget(new QLabel(tr("Minimum similarity")));
    thr->addStretch(1);
    thr->addWidget(threshold_);
    src->body()->addWidget(new QLabel(tr("Folders to scan")));
    src->body()->addWidget(folders_);
    src->body()->addLayout(r1);
    src->body()->addWidget(new QLabel(tr("Folders to skip")));
    src->body()->addWidget(excludes_);
    src->body()->addLayout(r2);
    auto *opts = new SectionCard(tr("Options"), tr("How the scan behaves."));
    opts->body()->addWidget(useCollection_);
    opts->body()->addWidget(recursive_);
    opts->body()->addWidget(similar_);
    opts->body()->addLayout(thr);

    start_ = new ChamferButton(tr("Find duplicates"));
    start_->setProperty("variant", QStringLiteral("primary"));
    start_->setGlyph(QStringLiteral("scan"));
    start_->setAutoDefault(false);
    cancel_ = new ChamferButton(tr("Cancel"));
    cancel_->setGlyph(QStringLiteral("stop"));
    cancel_->setAutoDefault(false);
    cancel_->hide();
    connect(start_, &QPushButton::clicked, this, &DuplicateFinder::start);
    connect(cancel_, &QPushButton::clicked, this, &DuplicateFinder::cancel);
    bar_ = new QProgressBar;
    bar_->setTextVisible(false);
    bar_->setFixedHeight(6);
    bar_->hide();
    status_ = new QLabel;
    status_->setWordWrap(true);
    status_->setProperty("role", QStringLiteral("muted"));
    auto *go = new QVBoxLayout;
    go->setSpacing(8);
    auto *goButtons = new QHBoxLayout;
    goButtons->addWidget(start_);
    goButtons->addWidget(cancel_);
    goButtons->addStretch(1);
    go->addLayout(goButtons);
    go->addWidget(bar_);
    go->addWidget(status_);

    // --- results ---
    resultsBox_ = new QWidget;
    auto *rv = new QVBoxLayout(resultsBox_);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(12);
    static const char *captions[8] = {"Files scanned", "Exact groups", "Exact duplicate files", "Possible savings",
                                      "Exact files' storage", "Similar groups", "Similar images", "Similar storage"};
    // One grid owns all eight cards: columns are shared by both rows (two independent HBoxes gave each
    // row its own column widths, so the cards never lined up) and rows get real vertical spacing.
    // Semantic grouping: a heading per group, its own row, generous gaps. Row 0 = scan overview + exact
    // results; row 1 = similar results. Files scanned leads the exact row as the overview number.
    auto *stats = new QGridLayout;
    stats->setHorizontalSpacing(4);
    stats->setVerticalSpacing(6);
    for (int i = 0; i < 8; ++i)
        { cards_[i] = new MetricCard(tr(captions[i])); cards_[i]->setMinimumWidth(80); }
    auto heading = [](const QString &t) {
        auto *l = new QLabel(t.toUpper());
        l->setProperty("role", QStringLiteral("caption"));
        return l;
    };
    stats->addWidget(heading(tr("Exact duplicates · identical content")), 0, 0, 1, 4);
    const int exactOrder[4] = {1, 2, 4, 3};
    for (int k = 0; k < 4; ++k)
        stats->addWidget(cards_[exactOrder[k]], 1, k);
    stats->setRowMinimumHeight(2, 8);
    stats->addWidget(heading(tr("Visually similar · not proven identical")), 3, 0, 1, 3);
    stats->addWidget(heading(tr("Scan")), 3, 3);
    for (int k = 0; k < 3; ++k)
        stats->addWidget(cards_[5 + k], 4, k);
    stats->addWidget(cards_[0], 4, 3);
    for (int c = 0; c < 4; ++c)
        stats->setColumnStretch(c, 1);
    cards_[3]->setToolTip(tr("Exact duplicates only: every group keeps one copy, so savings = size × (copies − 1)."));
    cards_[7]->setToolTip(tr("Combined size of visually similar images. These are different files: NOT recoverable space."));
    rv->addLayout(stats);

    exactTab_ = new ChamferButton(tr("Exact duplicates"));
    similarTab_ = new ChamferButton(tr("Visually similar"));
    for (auto *b : {exactTab_, similarTab_}) {
        b->setCompact(true);
        b->setAutoDefault(false);
    }
    connect(exactTab_, &QPushButton::clicked, this, [this] { showKind(true); });
    connect(similarTab_, &QPushButton::clicked, this, [this] { showKind(false); });
    kindNote_ = new QLabel;
    kindNote_->setWordWrap(true);
    kindNote_->setProperty("role", QStringLiteral("muted"));
    auto *tabs = new QHBoxLayout;
    tabs->addWidget(exactTab_);
    tabs->addWidget(similarTab_);
    tabs->addStretch(1);
    rv->addLayout(tabs);
    rv->addWidget(kindNote_);

    // One scrolling column: every group is a header followed by its numbered, indented images.
    membersHost_ = new QWidget;
    membersLayout_ = new QVBoxLayout(membersHost_);
    membersLayout_->setContentsMargins(0, 0, 8, 0);
    membersLayout_->setSpacing(6);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumWidth(360);  // an image entry (thumbnail + text + buttons) never gets clipped
    scroll->setMinimumHeight(220);
    scroll->setWidget(membersHost_);
    rv->addWidget(scroll, 1);
    resultsBox_->hide();

    auto *outer = new QWidget;
    auto *ov = new QVBoxLayout(outer);
    ov->setContentsMargins(0, 0, 4, 0);
    ov->setSpacing(10);
    outer->setMinimumHeight(380);
    // Page header: eyebrow, title, one readable description line block (capped width), then a rule.
    auto *title = new QLabel(tr("Find duplicate images"));
    title->setProperty("role", QStringLiteral("message"));
    note->setMaximumWidth(900);
    auto *rule = new QFrame;
    rule->setFrameShape(QFrame::HLine);
    rule->setStyleSheet(QStringLiteral("color:#2A2622; background:#2A2622; max-height:1px;"));
    auto *head = new QHBoxLayout;
    head->setSpacing(14);
    head->addWidget(title);
    head->addWidget(eyebrow, 0, Qt::AlignBottom);
    head->addStretch(1);
    ov->addLayout(head);
    ov->addWidget(note);
    ov->addWidget(rule);
    // Setup column: scrolling sections on top, the run action pinned below (always visible).
    auto *left = new QVBoxLayout;
    left->setSpacing(14);
    left->addWidget(src);
    left->addWidget(opts);
    left->addStretch(1);
    auto *setupHost = new QWidget;
    setupHost->setLayout(left);
    left->setContentsMargins(0, 0, 10, 0);
    auto *setupScroll = new QScrollArea;
    setupScroll->setWidgetResizable(true);
    setupScroll->setFrameShape(QFrame::NoFrame);
    setupScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setupScroll->setWidget(setupHost);
    auto *actionBox = new QFrame;
    actionBox->setStyleSheet(QStringLiteral("QFrame#dupAction{border-top:1px solid #2A2622;}"));
    actionBox->setObjectName(QStringLiteral("dupAction"));
    auto *ab = new QVBoxLayout(actionBox);
    ab->setContentsMargins(0, 12, 10, 0);
    ab->addLayout(go);
    auto *leftHost = new QWidget;
    auto *lh = new QVBoxLayout(leftHost);
    lh->setContentsMargins(0, 0, 0, 0);
    lh->setSpacing(0);
    lh->addWidget(setupScroll, 1);
    lh->addWidget(actionBox);
    leftHost->setMinimumWidth(270);
    leftHost->setMaximumWidth(370);
    placeholder_ = new QLabel(tr("Add a folder and press Find duplicates.\nIdentical files and visually similar images appear here, grouped."));
    placeholder_->setProperty("role", QStringLiteral("muted"));
    placeholder_->setAlignment(Qt::AlignCenter);
    auto *cols = new QHBoxLayout;
    cols->setSpacing(20);
    cols->addWidget(leftHost, 1);  // 1:2 -> capped at 380 on wide windows, results get the rest
    cols->addWidget(resultsBox_, 2);
    cols->addWidget(placeholder_, 2);
    ov->addLayout(cols, 1);
    // Only scrolls when the window is below the content's minimum (page-level safety net).
    auto *page = new QScrollArea;
    page->setWidgetResizable(true);
    page->setFrameShape(QFrame::NoFrame);
    page->setWidget(outer);
    bodyLayout()->addWidget(page, 1);
}

void DuplicateFinder::addFolder(const QString &path)
{
    const QString p = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    if (folders_->findItems(p, Qt::MatchFixedString).isEmpty())
        folders_->addItem(p);
    refreshControls();
}

void DuplicateFinder::addExclusion(const QString &path)
{
    const QString p = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    if (excludes_->findItems(p, Qt::MatchFixedString).isEmpty())
        excludes_->addItem(p);
}

void DuplicateFinder::pickFolder()
{
    const QString d = QFileDialog::getExistingDirectory(this, tr("Add folder to scan"), startFolder_);
    if (!d.isEmpty())
        addFolder(d);
}

void DuplicateFinder::pickExclusion()
{
    const QString d = QFileDialog::getExistingDirectory(this, tr("Folder to skip"), startFolder_);
    if (!d.isEmpty())
        addExclusion(d);
}

void DuplicateFinder::setCollection(const QStringList &images, const QString &startFolder)
{
    collection_ = images;
    startFolder_ = startFolder;
    useCollection_->setText(tr("Use imported collection (%1)").arg(collection_.size()));
    useCollection_->setEnabled(!collection_.isEmpty() && !isRunning());
    if (collection_.isEmpty())
        useCollection_->setChecked(false);
    refreshControls();
}

void DuplicateFinder::setUseCollection(bool on)
{
    useCollection_->setChecked(on && !collection_.isEmpty());
}

DupRequest DuplicateFinder::request() const
{
    DupRequest r;
    for (int i = 0; i < folders_->count(); ++i)
        r.folders << folders_->item(i)->text();
    for (int i = 0; i < excludes_->count(); ++i)
        r.exclude << excludes_->item(i)->text();
    if (useCollection_->isChecked())
        r.files = collection_;
    r.recursive = recursive_->isChecked();
    r.similar = similar_->isChecked();
    r.threshold = threshold_->value();
    r.cachePath = cachePath_;
    return r;
}

QString DuplicateFinder::validationError() const
{
    const DupRequest r = request();
    if (r.folders.isEmpty() && r.files.isEmpty())
        return tr("Add a folder, or switch on the imported collection.");
    for (const QString &f : r.folders)
        if (!QFileInfo(f).isDir())
            return tr("Not a folder: %1").arg(f);
    return QString();
}

void DuplicateFinder::refreshControls()
{
    const bool running = job_ != nullptr;
    start_->setEnabled(!running && validationError().isEmpty());
    cancel_->setVisible(running);
    start_->setVisible(!running);
    for (QWidget *w : {static_cast<QWidget *>(folders_), static_cast<QWidget *>(excludes_)})
        w->setEnabled(!running);
}

void DuplicateFinder::start()
{
    if (job_)
        return;
    const QString err = validationError();
    if (!err.isEmpty()) {
        finish(err, true);
        return;
    }
    const BackendLocation loc = locateBackend(QProcessEnvironment::systemEnvironment(), QCoreApplication::applicationDirPath());
    if (!loc.ok()) {
        finish(loc.error, true);
        return;
    }
    delete paramsFile_;
    paramsFile_ = new QTemporaryFile(QDir::temp().filePath(QStringLiteral("resora-dup-XXXXXX.json")), this);
    if (!paramsFile_->open() || paramsFile_->write(request().paramsJson()) < 0 || !paramsFile_->flush()) {
        finish(tr("Could not write the temporary request file."), true);
        return;
    }
    paramsFile_->close();
    resultsBox_->hide();
    if (placeholder_)
        placeholder_->show();
    bar_->setRange(0, 0);
    bar_->show();
    status_->setText(tr("Starting…"));
    job_ = new BackendProcess(this);
    connect(job_, &BackendProcess::progress, this, &DuplicateFinder::onProgress);
    connect(job_, &BackendProcess::succeeded, this, &DuplicateFinder::onSucceeded);
    connect(job_, &BackendProcess::failed, this,
            [this](const QString &code, const QString &m, int, const QString &) { finish(tr("Failed [%1]: %2").arg(code, m), true); });
    connect(job_, &BackendProcess::cancelled, this, [this] { finish(tr("Cancelled. Nothing was changed."), false); });
    job_->start(loc, {QStringLiteral("--params-file"), paramsFile_->fileName(), QStringLiteral("duplicates")});
    refreshControls();
}

void DuplicateFinder::cancel()
{
    if (job_) {
        status_->setText(tr("Cancelling…"));
        job_->cancel();
    }
}

void DuplicateFinder::onProgress(qint64 done, qint64 total, const QString &message)
{
    if (total > 0) {
        bar_->setRange(0, 100);
        bar_->setValue(UiFormat::percent(done, total));
    } else {
        bar_->setRange(0, 0);
    }
    status_->setText(total > 0 ? QStringLiteral("%1 (%2/%3)").arg(message).arg(done).arg(total) : message);
}

void DuplicateFinder::onSucceeded(const QJsonValue &data)
{
    const DupResult r = DuplicatesModel::fromJson(data.toObject());
    finish(r.valid ? tr("Scan complete.") : tr("The backend returned no result."), !r.valid);
    if (r.valid)
        showResult(r);
}

void DuplicateFinder::finish(const QString &text, bool error)
{
    Q_UNUSED(error)
    if (job_)
        job_->deleteLater();
    job_ = nullptr;
    delete paramsFile_;
    paramsFile_ = nullptr;
    bar_->hide();
    status_->setText(text);
    refreshControls();
}
