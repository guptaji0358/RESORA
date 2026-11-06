#include "savedlocationspanel.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

#include "chamferbutton.h"
#include "locationrow.h"
#include "ui/theme.h"

SavedLocationsPanel::SavedLocationsPanel(LocationStore *store, QWidget *parent)
    : QWidget(parent), store_(store), name_(new QLineEdit), path_(new QLineEdit),
      browse_(new ChamferButton(tr("Browse"))), add_(new ChamferButton(tr("Add location"))),
      message_(new QLabel), warning_(new QLabel), empty_(new QLabel), list_(new QVBoxLayout)
{
    name_->setPlaceholderText(tr("Display name, e.g. a project or album"));
    name_->setMaxLength(SavedLocations::kMaxNameLength);
    name_->setAccessibleName(tr("New location name"));
    path_->setPlaceholderText(tr("Folder path"));
    path_->setClearButtonEnabled(true);
    path_->setAccessibleName(tr("New location folder"));
    browse_->setToolTip(tr("Choose the folder"));
    static_cast<ChamferButton *>(browse_)->setGlyph(QStringLiteral("folder-open"));
    static_cast<ChamferButton *>(add_)->setGlyph(QStringLiteral("add"));
    Theme::setProperty(add_, "variant", QStringLiteral("primary"));

    warning_->setWordWrap(true);
    warning_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    warning_->setStyleSheet(QStringLiteral("color:%1;font-size:9pt;border-left:3px solid %1;padding:2px 10px;")
                                .arg(Theme::accentColor()));
    message_->setWordWrap(true);
    message_->setAccessibleName(tr("Saved locations status"));
    empty_->setText(tr("Nothing saved yet. Add the folders you use often and RESORA will offer them when you pick a source or a report destination."));
    empty_->setWordWrap(true);
    empty_->setProperty("role", QStringLiteral("muted"));

    auto *caption = new QLabel(tr("ADD A LOCATION"));
    caption->setProperty("role", QStringLiteral("caption"));
    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(path_, 1);
    pathRow->addWidget(browse_);
    auto *addRow = new QHBoxLayout;
    addRow->addWidget(add_);
    addRow->addStretch(1);

    auto *listCaption = new QLabel(tr("YOUR LOCATIONS"));
    listCaption->setProperty("role", QStringLiteral("caption"));
    auto *recheck = new ChamferButton(tr("Re-check folders"));
    recheck->setGlyph(QStringLiteral("refresh"));
    recheck->setToolTip(tr("Look again at which folders are available"));
    auto *listHead = new QHBoxLayout;
    listHead->addWidget(listCaption);
    listHead->addStretch(1);
    listHead->addWidget(recheck);

    auto *holder = new QWidget;
    auto *holderLayout = new QVBoxLayout(holder);
    holderLayout->setContentsMargins(0, 0, 0, 0);
    holderLayout->setSpacing(0);
    holderLayout->addWidget(empty_);
    holderLayout->addLayout(list_);
    holderLayout->addStretch(1);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumHeight(220);
    scroll->setWidget(holder);

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(10);
    col->addWidget(warning_);
    col->addWidget(caption);
    col->addWidget(name_);
    col->addLayout(pathRow);
    col->addLayout(addRow);
    col->addWidget(message_);
    col->addSpacing(6);
    col->addLayout(listHead);
    col->addWidget(scroll, 1);

    connect(browse_, &QPushButton::clicked, this, &SavedLocationsPanel::browseForNew);
    connect(add_, &QPushButton::clicked, this, [this] { addLocation(name_->text(), path_->text()); });
    connect(path_, &QLineEdit::returnPressed, add_, &QPushButton::click);
    connect(name_, &QLineEdit::returnPressed, add_, &QPushButton::click);
    connect(recheck, &QPushButton::clicked, this, [this] {
        rebuild();
        report({}, tr("Folders re-checked."));
    });
    connect(store_, &LocationStore::changed, this, &SavedLocationsPanel::rebuild);

    const bool editable = !store_->isReadOnly();
    for (QWidget *w : {static_cast<QWidget *>(name_), static_cast<QWidget *>(path_), static_cast<QWidget *>(browse_),
                       static_cast<QWidget *>(add_)})
        w->setEnabled(editable);
    rebuild();
}

QString SavedLocationsPanel::message() const { return message_->text(); }
bool SavedLocationsPanel::warningVisible() const { return !warning_->isHidden(); }

void SavedLocationsPanel::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    rebuild();  // folders may have appeared or vanished while the dialog was closed
}

void SavedLocationsPanel::rebuild()
{
    for (LocationRow *r : std::as_const(rows_)) {
        list_->removeWidget(r);
        r->hide();
        r->deleteLater();  // may be the sender of the signal that caused this rebuild
    }
    rows_.clear();
    const bool editable = !store_->isReadOnly();
    for (const SavedLocation &l : store_->locations()) {
        auto *row = new LocationRow(l, SavedLocations::probe(l.path), editable);
        connect(row, &LocationRow::renameRequested, this, &SavedLocationsPanel::renameLocation);
        connect(row, &LocationRow::changeFolderRequested, this, &SavedLocationsPanel::browseToChange);
        connect(row, &LocationRow::removeRequested, this, &SavedLocationsPanel::confirmRemove);
        list_->addWidget(row);
        rows_.append(row);
    }
    empty_->setVisible(rows_.isEmpty());
    const QStringList warnings = store_->warnings();
    warning_->setText(warnings.join(QLatin1Char('\n')));
    warning_->setVisible(!warnings.isEmpty());
}

void SavedLocationsPanel::report(const LocationStore::Result &r, const QString &success)
{
    const bool ok = r.ok();
    message_->setStyleSheet(QStringLiteral("color:%1;").arg(ok ? Theme::successColor() : Theme::dangerColor()));
    message_->setText(ok ? success : r.message);
}

LocationStore::Result SavedLocationsPanel::addLocation(const QString &name, const QString &path)
{
    const auto r = store_->add(name, path);
    report(r, tr("Saved."));
    if (r.ok()) {
        name_->clear();
        path_->clear();
    }
    return r;
}

LocationStore::Result SavedLocationsPanel::renameLocation(const QString &id, const QString &name)
{
    const auto r = store_->rename(id, name);
    report(r, tr("Renamed."));
    return r;
}

LocationStore::Result SavedLocationsPanel::changeLocationPath(const QString &id, const QString &path)
{
    const auto r = store_->setPath(id, path);
    report(r, tr("Folder updated."));
    return r;
}

LocationStore::Result SavedLocationsPanel::removeLocation(const QString &id)
{
    const auto r = store_->remove(id);
    report(r, tr("Removed. The folder itself was not touched."));
    return r;
}

void SavedLocationsPanel::browseForNew()
{
    const QString start = path_->text().trimmed().isEmpty() ? QDir::homePath() : path_->text().trimmed();
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a folder to save"), start);
    if (dir.isEmpty())
        return;
    path_->setText(QDir::toNativeSeparators(dir));
    if (name_->text().trimmed().isEmpty())
        name_->setText(QFileInfo(dir).fileName());  // a sensible default the user can overtype
}

void SavedLocationsPanel::browseToChange(const QString &id)
{
    const SavedLocation l = store_->find(id);
    if (l.id.isEmpty())
        return;
    QString start = l.path;
    while (!QFileInfo(start).isDir() && !QFileInfo(start).isRoot() && start != QFileInfo(start).path())
        start = QFileInfo(start).path();  // nearest existing parent of a moved folder
    const QString dir = QFileDialog::getExistingDirectory(this, tr("New folder for \"%1\"").arg(l.name), start);
    if (!dir.isEmpty())
        changeLocationPath(id, dir);
}

void SavedLocationsPanel::confirmRemove(const QString &id)
{
    const SavedLocation l = store_->find(id);
    if (l.id.isEmpty())
        return;
    if (QMessageBox::question(this, tr("Remove saved location?"),
                              tr("Remove \"%1\" from your saved locations? The folder itself is not touched.").arg(l.name),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        removeLocation(id);
}
