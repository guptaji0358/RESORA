#include "widgets/exportdialog.h"

#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include "ui/theme.h"
#include "widgets/chamferbutton.h"
#include "widgets/confirmdialog.h"

using resoraui::Mode;

namespace {
// A mode card is a checkable QPushButton (focus, Space, accessibility) with labels on top.
QPushButton *modeCard(const QString &key, const QString &title, const QString &tag, const QString &lines,
                      const QString &accent)
{
    auto *b = new QPushButton;
    b->setObjectName(QStringLiteral("modeCard_") + key);
    b->setCheckable(true);
    b->setAutoExclusive(true);
    b->setAutoDefault(false);
    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumHeight(104);
    b->setAccessibleName(title);
    b->setAccessibleDescription(lines);
    b->setStyleSheet(QStringLiteral(
        "QPushButton{background:#15130F;border:1px solid #2A2622;border-left:3px solid #2A2622;text-align:left;}"
        "QPushButton:hover,QPushButton:focus{border-color:%1;outline:none;}"
        "QPushButton:checked{background:#1B1814;border-color:%1;border-left:5px solid %1;}")
                         .arg(accent));
    auto *col = new QVBoxLayout(b);
    col->setContentsMargins(18, 10, 14, 10);
    col->setSpacing(3);
    auto *head = new QLabel(QStringLiteral("%1   <span style='color:%2;font-size:8pt'>%3</span>")
                                .arg(title.toUpper(), accent, tag));
    head->setTextFormat(Qt::RichText);
    head->setFont(Theme::display(11, QFont::DemiBold, 2));
    head->setStyleSheet(QStringLiteral("color:#EFE9DD;background:transparent;"));
    auto *body = new QLabel(lines);
    body->setWordWrap(true);
    body->setStyleSheet(QStringLiteral("color:#9A9186;font-size:9pt;background:transparent;"));
    for (QLabel *l : {head, body}) {
        l->setAttribute(Qt::WA_TransparentForMouseEvents);
        col->addWidget(l);
    }
    return b;
}
}  // namespace

ExportDialog::ExportDialog(const dashboard::State &state, const QString &appVersion, QWidget *parent)
    : FramelessDialog(tr("Export dashboard"), parent), state_(state), appVersion_(appVersion),
      estimate_(resoraui::estimate(state))
{
    setObjectName(QStringLiteral("exportDialog"));
    setMinimumWidth(560);
    confirm_ = [this](const QString &path) {
        return ConfirmDialog::ask(this, tr("Replace existing file?"),
                                  tr("%1 already exists. Exporting will replace it. If the export fails or is "
                                     "cancelled, the existing file is kept.").arg(QDir::toNativeSeparators(path)),
                                  tr("Replace"));
    };

    auto *col = bodyLayout();
    auto *eyebrow = new QLabel(tr("EXPORT"));
    eyebrow->setProperty("role", QStringLiteral("eyebrow"));
    auto *head = new QLabel(tr("Save this dashboard as .resora"));
    head->setFont(Theme::display(18, QFont::DemiBold));
    head->setStyleSheet(QStringLiteral("color:#EFE9DD;"));
    col->addWidget(eyebrow);
    col->addWidget(head);
    col->addSpacing(6);

    const QString size = QLocale().formattedDataSize(qint64(estimate_.bytes));
    bundled_ = modeCard(QStringLiteral("bundled"), tr("Bundled"), tr("SELF-CONTAINED"),
                        tr("Includes the original images (%1 image(s), about %2). Opens anywhere, even if the "
                           "originals move. Larger file.").arg(estimate_.images).arg(size),
                        QStringLiteral("#FF5B2E"));
    linked_ = modeCard(QStringLiteral("linked"), tr("Linked"), tr("SMALL FILE"),
                       tr("No image bytes: stores references plus each image's size and SHA-256. The originals "
                          "must stay accessible; opening checks that each one is the same file."),
                       QStringLiteral("#6FB58A"));
    col->addWidget(bundled_);
    col->addWidget(linked_);
    modeNote_ = new QLabel;
    modeNote_->setWordWrap(true);
    modeNote_->setProperty("role", QStringLiteral("muted"));
    col->addWidget(modeNote_);

    auto *cap = new QLabel(tr("DESTINATION"));
    cap->setProperty("role", QStringLiteral("caption"));
    dest_ = new QLineEdit;
    dest_->setAccessibleName(tr("Destination .resora file"));
    dest_->setPlaceholderText(tr("Choose a file ending in .resora"));
    auto *browse = new ChamferButton(tr("Browse…"));
    browse->setCompact(true);
    browse->setAutoDefault(false);
    browse_ = browse;
    auto *row = new QHBoxLayout;
    row->addWidget(dest_, 1);
    row->addWidget(browse_);
    col->addSpacing(4);
    col->addWidget(cap);
    col->addLayout(row);
    error_ = new QLabel;
    error_->setProperty("role", QStringLiteral("error"));
    error_->setWordWrap(true);
    error_->hide();
    col->addWidget(error_);

    bar_ = new QProgressBar;
    bar_->setRange(0, 1000);
    bar_->setTextVisible(false);
    bar_->hide();
    status_ = new QLabel;
    status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    status_->setAccessibleName(tr("Export status"));
    status_->hide();
    col->addWidget(bar_);
    col->addWidget(status_);

    cancelBtn_ = new ChamferButton(tr("Close"));
    cancelBtn_->setAutoDefault(false);
    exportBtn_ = new ChamferButton(tr("Export"));
    exportBtn_->setAutoDefault(false);
    Theme::setProperty(exportBtn_, "variant", QStringLiteral("primary"));
    auto *btns = new QHBoxLayout;
    btns->addStretch(1);
    btns->addWidget(cancelBtn_);
    btns->addWidget(exportBtn_);
    col->addSpacing(6);
    col->addLayout(btns);

    connect(bundled_, &QPushButton::clicked, this, [this] { setMode(Mode::Bundled); });
    connect(linked_, &QPushButton::clicked, this, [this] { setMode(Mode::Linked); });
    connect(dest_, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(browse_, &QPushButton::clicked, this, &ExportDialog::browse);
    connect(exportBtn_, &QPushButton::clicked, this, [this] { startExport(); });
    connect(cancelBtn_, &QPushButton::clicked, this, &ExportDialog::reject);
    connect(&job_, &resoraui::ExportJob::progress, this, &ExportDialog::onProgress);
    connect(&job_, &resoraui::ExportJob::finished, this, &ExportDialog::onFinished);
    setMode(Mode::Bundled);
}

void ExportDialog::setMode(Mode m)
{
    if (job_.isRunning())
        return;
    mode_ = m;
    (m == Mode::Bundled ? bundled_ : linked_)->setChecked(true);
    modeNote_->setText(m == Mode::Bundled
                           ? tr("Bundled: the file contains copies of the original images; your originals are not touched.")
                           : tr("Linked: absolute image paths are stored in the file. Do not share it if paths are private."));
    refresh();
}

void ExportDialog::setDestination(const QString &path) { dest_->setText(path); }

void ExportDialog::refresh()
{
    check_ = resoraui::checkDestination(dest_->text(), estimate_.sources);
    QStringList problems = estimate_.problems;
    const bool showDest = !dest_->text().trimmed().isEmpty() && !check_.ok;
    if (showDest)
        problems.prepend(check_.error);
    error_->setText(problems.join(QLatin1Char('\n')));
    error_->setVisible(!problems.isEmpty());
    Theme::setProperty(dest_, "invalid", showDest ? QStringLiteral("true") : QStringLiteral("false"));
    exportBtn_->setEnabled(!job_.isRunning() && check_.ok && estimate_.problems.isEmpty());
}

void ExportDialog::browse()
{
    const QString start = dest_->text().trimmed().isEmpty() ? QDir::homePath() : dest_->text();
    const QString f = QFileDialog::getSaveFileName(this, tr("Export dashboard"), start, tr("RESORA dashboard (*.resora)"),
                                                   nullptr, QFileDialog::DontConfirmOverwrite);
    if (!f.isEmpty())
        dest_->setText(f);
}

bool ExportDialog::startExport()
{
    if (job_.isRunning())
        return false;  // duplicate requests are refused while one export runs
    refresh();
    if (!check_.ok || !estimate_.problems.isEmpty())
        return false;
    if (check_.exists && !confirm_(check_.path))
        return false;
    outcome_ = {};
    if (!job_.start(state_, mode_, check_.path, appVersion_))
        return false;
    status_->setStyleSheet(QString());
    status_->setText(tr("Starting…"));
    status_->show();
    bar_->setValue(0);
    bar_->show();
    setBusy(true);
    return true;
}

void ExportDialog::setBusy(bool busy)
{
    for (QWidget *w : {static_cast<QWidget *>(bundled_), static_cast<QWidget *>(linked_),
                       static_cast<QWidget *>(dest_), static_cast<QWidget *>(browse_)})
        w->setEnabled(!busy);
    cancelBtn_->setText(busy ? tr("Cancel export") : tr("Close"));
    refresh();
}

void ExportDialog::onProgress(int permille, const QString &stage)
{
    bar_->setValue(permille);
    status_->setText(stage);
}

void ExportDialog::onFinished(const resoraui::ExportOutcome &o)
{
    outcome_ = o;
    setBusy(false);
    bar_->hide();
    status_->show();
    if (o.ok) {
        status_->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::successColor()));
        status_->setText(tr("Exported %1 (%2, %3 image(s) %4).")
                             .arg(QDir::toNativeSeparators(o.path), QLocale().formattedDataSize(qint64(o.fileBytes)))
                             .arg(o.images)
                             .arg(mode_ == Mode::Bundled ? tr("included") : tr("linked")));
    } else {
        status_->setStyleSheet(QStringLiteral("color:%1;")
                                   .arg(o.cancelled ? Theme::accentColor() : Theme::dangerColor()));
        status_->setText(o.message);
    }
    if (closeWhenDone_)
        FramelessDialog::reject();
}

void ExportDialog::reject()
{
    if (job_.isRunning()) {  // never close under a running worker: cancel, close when it has finished
        closeWhenDone_ = true;
        job_.cancel();
        status_->show();
        status_->setText(tr("Cancelling…"));
        return;
    }
    FramelessDialog::reject();
}
