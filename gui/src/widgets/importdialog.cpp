#include "importdialog.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

#include "ui/theme.h"

namespace {
// A tile is a real QPushButton (focus, Enter/Space, accessibility) with two labels on top of it.
QPushButton *tileButton(const QString &key, const QString &title, const QString &text, const QString &keyHint)
{
    auto *b = new QPushButton;
    b->setObjectName(QStringLiteral("importTile_") + key);
    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumHeight(78);
    b->setAccessibleName(title);
    b->setToolTip(text);
    b->setStyleSheet(QStringLiteral(
        "QPushButton { background:#15130F; border:1px solid #2A2622; border-left:3px solid #2A2622; text-align:left; }"
        "QPushButton:hover, QPushButton:focus { background:#1B1814; border-color:#FF5B2E; outline:none; }"));
    auto *col = new QVBoxLayout(b);
    col->setContentsMargins(18, 10, 18, 10);
    col->setSpacing(2);
    auto *head = new QLabel(title.toUpper() + QStringLiteral("   <span style='color:#9A9186'>%1</span>").arg(keyHint));
    head->setTextFormat(Qt::RichText);
    head->setFont(Theme::display(11, QFont::DemiBold, 2));
    head->setStyleSheet(QStringLiteral("color:#EFE9DD;background:transparent;"));
    auto *sub = new QLabel(text);
    sub->setWordWrap(true);
    sub->setStyleSheet(QStringLiteral("color:#9A9186;background:transparent;font-size:9pt;"));
    for (QLabel *l : {head, sub})
        l->setAttribute(Qt::WA_TransparentForMouseEvents);
    col->addWidget(head);
    col->addWidget(sub);
    return b;
}
}  // namespace

ImportDialog::ImportDialog(const QString &startDir, QWidget *parent) : QDialog(parent), startDir_(startDir)
{
    setObjectName(QStringLiteral("importDialog"));
    setWindowTitle(tr("Import"));
    setModal(true);
    setMinimumWidth(520);
    setStyleSheet(QStringLiteral("QDialog{background:#0D0C0B;}"));

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(32, 28, 32, 24);
    col->setSpacing(10);
    auto *eyebrow = new QLabel(tr("IMPORT"));
    eyebrow->setProperty("role", QStringLiteral("eyebrow"));
    auto *head = new QLabel(tr("What do you want to analyze?"));
    head->setFont(Theme::display(18, QFont::DemiBold));
    head->setStyleSheet(QStringLiteral("color:#EFE9DD;"));
    auto *note = new QLabel(tr("Only image headers are read; your originals are never modified."));
    note->setProperty("role", QStringLiteral("muted"));
    note->setWordWrap(true);
    col->addWidget(eyebrow);
    col->addWidget(head);
    col->addWidget(note);
    col->addSpacing(8);

    col->addWidget(makeTile(QStringLiteral("folder"), tr("Import Folder"),
                            tr("Every image in a folder, optionally including its subfolders."), QKeySequence(Qt::Key_1),
                            &ImportDialog::chooseFolder));
    col->addWidget(makeTile(QStringLiteral("single"), tr("Import Single Image"),
                            tr("Analyze one image file."), QKeySequence(Qt::Key_2), &ImportDialog::chooseSingle));
    col->addWidget(makeTile(QStringLiteral("multiple"), tr("Import Multiple Images"),
                            tr("Pick any number of image files."),
                            QKeySequence(Qt::Key_3), &ImportDialog::chooseMultiple));

    auto *cancel = new QPushButton(tr("Cancel"));
    cancel->setAutoDefault(false);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    col->addSpacing(6);
    col->addWidget(cancel, 0, Qt::AlignRight);
}

QWidget *ImportDialog::makeTile(const QString &key, const QString &title, const QString &text,
                                const QKeySequence &shortcut, void (ImportDialog::*slot)())
{
    auto *b = tileButton(key, title, text, shortcut.toString());
    connect(b, &QPushButton::clicked, this, slot);
    auto *sc = new QShortcut(shortcut, this);
    connect(sc, &QShortcut::activated, b, &QPushButton::click);
    return b;
}

void ImportDialog::chooseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select folder to scan"), startDir_);
    if (!dir.isEmpty())
        applyFolder(dir);
}

void ImportDialog::chooseSingle()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Select an image"), startDir_,
                                                      ImportSelection::dialogFilter());
    if (!file.isEmpty())
        applyFiles({file});
}

void ImportDialog::chooseMultiple()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Select images"), startDir_,
                                                            ImportSelection::dialogFilter());
    if (!files.isEmpty())
        applyFiles(files);
}

void ImportDialog::applyFolder(const QString &path)
{
    selection_ = ImportSelection::fromFolder(path);
    accept();  // an invalid folder is explained on the Source page, where it can be changed
}

void ImportDialog::applyFiles(const QStringList &paths)
{
    selection_ = ImportSelection::fromFiles(paths);
    accept();  // skipped / missing files are listed on the Source page, never dropped silently
}
