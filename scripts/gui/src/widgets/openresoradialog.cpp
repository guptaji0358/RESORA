#include "widgets/openresoradialog.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QVBoxLayout>

#include "ui/theme.h"
#include "widgets/chamferbutton.h"
#include "widgets/relinkpanel.h"

OpenResoraDialog::OpenResoraDialog(const QString &startDir, QWidget *parent,
                                   std::shared_ptr<dashboard::CacheCleanupManager> cleanup)
    : FramelessDialog(tr("Open dashboard"), parent), startDir_(startDir), cleanup_(std::move(cleanup))
{
    setObjectName(QStringLiteral("openResoraDialog"));
    setMinimumWidth(640);
    auto *col = bodyLayout();
    auto *eyebrow = new QLabel(tr("OPEN"));
    eyebrow->setProperty("role", QStringLiteral("eyebrow"));
    auto *head = new QLabel(tr("Open a .resora dashboard"));
    head->setFont(Theme::display(18, QFont::DemiBold));
    head->setStyleSheet(QStringLiteral("color:#EFE9DD;"));
    auto *note = new QLabel(tr("The file is validated first. Your current dashboard stays as it is unless the new "
                               "one opens successfully."));
    note->setWordWrap(true);
    note->setProperty("role", QStringLiteral("muted"));
    col->addWidget(eyebrow);
    col->addWidget(head);
    col->addWidget(note);
    col->addSpacing(6);

    path_ = new QLineEdit;
    path_->setAccessibleName(tr(".resora file"));
    path_->setPlaceholderText(tr("Choose a .resora file"));
    browse_ = new ChamferButton(tr("Browse…"));
    browse_->setCompact(true);
    browse_->setAutoDefault(false);
    auto *row = new QHBoxLayout;
    row->addWidget(path_, 1);
    row->addWidget(browse_);
    col->addLayout(row);

    bar_ = new QProgressBar;
    bar_->setRange(0, 1000);
    bar_->setTextVisible(false);
    bar_->hide();
    status_ = new QLabel;
    status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    status_->setAccessibleName(tr("Open status"));
    status_->hide();
    col->addWidget(bar_);
    col->addWidget(status_);

    relink_ = new RelinkPanel;
    relinkTools_ = new QWidget;
    auto *tl = new QHBoxLayout(relinkTools_);
    tl->setContentsMargins(0, 0, 0, 0);
    folder_ = new ChamferButton(tr("Search a folder…"));
    folder_->setCompact(true);
    folder_->setAutoDefault(false);
    folder_->setToolTip(tr("Match the remaining images by file name inside a folder (and its subfolders)"));
    tl->addWidget(folder_);
    tl->addStretch(1);
    relink_->hide();
    relinkTools_->hide();
    col->addWidget(relinkTools_);
    col->addWidget(relink_, 1);

    close_ = new ChamferButton(tr("Close"));
    close_->setAutoDefault(false);
    open_ = new ChamferButton(tr("Open"));
    open_->setAutoDefault(false);
    Theme::setProperty(open_, "variant", QStringLiteral("primary"));
    auto *btns = new QHBoxLayout;
    btns->addStretch(1);
    btns->addWidget(close_);
    btns->addWidget(open_);
    col->addSpacing(6);
    col->addLayout(btns);

    connect(browse_, &QPushButton::clicked, this, &OpenResoraDialog::browse);
    connect(folder_, &QPushButton::clicked, this, &OpenResoraDialog::chooseFolder);
    connect(path_, &QLineEdit::textChanged, this, [this](const QString &t) {
        if (state_ == State::Idle || state_ == State::Failed) {
            file_ = QDir::fromNativeSeparators(t.trimmed());
            relink_->reset();  // choices and removals belong to one file and one open operation
            setState(State::Idle);
        }
    });
    connect(relink_, &RelinkPanel::changed, this, [this] { setState(state_); });
    connect(open_, &QPushButton::clicked, this, [this] {
        if (state_ == State::NeedsRelink)
            retryWithRelinks();
        else
            startOpen();
    });
    connect(close_, &QPushButton::clicked, this, &OpenResoraDialog::reject);
    connect(&job_, &resoraui::ImportJob::progress, this, &OpenResoraDialog::onProgress);
    connect(&job_, &resoraui::ImportJob::finished, this, &OpenResoraDialog::onFinished);
    setState(State::Idle);
}

QPushButton *OpenResoraDialog::openButton() const { return open_; }
QString OpenResoraDialog::message() const { return status_->text(); }

void OpenResoraDialog::setFile(const QString &path) { path_->setText(QDir::toNativeSeparators(path)); }

void OpenResoraDialog::browse()
{
    const QString f = QFileDialog::getOpenFileName(this, tr("Open dashboard"), startDir_,
                                                   tr("RESORA dashboard (*.resora)"));
    if (!f.isEmpty())
        setFile(f);
}

void OpenResoraDialog::chooseFolder()
{
    const QString d = QFileDialog::getExistingDirectory(this, tr("Folder containing the original images"));
    if (!d.isEmpty())
        relink_->assignFolder(d);
}

void OpenResoraDialog::setState(State s)
{
    state_ = s;
    const bool busy = s == State::Opening;
    const bool relinking = s == State::NeedsRelink;
    path_->setEnabled(!busy && !relinking && s != State::Done);
    browse_->setEnabled(path_->isEnabled());
    relink_->setVisible(relinking);
    relinkTools_->setVisible(relinking);
    bar_->setVisible(busy);
    open_->setText(relinking ? tr("Check and open") : tr("Open"));
    // Opening is blocked until every image has a replacement or is removed; the engine then revalidates ALL.
    open_->setEnabled(!busy && s != State::Done &&
                      (relinking ? relink_->complete() : QFileInfo(file_).isFile()));
    close_->setText(busy ? tr("Cancel") : tr("Close"));
}

bool OpenResoraDialog::startOpen()
{
    if (job_.isRunning() || state_ == State::NeedsRelink || !QFileInfo(file_).isFile())
        return false;
    begin(resora::OpenOptions());
    return job_.isRunning();
}

bool OpenResoraDialog::retryWithRelinks()
{
    if (job_.isRunning() || state_ != State::NeedsRelink || !relink_->complete())
        return false;  // never open while an image is still unresolved
    // Verified and newly chosen replacements, plus ONLY the images the user explicitly removed. Everything
    // else is revalidated by the engine and still blocks the open.
    resora::OpenOptions o;
    o.relinks = relink_->chosen();
    o.omitImages = relink_->omitted();
    begin(o);
    return job_.isRunning();
}

void OpenResoraDialog::begin(const resora::OpenOptions &opts)
{
    if (!job_.start(file_, opts, cleanup_))
        return;
    status_->setStyleSheet(QString());
    status_->setText(tr("Starting…"));
    status_->show();
    bar_->setValue(0);
    setState(State::Opening);
}

void OpenResoraDialog::onProgress(int permille, const QString &stage)
{
    bar_->setValue(permille);
    status_->setText(stage);
}

void OpenResoraDialog::onFinished(const resoraui::ImportOutcome &o)
{
    status_->show();
    if (closeWhenDone_) {  // the user closed the dialog meanwhile: discard whatever came back
        FramelessDialog::reject();
        return;
    }
    if (o.ok) {
        session_ = o.session;
        status_->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::successColor()));
        status_->setText(tr("Dashboard ready."));
        setState(State::Done);
        accept();
        return;
    }
    if (o.error == resora::OpenError::LinksBlocked && !o.problems.isEmpty()) {
        status_->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::accentColor()));
        status_->setText(tr("%1 of %2 linked image(s) could not be verified. Choose the original files, or remove them from the dashboard, to continue.")
                             .arg(o.problems.size()).arg(o.linkCount));
        // The table keeps every image: replacements that passed become Resolved (and stay accepted across
        // retries); rejected ones turn into Wrong Image and must be chosen again.
        relink_->setProblems(o.problems);
        setState(State::NeedsRelink);
        return;
    }
    status_->setStyleSheet(QStringLiteral("color:%1;").arg(o.cancelled ? Theme::accentColor() : Theme::dangerColor()));
    status_->setText(o.message.isEmpty() ? tr("The file could not be opened.") : o.message);
    setState(State::Failed);
}

void OpenResoraDialog::reject()
{
    if (job_.isRunning()) {
        closeWhenDone_ = true;
        job_.cancel();
        status_->show();
        status_->setText(tr("Cancelling…"));
        return;
    }
    FramelessDialog::reject();
}
