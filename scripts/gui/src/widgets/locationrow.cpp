#include "widgets/locationrow.h"

#include <functional>

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include "widgets/chamferbutton.h"
#include "widgets/elidedlabel.h"
#include "ui/icons.h"
#include "ui/theme.h"

namespace {
// Row actions are the shared ChamferButton in its compact size: same states and hierarchy as every
// other button (ghost outline; red outline for the destructive one).
ChamferButton *actionButton(const QString &text, const QString &tip, const QString &glyph, bool danger)
{
    auto *b = new ChamferButton(text);
    b->setCompact(true);
    b->setGlyph(glyph);
    b->setToolTip(tip);
    b->setAccessibleName(text);
    if (danger)
        b->setProperty("variant", QStringLiteral("danger"));
    return b;
}

// Esc cancels the inline rename.
class NameEdit : public QLineEdit
{
public:
    std::function<void()> onCancel;
protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape && onCancel)
            onCancel();
        else
            QLineEdit::keyPressEvent(e);
    }
};
}  // namespace

LocationRow::LocationRow(const SavedLocation &location, LocationStatus status, bool editable, QWidget *parent)
    : QFrame(parent), location_(location), status_(status),
      name_(new QLabel(location.name)), nameEdit_(new NameEdit), path_(new ElidedLabel),
      note_(new QLabel(SavedLocations::statusText(status))),
      rename_(actionButton(tr("Rename"), tr("Change the display name"), QStringLiteral("rename"), false)),
      change_(actionButton(tr("Change folder"), tr("Point this location at a different folder"),
                           QStringLiteral("swap"), false)),
      remove_(actionButton(tr("Remove"), tr("Remove this shortcut (the folder itself is not touched)"),
                           QStringLiteral("remove"), true))
{
    setObjectName(QStringLiteral("locationRow"));
    setMinimumHeight(70);
    const bool ok = SavedLocations::isUsable(status);

    name_->setFont(Theme::display(12, QFont::DemiBold, 0.4));
    name_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    name_->setAccessibleName(tr("Location name"));
    nameEdit_->setMaxLength(SavedLocations::kMaxNameLength);
    nameEdit_->setAccessibleName(tr("New location name"));
    nameEdit_->hide();
    static_cast<NameEdit *>(nameEdit_)->onCancel = [this] { endRename(false); };
    path_->setFont(Theme::mono(9));
    path_->setProperty("role", QStringLiteral("muted"));
    path_->setFullText(location.displayPath());
    path_->setAccessibleName(tr("Folder path"));
    note_->setStyleSheet(QStringLiteral("color:%1;font-size:8.5pt;").arg(Theme::dangerColor()));
    note_->setWordWrap(true);
    note_->setVisible(!ok);

    auto *text = new QVBoxLayout;
    text->setSpacing(2);
    text->addWidget(name_);
    text->addWidget(nameEdit_);
    text->addWidget(path_);
    text->addWidget(note_);

    // One fixed-size horizontal strip: never wraps. When the row gets narrow the labels collapse to
    // icons (see resizeEvent) instead of the buttons reflowing.
    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    actions->addWidget(rename_);
    actions->addWidget(change_);
    actions->addWidget(remove_);

    auto *glyph = new QLabel;
    glyph->setFixedSize(24, 24);
    glyph->setAccessibleName(ok ? tr("Folder available") : tr("Folder unavailable"));
    glyph->setPixmap(ok ? Icons::pixmap(QStringLiteral("folder"), 24, Theme::paper(), Theme::ember(), devicePixelRatioF())
                        : Icons::pixmap(QStringLiteral("warning"), 24, Theme::alert(), Theme::alert(), devicePixelRatioF()));
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(18, 8, 8, 8);
    row->setSpacing(14);
    row->addWidget(glyph, 0, Qt::AlignVCenter);
    row->addLayout(text, 1);
    row->addLayout(actions);

    for (ChamferButton *b : {rename_, change_, remove_})
        b->setEnabled(editable);
    connect(rename_, &QPushButton::clicked, this, &LocationRow::beginRename);
    connect(change_, &QPushButton::clicked, this, [this] { emit changeFolderRequested(location_.id); });
    connect(remove_, &QPushButton::clicked, this, [this] { emit removeRequested(location_.id); });
    connect(nameEdit_, &QLineEdit::returnPressed, this, [this] { endRename(true); });
}

QString LocationRow::statusMessage() const
{
    return SavedLocations::isUsable(status_) ? QString() : SavedLocations::statusText(status_);
}

QString LocationRow::fullPath() const
{
    return path_->fullText();
}

void LocationRow::beginRename()
{
    nameEdit_->setText(location_.name);
    name_->hide();
    nameEdit_->show();
    nameEdit_->setFocus();
    nameEdit_->selectAll();
}

void LocationRow::endRename(bool commit)
{
    const QString text = nameEdit_->text();
    nameEdit_->hide();
    name_->show();
    if (commit && text.simplified() != location_.name)
        emit renameRequested(location_.id, text);
}

void LocationRow::resizeEvent(QResizeEvent *e)
{
    QFrame::resizeEvent(e);
    // Keep at least ~170px for name/path next to the actions; below that, icons only.
    int full = 0;
    for (ChamferButton *b : {rename_, change_, remove_}) {
        const bool was = b->isIconOnly();
        b->setIconOnly(false);
        full += b->sizeHint().width() + 8;
        b->setIconOnly(was);
    }
    const bool compact = width() - (18 + 8 + 24 + 14 + 14) - full < 170;
    for (ChamferButton *b : {rename_, change_, remove_})
        b->setIconOnly(compact);  // tooltip + accessible name keep the action discoverable
}

void LocationRow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Theme::panel());
    const QColor bar = SavedLocations::isUsable(status_) ? Theme::signal() : Theme::alert();
    p.fillRect(QRect(0, 0, 3, height()), bar);
    p.fillRect(QRect(0, height() - 1, width(), 1), Theme::line());
}
