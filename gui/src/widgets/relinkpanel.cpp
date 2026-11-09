#include "widgets/relinkpanel.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include "ui/theme.h"
#include "widgets/chamferbutton.h"
#include "widgets/elidedlabel.h"

RelinkPanel::RelinkPanel(QWidget *parent) : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    summary_ = new QLabel;
    summary_->setWordWrap(true);
    summary_->setProperty("role", QStringLiteral("muted"));
    outer->addWidget(summary_);
    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("relinkScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumHeight(140);
    auto *inner = new QWidget;
    rows_ = new QVBoxLayout(inner);
    rows_->setContentsMargins(0, 0, 6, 0);
    rows_->setSpacing(6);
    rows_->addStretch(1);
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);
}

void RelinkPanel::setProblems(const QList<resora::LinkStatus> &problems)
{
    problems_ = problems;
    chosen_.clear();  // every choice was just evaluated; those still failing must be chosen again
    rebuild();
}

void RelinkPanel::assign(const QString &imageId, const QString &path)
{
    chosen_.insert(imageId, QDir::fromNativeSeparators(path));
    rebuild();
    emit changed();
}

void RelinkPanel::assignFolder(const QString &folder)
{
    const QHash<QString, QString> m = resoraui::matchByName(problems_, folder);
    for (auto it = m.begin(); it != m.end(); ++it)
        chosen_.insert(it.key(), it.value());
    rebuild();
    emit changed();
}

void RelinkPanel::locate(const QString &imageId)
{
    const QString f = QFileDialog::getOpenFileName(this, tr("Select the original image"), QString(),
                                                   tr("All files (*)"));
    if (!f.isEmpty())
        assign(imageId, f);
}

void RelinkPanel::rebuild()
{
    while (rows_->count() > 1) {
        QLayoutItem *it = rows_->takeAt(0);
        delete it->widget();
        delete it;
    }
    int done = 0;
    for (const resora::LinkStatus &p : problems_)
        done += chosen_.value(p.imageId).isEmpty() ? 0 : 1;
    summary_->setText(tr("%1 image(s) cannot be used as recorded; %2 replacement(s) chosen. A replacement is "
                         "accepted only if its size and SHA-256 match the recorded image - a file with the same "
                         "name but different content is rejected.")
                          .arg(problems_.size()).arg(done));
    int pos = 0;
    for (const resora::LinkStatus &p : problems_) {
        auto *row = new QFrame;
        row->setObjectName(QStringLiteral("relinkRow"));
        row->setStyleSheet(QStringLiteral("QFrame#relinkRow{background:#15130F;border:1px solid #2A2622;"
                                          "border-left:3px solid %1;}")
                               .arg(chosen_.contains(p.imageId) ? Theme::accentColor() : Theme::dangerColor()));
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(12, 8, 10, 8);
        auto *col = new QVBoxLayout;
        col->setSpacing(1);
        auto *name = new QLabel(QStringLiteral("<b>%1</b>  <span style='color:%2;font-size:8pt'>%3</span>")
                                    .arg(p.name.toHtmlEscaped(), Theme::dangerColor(),
                                         resoraui::stateText(p.state).toUpper()));
        name->setTextFormat(Qt::RichText);
        auto *path = new ElidedLabel;
        path->setText(QDir::toNativeSeparators(p.recordedPath));
        path->setProperty("role", QStringLiteral("mono"));
        path->setToolTip(QDir::toNativeSeparators(p.recordedPath));
        col->addWidget(name);
        col->addWidget(path);
        if (!p.message.isEmpty()) {
            auto *msg = new QLabel(p.message);
            msg->setWordWrap(true);
            msg->setProperty("role", QStringLiteral("muted"));
            col->addWidget(msg);
        }
        if (chosen_.contains(p.imageId)) {
            auto *rep = new ElidedLabel;
            rep->setText(tr("Replacement: %1").arg(QDir::toNativeSeparators(chosen_.value(p.imageId))));
            rep->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::accentColor()));
            col->addWidget(rep);
        }
        h->addLayout(col, 1);
        auto *btn = new ChamferButton(chosen_.contains(p.imageId) ? tr("Change…") : tr("Locate…"));
        btn->setCompact(true);
        btn->setAutoDefault(false);
        btn->setAccessibleName(tr("Locate replacement for %1").arg(p.name));
        const QString id = p.imageId;
        connect(btn, &QPushButton::clicked, this, [this, id] { locate(id); });
        h->addWidget(btn, 0, Qt::AlignTop);
        rows_->insertWidget(pos++, row);
    }
}
