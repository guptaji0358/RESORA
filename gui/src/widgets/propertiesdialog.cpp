#include "propertiesdialog.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProcessEnvironment>
#include <QScrollArea>
#include <QTemporaryFile>
#include <QVBoxLayout>

#include "backend/backendlocator.h"
#include "backend/backendprocess.h"
#include "backend/importselection.h"
#include "chamferbutton.h"
#include "sectioncard.h"
#include "ui/icons.h"
#include "ui/theme.h"

namespace {
QLabel *textLabel(const QString &text, const char *role = nullptr)
{
    auto *l = new QLabel(text);
    if (role)
        l->setProperty("role", QString::fromLatin1(role));
    l->setWordWrap(true);
    return l;
}

SectionCard *cardFor(const PropertySection &s)
{
    auto *card = new SectionCard(s.title);
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(7);
    grid->setColumnMinimumWidth(0, 150);
    grid->setColumnStretch(1, 1);
    int r = 0;
    for (const PropertyRow &row : s.rows) {
        auto *k = new QLabel(row.label);
        k->setProperty("role", QStringLiteral("muted"));
        k->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        k->setWordWrap(true);
        auto *v = new QLabel(row.value);
        v->setObjectName(row.available ? QStringLiteral("propValue") : QStringLiteral("propUnavailable"));
        v->setWordWrap(true);
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        v->setStyleSheet(row.available ? QStringLiteral("color:#EFE9DD;") : QStringLiteral("color:#9A9186; font-style:italic;"));
        if (row.value.size() > 40)
            v->setToolTip(row.value);
        grid->addWidget(k, r, 0);
        grid->addWidget(v, r, 1);
        ++r;
    }
    card->body()->addLayout(grid);
    return card;
}
}  // namespace

PropertiesDialog::PropertiesDialog(const QStringList &files, QWidget *parent)
    : FramelessDialog(tr("Image properties"), parent)
{
    setObjectName(QStringLiteral("propertiesDialog"));
    setMinimumSize(640, 600);

    auto *eyebrow = textLabel(tr("IMAGE PROPERTIES"), "eyebrow");
    title_ = new QLabel(tr("Pick an image."));
    title_->setFont(Theme::display(20, QFont::DemiBold));
    subtitle_ = textLabel(tr("Reads the file header and EXIF only. The image is never modified."), "muted");

    prev_ = new ChamferButton(tr("Prev"));
    prev_->setCompact(true);
    prev_->setAutoDefault(false);
    next_ = new ChamferButton(tr("Next"));
    next_->setCompact(true);
    next_->setAutoDefault(false);
    counter_ = new QLabel;
    counter_->setProperty("role", QStringLiteral("mono"));
    auto *choose = new ChamferButton(tr("Choose image…"));
    choose->setGlyph(QStringLiteral("image"));
    choose->setAutoDefault(false);
    copy_ = new ChamferButton(tr("Copy"));
    copy_->setGlyph(QStringLiteral("report"));
    copy_->setToolTip(tr("Copy these properties as text"));
    copy_->setAutoDefault(false);
    copy_->setEnabled(false);
    connect(choose, &QPushButton::clicked, this, &PropertiesDialog::chooseImages);
    connect(prev_, &QPushButton::clicked, this, [this] { showIndex(index_ - 1); });
    connect(next_, &QPushButton::clicked, this, [this] { showIndex(index_ + 1); });
    connect(copy_, &QPushButton::clicked, this, [this] { QGuiApplication::clipboard()->setText(currentText()); });

    auto *bar = new QHBoxLayout;
    bar->setSpacing(10);
    bar->addWidget(choose);
    bar->addWidget(prev_);
    bar->addWidget(counter_);
    bar->addWidget(next_);
    bar->addStretch(1);
    bar->addWidget(copy_);

    cards_ = new QWidget;
    cardsLayout_ = new QVBoxLayout(cards_);
    cardsLayout_->setContentsMargins(0, 0, 8, 0);
    cardsLayout_->setSpacing(14);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(cards_);

    auto *done = new ChamferButton(tr("Done"));
    done->setProperty("variant", QStringLiteral("primary"));
    done->setAutoDefault(false);
    connect(done, &QPushButton::clicked, this, &QDialog::accept);

    auto *col = bodyLayout();
    col->addWidget(eyebrow);
    col->addWidget(title_);
    col->addWidget(subtitle_);
    col->addSpacing(6);
    col->addLayout(bar);
    col->addWidget(scroll, 1);
    col->addWidget(done, 0, Qt::AlignRight);

    prev_->hide();
    next_->hide();
    counter_->hide();
    showMessage(tr("Choose an image to see its dimensions, colour, density and EXIF data."), false);
    if (!files.isEmpty())
        setFiles(files);
}

PropertiesDialog::~PropertiesDialog()
{
    stopJob();
}

void PropertiesDialog::stopJob()
{
    if (job_) {
        // Let the backend finish its cancel -> kill sequence after this dialog is gone.
        job_->disconnect(this);
        job_->setParent(nullptr);
        connect(job_, &BackendProcess::succeeded, job_, &QObject::deleteLater);
        connect(job_, &BackendProcess::failed, job_, &QObject::deleteLater);
        connect(job_, &BackendProcess::cancelled, job_, &QObject::deleteLater);
        job_->cancel();
        job_ = nullptr;
    }
    delete paramsFile_;
    paramsFile_ = nullptr;
}

void PropertiesDialog::clearCards()
{
    while (QLayoutItem *item = cardsLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

void PropertiesDialog::showMessage(const QString &text, bool error)
{
    clearCards();
    auto *l = textLabel(text, error ? "error" : "muted");
    cardsLayout_->addWidget(l);
    cardsLayout_->addStretch(1);
}

void PropertiesDialog::chooseImages()
{
    const QStringList picked = QFileDialog::getOpenFileNames(this, tr("Choose images"), QString(), ImportSelection::dialogFilter());
    if (!picked.isEmpty())
        setFiles(picked);
}

void PropertiesDialog::setFiles(const QStringList &files)
{
    stopJob();
    images_.clear();
    index_ = -1;
    copy_->setEnabled(false);
    prev_->hide();
    next_->hide();
    counter_->hide();
    if (files.isEmpty())
        return;

    const BackendLocation location =
        locateBackend(QProcessEnvironment::systemEnvironment(), QCoreApplication::applicationDirPath());
    if (!location.ok()) {
        title_->setText(tr("Backend not found"));
        showMessage(location.error, true);
        return;
    }
    // The path list travels in a params file (no command-line length/quoting limits).
    auto *file = new QTemporaryFile(QDir::temp().filePath(QStringLiteral("resora-props-XXXXXX.json")), this);
    QJsonArray list;
    for (const QString &f : files)
        list.append(QDir::toNativeSeparators(QFileInfo(f).absoluteFilePath()));
    const QByteArray body = QJsonDocument(QJsonObject{{QStringLiteral("files"), list}}).toJson(QJsonDocument::Compact);
    if (!file->open() || file->write(body) < 0 || !file->flush()) {
        title_->setText(tr("Cannot read properties"));
        showMessage(tr("Could not write the temporary selection file: %1").arg(file->errorString()), true);
        delete file;
        return;
    }
    file->close();
    paramsFile_ = file;

    title_->setText(files.size() == 1 ? QFileInfo(files.first()).fileName() : tr("%1 images").arg(files.size()));
    showMessage(tr("Reading image properties…"), false);
    job_ = new BackendProcess(this);
    connect(job_, &BackendProcess::succeeded, this, &PropertiesDialog::onSucceeded);
    connect(job_, &BackendProcess::failed, this,
            [this](const QString &, const QString &message, int, const QString &) { onFailed(QString(), message); });
    connect(job_, &BackendProcess::cancelled, this, [this] { onFailed(QString(), tr("Cancelled.")); });
    job_->start(location, {QStringLiteral("--params-file"), paramsFile_->fileName(), QStringLiteral("properties")});
}

void PropertiesDialog::onSucceeded(const QJsonValue &data)
{
    if (job_)
        job_->deleteLater();
    job_ = nullptr;
    delete paramsFile_;
    paramsFile_ = nullptr;
    showImages(PropertiesModel::fromResult(data.toObject()));
}

void PropertiesDialog::onFailed(const QString &, const QString &message)
{
    if (job_)
        job_->deleteLater();
    job_ = nullptr;
    delete paramsFile_;
    paramsFile_ = nullptr;
    title_->setText(tr("Cannot read properties"));
    showMessage(message, true);
}

void PropertiesDialog::showImages(const QList<ImageProperties> &images)
{
    images_ = images;
    const bool many = images_.size() > 1;
    prev_->setVisible(many);
    next_->setVisible(many);
    counter_->setVisible(many);
    if (images_.isEmpty()) {
        showMessage(tr("The backend returned no images."), true);
        return;
    }
    showIndex(0);
}

void PropertiesDialog::showIndex(int index)
{
    if (index < 0 || index >= images_.size())
        return;
    index_ = index;
    const ImageProperties &p = images_.at(index);
    title_->setText(p.name.isEmpty() ? tr("Image") : p.name);
    counter_->setText(QStringLiteral("%1 / %2").arg(index + 1).arg(images_.size()));
    prev_->setEnabled(index > 0);
    next_->setEnabled(index + 1 < images_.size());
    copy_->setEnabled(true);

    clearCards();
    if (!p.ok()) {
        cardsLayout_->addWidget(textLabel(tr("This image cannot be analysed (%1): %2").arg(p.status, p.error), "error"));
        cardsLayout_->addStretch(1);
        return;
    }
    for (const QString &w : p.warnings)
        cardsLayout_->addWidget(textLabel(w, "muted"));
    for (const PropertySection &s : p.sections)
        cardsLayout_->addWidget(cardFor(s));
    cardsLayout_->addStretch(1);
}

QString PropertiesDialog::currentText() const
{
    return index_ >= 0 && index_ < images_.size() ? images_.at(index_).toPlainText() : QString();
}
