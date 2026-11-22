#include "widgets/progresspanel.h"

#include <climits>

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QVBoxLayout>

#include "widgets/elidedlabel.h"
#include "widgets/viewfinder.h"
#include "ui/theme.h"
#include "ui/uiformat.h"

ProgressPanel::ProgressPanel(QWidget *parent)
    : QWidget(parent),
      badge_(new QLabel(this)),
      message_(new QLabel(this)),
      item_(new ElidedLabel(this)),
      counts_(new QLabel(this)),
      bar_(new QProgressBar(this)),
      finder_(new Viewfinder(this)),
      details_(new QPlainTextEdit(this))
{
    badge_->setProperty("role", QStringLiteral("state"));
    badge_->setAccessibleName(tr("Scan state"));
    message_->setWordWrap(true);
    message_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    item_->setProperty("role", QStringLiteral("mono"));
    counts_->setProperty("role", QStringLiteral("muted"));
    bar_->setTextVisible(false);
    
    bar_->setAccessibleName(tr("Scan progress"));
    details_->setReadOnly(true);
    details_->setMaximumHeight(110);
    details_->setAccessibleName(tr("Backend diagnostics"));

    message_->setProperty("role", QStringLiteral("message"));
    auto *top = new QHBoxLayout;
    top->setSpacing(12);
    top->addWidget(badge_, 1);
    top->addWidget(counts_, 0);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    finder_->setMaximumHeight(250);
    layout->addWidget(finder_);
    layout->addLayout(top);
    layout->addWidget(message_);
    layout->addWidget(bar_);
    layout->addWidget(item_);
    layout->addWidget(details_);
    reset();
}

bool ProgressPanel::isIndeterminate() const
{
    return bar_->minimum() == 0 && bar_->maximum() == 0;
}

void ProgressPanel::setBarColor(const QColor &color)
{
    QPalette p = bar_->palette();
    p.setColor(QPalette::Highlight, color);
    bar_->setPalette(p);
}

void ProgressPanel::setState(State s, const QString &text)
{
    state_ = s;
    static const char *const names[] = {"idle", "starting", "running", "cancelling",
                                        "succeeded", "failed", "cancelled"};
    const QString labels[] = {tr("Ready"),     tr("Starting"), tr("Scanning"), tr("Cancelling"),
                              tr("Completed"), tr("Failed"),   tr("Cancelled")};
    const int i = static_cast<int>(s);
    badge_->setText(labels[i].toUpper());  // the word carries the state, not only the colour
    Theme::setProperty(badge_, "state", QString::fromLatin1(names[i]));
    message_->setText(text);
    Theme::setProperty(message_, "role", s == State::Failed ? QStringLiteral("error") : QStringLiteral("message"));
    syncRing();
    emit stateChanged(labels[i], QString::fromLatin1(names[i]));
}

// The ring only mirrors the bar: real done/total, or indeterminate; never a made-up value.
void ProgressPanel::syncRing()
{
    if (isIndeterminate()) {
        finder_->setFraction(-1);
        finder_->setTexts(QStringLiteral("…"), badge_->text());
        return;
    }
    const int pct = UiFormat::percent(bar_->value(), bar_->maximum());
    const bool has = state_ != State::Idle && state_ != State::Starting && bar_->maximum() > 1;
    const bool done = state_ == State::Succeeded;
    finder_->setFraction(done ? 1.0 : (has ? double(bar_->value()) / bar_->maximum() : 0.0));
    finder_->setTexts(done ? QStringLiteral("100%") : (has && pct >= 0 ? QStringLiteral("%1%").arg(pct) : QStringLiteral("—")),
                    badge_->text());
}

// Ends the bar in a non-success state: keeps a real value, drops an indeterminate one.
void ProgressPanel::finishBar(const QColor &color)
{
    setBarColor(color);
    if (isIndeterminate()) {
        bar_->setRange(0, 1);
        bar_->setValue(0);
    }
    finder_->setColor(color);
    syncRing();
}

void ProgressPanel::reset()
{
    setState(State::Idle, tr("Import images (Ctrl+O) and press Start Scan."));
    bar_->setRange(0, 1);
    bar_->setValue(0);
    setBarColor(QColor(Theme::accentColor()));
    finder_->setColor(QColor(Theme::accentColor()));
    syncRing();
    counts_->clear();
    item_->setFullText(QString());
    details_->clear();
    details_->hide();
}

void ProgressPanel::setStarting()
{
    reset();
    setState(State::Starting, tr("Starting the analysis engine..."));
    bar_->setRange(0, 0);
    syncRing();
}

void ProgressPanel::setProgress(qint64 done, qint64 total, const QString &message)
{
    if (state_ == State::Cancelling)
        return;  // keep the "Cancelling" message until the process ends
    if (total > 0) {
        bar_->setRange(0, static_cast<int>(qMin<qint64>(total, INT_MAX)));
        bar_->setValue(static_cast<int>(qMin<qint64>(done, INT_MAX)));
        counts_->setText(UiFormat::progressSummary(done, total));
        setState(State::Running, tr("Analyzing images"));
        item_->setFullText(message);
        syncRing();
    } else {
        bar_->setRange(0, 0);  // total unknown (e.g. discovery): do not fake a percentage
        counts_->clear();
        setState(State::Running,
                 message.isEmpty() ? tr("Working...")
                                   : message.left(1).toUpper() + message.mid(1) + QStringLiteral("..."));
        item_->setFullText(QString());
        syncRing();
    }
}

void ProgressPanel::setCancelling()
{
    setState(State::Cancelling, tr("Cancelling the scan..."));
}

void ProgressPanel::setSucceeded(const QString &text)
{
    setState(State::Succeeded, text);
    bar_->setRange(0, 1);
    bar_->setValue(1);
    setBarColor(QColor(Theme::successColor()));
    finder_->setColor(QColor(Theme::successColor()));
    syncRing();
    item_->setFullText(QString());
}

void ProgressPanel::setFailed(const QString &text, const QString &diagnostics)
{
    setState(State::Failed, text);
    finishBar(QColor(Theme::dangerColor()));
    item_->setFullText(QString());
    if (!diagnostics.isEmpty()) {
        details_->setPlainText(diagnostics);
        details_->show();
    }
}

void ProgressPanel::setCancelled(const QString &text)
{
    setState(State::Cancelled, text);
    finishBar(QColor(Theme::neutralColor()));
    item_->setFullText(QString());
}

void ProgressPanel::setNotice(const QString &text, bool isError)
{
    message_->setText(text);
    Theme::setProperty(message_, "role", isError ? QStringLiteral("error") : QStringLiteral("message"));
}
