#include "widgets/comparisonworkspace.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QProcessEnvironment>
#include <QTemporaryFile>
#include <QThreadPool>

#include "backend/backendlocator.h"
#include "backend/backendprocess.h"
#include "widgets/comparisontable.h"
#include "widgets/comparisonviewer.h"

namespace {
constexpr int kMaxCollectionFiles = 5000;

QString normal(const QString &path)
{
    return QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
}

QJsonObject failure(const QString &path, const QString &message)
{
    return QJsonObject{{QStringLiteral("path"), path},
                       {QStringLiteral("name"), QFileInfo(path).fileName()},
                       {QStringLiteral("status"), QStringLiteral("unreadable")},
                       {QStringLiteral("error"), message}};
}
}  // namespace

ComparisonWorkspace::ComparisonWorkspace(QWidget *parent) : QWidget(parent)
{
    buildUi();
    refresh();
}

ComparisonWorkspace::~ComparisonWorkspace()
{
    for (auto it = jobs_.begin(); it != jobs_.end(); ++it) {
        if (BackendProcess *job = it.value()) {
            job->disconnect(this);
            job->setParent(nullptr);  // let it finish its cancel -> kill sequence on its own
            connect(job, &BackendProcess::succeeded, job, &QObject::deleteLater);
            connect(job, &BackendProcess::failed, job, &QObject::deleteLater);
            connect(job, &BackendProcess::cancelled, job, &QObject::deleteLater);
            job->cancel();
        }
    }
    qDeleteAll(paramFiles_);
}

QString ComparisonWorkspace::setImage(Side side, const QString &rawPath)
{
    QString error;
    const QFileInfo info(rawPath);
    if (rawPath.trimmed().isEmpty())
        error = tr("No file was chosen.");
    else if (!info.exists() || !info.isFile())
        error = tr("%1 does not exist or is not a file.").arg(QDir::toNativeSeparators(rawPath));
    else if (!ImportSelection::isSupportedExtension(rawPath))
        error = tr("%1 is not a supported image type.").arg(info.fileName());
    if (!error.isEmpty()) {
        setNotice(error);
        return error;
    }
    setNotice(QString());
    paths_[side] = normal(rawPath);
    ensureLoaded(paths_[side]);
    trimCaches();
    refresh();
    return QString();
}

void ComparisonWorkspace::clearImage(Side side)
{
    paths_[side].clear();
    setNotice(QString());
    trimCaches();
    refresh();
}

void ComparisonWorkspace::swapImages()
{
    std::swap(paths_[0], paths_[1]);  // caches are keyed by path: nothing is decoded again
    std::swap(shown_[0], shown_[1]);
    // Panes still hold the old content; force a re-show with each path's cached preview.
    shown_[0].clear();
    shown_[1].clear();
    refresh();
}

void ComparisonWorkspace::ensureLoaded(const QString &path)
{
    if (!previews_.contains(path) && !pendingPreviews_.contains(path)) {
        pendingPreviews_.insert(path);
        QPointer<ComparisonWorkspace> self(this);
        QThreadPool::globalInstance()->start([self, path] {
            PreviewResult result = PreviewLoader::load(path);
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self.data(),
                [self, path, result] {
                    if (!self)
                        return;
                    self->pendingPreviews_.remove(path);
                    if (self->paths_[0] == path || self->paths_[1] == path)
                        self->previews_.insert(path, result);
                    self->refresh();
                },
                Qt::QueuedConnection);
        });
    }
    if (!props_.contains(path) && !pendingProps_.contains(path))
        startProperties(path);
}

void ComparisonWorkspace::startProperties(const QString &path)
{
    const BackendLocation location =
        locateBackend(QProcessEnvironment::systemEnvironment(), QCoreApplication::applicationDirPath());
    if (!location.ok()) {
        props_.insert(path, failure(path, location.error));
        return;
    }
    auto *file = new QTemporaryFile(QDir::temp().filePath(QStringLiteral("resora-cmp-XXXXXX.json")), this);
    const QByteArray body =
        QJsonDocument(QJsonObject{{QStringLiteral("files"), QJsonArray{path}}}).toJson(QJsonDocument::Compact);
    if (!file->open() || file->write(body) < 0 || !file->flush()) {
        props_.insert(path, failure(path, tr("Could not write the temporary selection file.")));
        delete file;
        return;
    }
    file->close();
    pendingProps_.insert(path);
    paramFiles_.insert(path, file);

    auto *job = new BackendProcess(this);
    jobs_.insert(path, job);
    auto done = [this, path, job](const QJsonObject &props) {
        job->deleteLater();
        jobs_.remove(path);
        delete paramFiles_.take(path);
        pendingProps_.remove(path);
        onPropsDone(path, props);
    };
    connect(job, &BackendProcess::succeeded, this, [done, path](const QJsonValue &data) {
        const QJsonArray images = data.toObject().value(QStringLiteral("images")).toArray();
        done(images.isEmpty() ? failure(path, QObject::tr("The backend returned no result.")) : images.first().toObject());
    });
    connect(job, &BackendProcess::failed, this,
            [done, path](const QString &, const QString &message, int, const QString &) { done(failure(path, message)); });
    connect(job, &BackendProcess::cancelled, this, [done, path] { done(failure(path, QObject::tr("Cancelled."))); });
    job->start(location, {QStringLiteral("--params-file"), file->fileName(), QStringLiteral("properties")});
}

void ComparisonWorkspace::onPropsDone(const QString &path, const QJsonObject &props)
{
    if (paths_[0] == path || paths_[1] == path)
        props_.insert(path, props);
    refresh();
}

void ComparisonWorkspace::trimCaches()
{
    auto keep = [this](const QString &p) { return p == paths_[0] || p == paths_[1]; };
    previews_.removeIf([&](auto it) { return !keep(it.key()); });
    props_.removeIf([&](auto it) { return !keep(it.key()); });
    for (auto it = jobs_.begin(); it != jobs_.end(); ++it)  // stale metadata jobs: stop early
        if (!keep(it.key()) && it.value())
            it.value()->cancel();
}

void ComparisonWorkspace::setCollection(const ImportSelection &selection, bool recursive)
{
    collection_ = selection;
    collectionRecursive_ = recursive;
}

QStringList ComparisonWorkspace::collectionFiles() const
{
    if (!collection_.isValid())
        return {};
    if (collection_.kind != ImportSelection::Folder)
        return collection_.files;
    QStringList out;  // names only: nothing is opened here
    QDirIterator it(collection_.folder, QDir::Files | QDir::Readable, collectionRecursive_ ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
    while (it.hasNext() && out.size() < kMaxCollectionFiles) {
        const QString f = it.next();
        if (ImportSelection::isSupportedExtension(f))
            out << QDir::toNativeSeparators(f);
    }
    out.sort(Qt::CaseInsensitive);
    return out;
}
