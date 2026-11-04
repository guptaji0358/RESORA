#include "sourcepicker.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QLabel>
#include <QMimeData>
#include <QPushButton>
#include <QVBoxLayout>

#include "chamferbutton.h"
#include "elidedlabel.h"
#include "importdialog.h"
#include "ui/theme.h"

SourcePicker::SourcePicker(QWidget *parent)
    : QWidget(parent),
      kind_(new QLabel(this)),
      title_(new QLabel(this)),
      location_(new ElidedLabel(this)),
      detail_(new QLabel(this)),
      notes_(new QLabel(this)),
      change_(new ChamferButton(tr("Import..."), this))
{
    setAcceptDrops(true);
    setObjectName(QStringLiteral("sourcePicker"));
    kind_->setProperty("role", QStringLiteral("caption"));
    title_->setFont(Theme::display(16, QFont::DemiBold));
    title_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    title_->setWordWrap(true);
    location_->setFont(Theme::mono(10));
    location_->setProperty("role", QStringLiteral("muted"));
    detail_->setProperty("role", QStringLiteral("muted"));
    notes_->setProperty("role", QStringLiteral("error"));
    notes_->setWordWrap(true);
    notes_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    change_->setAccessibleName(tr("Import or change the selection"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(kind_);
    layout->addWidget(title_);
    layout->addWidget(location_);
    layout->addWidget(detail_);
    layout->addWidget(notes_);
    layout->addSpacing(4);
    layout->addWidget(change_, 0, Qt::AlignLeft);

    connect(change_, &QPushButton::clicked, this, &SourcePicker::openImportDialog);
    refresh();
}

void SourcePicker::setSelection(const ImportSelection &selection)
{
    selection_ = selection;
    refresh();
    emit selectionChanged();
}

QString SourcePicker::validationError() const
{
    if (selection_.kind == ImportSelection::None && selection_.problem.isEmpty())
        return tr("Import a folder or images to scan (Ctrl+O).");
    return selection_.problem;
}

void SourcePicker::setBusy(bool busy)
{
    busy_ = busy;
    change_->setEnabled(!busy);
    setAcceptDrops(!busy);
}

void SourcePicker::openImportDialog()
{
    if (busy_)
        return;
    QString start;
    if (selection_.kind == ImportSelection::Folder)
        start = selection_.folder;
    else if (!selection_.files.isEmpty())
        start = QFileInfo(selection_.files.first()).absolutePath();
    ImportDialog dialog(start, window());
    if (dialog.exec() == QDialog::Accepted)
        setSelection(dialog.selection());
}

void SourcePicker::refresh()
{
    const ImportSelection &s = selection_;
    const bool has = s.isValid();
    kind_->setText(tr("SOURCE  /  ") + s.kindLabel());
    if (has) {
        title_->setText(s.title());
        location_->setFullText(s.location());
        QString detail;
        switch (s.kind) {
        case ImportSelection::Folder: detail = tr("Folder import"); break;
        case ImportSelection::SingleImage: detail = tr("1 image selected"); break;
        default: detail = tr("%1 images selected").arg(s.imageCount()); break;
        }
        detail_->setText(detail);
    } else {
        title_->setText(tr("Nothing imported yet"));
        location_->setFullText(tr("Press Ctrl+O, or drop a folder or images here."));
        detail_->setText(QString());
    }
    location_->setVisible(true);
    detail_->setVisible(has);

    QStringList lines = s.notes();
    if (!s.problem.isEmpty())
        lines.prepend(s.problem);
    notes_->setText(lines.join(QLatin1Char('\n')));
    notes_->setVisible(!lines.isEmpty());
    change_->setText(has ? tr("Change selection...") : tr("Import..."));
    change_->setToolTip(tr("Import a folder, a single image or several images (Ctrl+O)"));
}

void SourcePicker::dragEnterEvent(QDragEnterEvent *e)
{
    if (!busy_ && e->mimeData()->hasUrls())
        e->acceptProposedAction();
}

void SourcePicker::dropEvent(QDropEvent *e)
{
    QStringList paths;
    for (const QUrl &u : e->mimeData()->urls())
        if (u.isLocalFile())
            paths << u.toLocalFile();
    if (!paths.isEmpty())
        setSelection(ImportSelection::fromPaths(paths));
    e->acceptProposedAction();
}
