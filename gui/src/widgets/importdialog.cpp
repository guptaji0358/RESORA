#include "importdialog.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "chamferbutton.h"
#include <QShortcut>
#include <QVBoxLayout>

#include "locations/locationstore.h"
#include "locationsuggester.h"
#include "ui/icons.h"
#include "ui/theme.h"

namespace {
// A tile is a real QPushButton (focus, Enter/Space, accessibility) with two labels on top of it.
QPushButton *tileButton(const QString &key, const QString &title, const QString &text, const QString &keyHint)
{
    auto *b = new QPushButton;
    b->setObjectName(QStringLiteral("importTile_") + key);
    b->setCursor(Qt::PointingHandCursor);
    b->setAutoDefault(false);  // Enter never triggers a tile implicitly; 1/2/3, click and Space do
    b->setMinimumHeight(78);
    b->setAccessibleName(title);
    b->setToolTip(text);
    b->setStyleSheet(QStringLiteral(
        "QPushButton { background:#15130F; border:1px solid #2A2622; border-left:3px solid #2A2622; text-align:left; }"
        "QPushButton:hover, QPushButton:focus { background:#1B1814; border-color:#FF5B2E; outline:none; }"));
    const QString glyph = key == QLatin1String("folder") ? QStringLiteral("folder")
                          : key == QLatin1String("single") ? QStringLiteral("image") : QStringLiteral("images");
    auto *iconLabel = new QLabel;
    iconLabel->setFixedSize(30, 30);
    iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    iconLabel->setStyleSheet(QStringLiteral("background:transparent;"));
    iconLabel->setPixmap(Icons::pixmap(glyph, 30, Theme::paper(), Theme::ember(), b->devicePixelRatioF()));
    auto *outer = new QHBoxLayout(b);
    outer->setContentsMargins(18, 10, 18, 10);
    outer->setSpacing(16);
    outer->addWidget(iconLabel);
    auto *col = new QVBoxLayout;
    col->setSpacing(2);
    outer->addLayout(col, 1);
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

ImportDialog::ImportDialog(const QString &startDir, QWidget *parent, const LocationStore *store)
    : FramelessDialog(tr("Import"), parent), startDir_(startDir)
{
    setObjectName(QStringLiteral("importDialog"));
    setMinimumWidth(520);

    auto *col = bodyLayout();
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

    // Folder path input: type or paste a path, or pick one of your Saved Locations. Enter imports it.
    auto *pathCaption = new QLabel(tr("OR ENTER A FOLDER PATH"));
    pathCaption->setProperty("role", QStringLiteral("caption"));
    path_ = new QLineEdit;
    path_->setClearButtonEnabled(true);
    path_->setAccessibleName(tr("Folder path"));
    path_->setPlaceholderText(store && !store->locations().isEmpty() ? tr("Click to choose a saved location, or type a path")
                                                                      : tr("Type or paste a folder path, then press Enter"));
    col->addSpacing(6);
    col->addWidget(pathCaption);
    col->addWidget(path_);
    if (store)
        new LocationSuggester(path_, store, {LocationRecommender::Kind::Folder, QString()});
    connect(path_, &QLineEdit::returnPressed, this, &ImportDialog::importTypedFolder);

    auto *cancel = new ChamferButton(tr("Cancel"));
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

void ImportDialog::importTypedFolder()
{
    const QString text = path_->text().trimmed();
    if (!text.isEmpty())
        applyFolder(QDir::fromNativeSeparators(text));
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
