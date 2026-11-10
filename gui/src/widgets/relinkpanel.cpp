#include "widgets/relinkpanel.h"

#include <QAbstractItemView>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include "ui/theme.h"
#include "widgets/chamferbutton.h"
#include "widgets/confirmdialog.h"

using resoraui::RelinkModel;

namespace {
enum Column { ColImage, ColRole, ColStatus, ColActions, ColCount };

QString roleText(const QStringList &roles)
{
    QStringList out;
    for (const QString &r : roles)
        out << (r.isEmpty() ? r : r.left(1).toUpper() + r.mid(1));
    return out.isEmpty() ? QObject::tr("(none)") : out.join(QStringLiteral(", "));
}

QString statusColor(RelinkModel::Status s)
{
    switch (s) {
    case RelinkModel::Status::Resolved: return Theme::successColor();
    case RelinkModel::Status::ReplacementChosen: return Theme::accentColor();
    case RelinkModel::Status::Removed: return QStringLiteral("#8A8378");
    default: return Theme::dangerColor();
    }
}
}  // namespace

RelinkPanel::RelinkPanel(QWidget *parent) : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    summary_ = new QLabel;
    summary_->setWordWrap(true);
    summary_->setProperty("role", QStringLiteral("muted"));
    outer->addWidget(summary_);

    table_ = new QTableWidget(0, ColCount);
    table_->setObjectName(QStringLiteral("relinkTable"));
    table_->setHorizontalHeaderLabels({tr("Image"), tr("Used as"), tr("Status"), tr("Actions")});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);  // narrow windows scroll, never clip
    table_->setShowGrid(false);
    table_->setFrameShape(QFrame::NoFrame);
    table_->verticalHeader()->hide();
    table_->verticalHeader()->setDefaultSectionSize(44);
    table_->setMinimumHeight(150);
    QHeaderView *h = table_->horizontalHeader();
    h->setSectionResizeMode(ColImage, QHeaderView::Stretch);
    h->setSectionResizeMode(ColRole, QHeaderView::ResizeToContents);
    h->setSectionResizeMode(ColStatus, QHeaderView::ResizeToContents);
    h->setSectionResizeMode(ColActions, QHeaderView::ResizeToContents);
    h->setMinimumSectionSize(90);
    table_->setStyleSheet(QStringLiteral(
        "QTableWidget#relinkTable{background:#15130F;color:#EFE9DD;border:1px solid #2A2622;}"
        "QTableWidget#relinkTable::item{padding:2px 8px;border-bottom:1px solid #2A2622;}"
        "QTableWidget#relinkTable::item:selected{background:#241F19;color:#EFE9DD;}"
        "QHeaderView::section{background:#1B1814;color:#A59D8F;border:0;border-bottom:1px solid #2A2622;padding:4px 8px;}"));
    outer->addWidget(table_, 1);

    detail_ = new QLabel;
    detail_->setObjectName(QStringLiteral("relinkDetail"));
    detail_->setWordWrap(true);
    detail_->setProperty("role", QStringLiteral("muted"));
    detail_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    outer->addWidget(detail_);
    connect(table_, &QTableWidget::currentCellChanged, this, [this](int row) {
        const QTableWidgetItem *it = row >= 0 ? table_->item(row, ColImage) : nullptr;
        detail_->setText(it ? it->toolTip() : QString());
    });

    confirm_ = [this](const QString &name, const QStringList &roles) {
        return ConfirmDialog::ask(this, tr("Remove from dashboard"),
                                  tr("Remove \"%1\" from this dashboard?\n\nThe dashboard will open without the image "
                                     "association (%2). The original image file is not deleted or changed, and the "
                                     ".resora file stays as it is. Everything else in the dashboard is kept.")
                                      .arg(name, roleText(roles)),
                                  tr("Remove from Dashboard"));
    };
}

void RelinkPanel::setProblems(const QList<resora::LinkStatus> &problems)
{
    model_.update(problems);
    rebuild();
}

void RelinkPanel::reset()
{
    model_.clear();
    rebuild();
}

bool RelinkPanel::assign(const QString &imageId, const QString &path)
{
    if (!model_.assign(imageId, path))
        return false;
    rebuild();
    emit changed();
    return true;
}

void RelinkPanel::assignFolder(const QString &folder)
{
    const QHash<QString, QString> m = resoraui::matchByName(model_.unresolved(), folder);
    for (auto it = m.begin(); it != m.end(); ++it)
        model_.assign(it.key(), it.value());
    rebuild();
    emit changed();
}

bool RelinkPanel::clearReplacement(const QString &imageId)
{
    if (!model_.clearReplacement(imageId))
        return false;
    rebuild();
    emit changed();
    return true;
}

bool RelinkPanel::requestRemove(const QString &imageId)
{
    const RelinkModel::Row *r = model_.find(imageId);
    if (!r || !confirm_ || !confirm_(r->name, r->roles))
        return false;
    model_.remove(imageId);
    rebuild();
    emit changed();
    return true;
}

void RelinkPanel::locate(const QString &imageId)
{
    const QString f = QFileDialog::getOpenFileName(this, tr("Select the original image"), QString(), tr("All files (*)"));
    if (!f.isEmpty())
        assign(imageId, f);
}

void RelinkPanel::rebuild()
{
    using S = RelinkModel::Status;
    const QString keep = table_->currentRow() >= 0 && table_->currentRow() < model_.rows().size()
                             ? model_.rows().at(table_->currentRow()).imageId : QString();
    table_->setRowCount(0);
    table_->setRowCount(model_.rows().size());
    int unresolved = model_.count(S::Missing) + model_.count(S::WrongImage);
    summary_->setText(model_.rows().isEmpty()
                          ? QString()
                          : tr("%1 image(s) cannot be used as recorded: %2 unresolved, %3 replacement(s) chosen, "
                               "%4 resolved, %5 removed from the dashboard. A replacement is accepted only if its size "
                               "and SHA-256 match the recorded image. Opening stays blocked until every image is "
                               "resolved or removed.")
                                .arg(model_.rows().size()).arg(unresolved).arg(model_.count(S::ReplacementChosen))
                                .arg(model_.count(S::Resolved)).arg(model_.count(S::Removed)));
    int row = 0, keepRow = -1;
    for (const RelinkModel::Row &r : model_.rows()) {
        if (r.imageId == keep)
            keepRow = row;
        auto *name = new QTableWidgetItem(r.name);
        QString tip = tr("%1\nRecorded path: %2").arg(r.name, QDir::toNativeSeparators(r.recordedPath));
        if (!r.message.isEmpty())
            tip += tr("\nReason: %1").arg(r.message);
        if (!r.replacement.isEmpty())
            tip += tr("\nReplacement: %1").arg(QDir::toNativeSeparators(r.replacement));
        if (r.status == S::Removed)
            tip += tr("\nRemoved from this dashboard. The image file itself is untouched.");
        name->setToolTip(tip);
        table_->setItem(row, ColImage, name);
        auto *role = new QTableWidgetItem(roleText(r.roles));
        role->setToolTip(tip);
        table_->setItem(row, ColRole, role);
        auto *st = new QTableWidgetItem(RelinkModel::statusText(r.status));
        st->setForeground(QColor(statusColor(r.status)));
        st->setToolTip(tip);
        table_->setItem(row, ColStatus, st);

        auto *cell = new QWidget;
        auto *hl = new QHBoxLayout(cell);
        hl->setContentsMargins(4, 4, 4, 4);
        hl->setSpacing(6);
        const QString id = r.imageId;
        const bool removed = r.status == S::Removed;
        auto addButton = [&](const QString &text, const QString &accessible, bool enabled, auto &&fn) {
            auto *b = new ChamferButton(text);
            b->setCompact(true);
            b->setAutoDefault(false);
            b->setEnabled(enabled);
            b->setAccessibleName(accessible.arg(r.name));
            connect(b, &QPushButton::clicked, this, fn);
            hl->addWidget(b);
            return b;
        };
        addButton(r.replacement.isEmpty() ? tr("Choose Replacement") : tr("Change Replacement"),
                  tr("Choose replacement for %1"), !removed, [this, id] { locate(id); });
        if (!r.replacement.isEmpty())
            addButton(tr("Clear"), tr("Clear replacement for %1"), true, [this, id] { clearReplacement(id); });
        addButton(tr("Remove from Dashboard"), tr("Remove %1 from the dashboard"), !removed,
                  [this, id] { requestRemove(id); });
        table_->setCellWidget(row, ColActions, cell);
        ++row;
    }
    if (keepRow >= 0)
        table_->setCurrentCell(keepRow, ColImage);
    else
        detail_->clear();
}
