#include "backend/resorajobs.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QMutexLocker>
#include <QThreadPool>

namespace resoraui {

void JobBase::shutdown()
{
    cancel_ = true;
    QMutexLocker lock(&mutex_);
    while (workerBusy_)
        idle_.wait(&mutex_);  // the worker only posts to this (still alive) object, then goes idle
}

bool JobBase::launch(std::function<void()> work, std::function<void()> deliver)
{
    if (running_)
        return false;
    running_ = true;
    cancel_ = false;
    {
        QMutexLocker lock(&mutex_);
        workerBusy_ = true;
    }
    QThreadPool::globalInstance()->start([this, work = std::move(work), deliver = std::move(deliver)]() mutable {
        work();
        post([this, deliver = std::move(deliver)] {
            running_ = false;  // GUI thread: a new job may start from inside the finished handler
            deliver();
        });
        QMutexLocker lock(&mutex_);
        workerBusy_ = false;
        idle_.wakeAll();
    });
    return true;
}

void JobBase::post(std::function<void()> fn)
{
    QMetaObject::invokeMethod(this, std::move(fn), Qt::QueuedConnection);
}

// ---- export ---------------------------------------------------------------------------------------

bool ExportJob::start(const dashboard::State &state, Mode mode, const QString &dest, const QString &appVersion)
{
    return launch(
        [this, state, mode, dest, appVersion] {
            int last = -1;
            QString lastStage;
            const resora::ExportProgressFn cb = [this, &last, &lastStage](const resora::ExportProgress &p) {
                const QString stage = stageText(p);
                if (p.permille != last || stage != lastStage) {
                    last = p.permille;
                    lastStage = stage;
                    post([this, p = p.permille, stage] { emit progress(p, stage); });
                }
                return !cancel_.load();
            };
            ExportOutcome o;
            o.path = dest;
            resora::ExportResult r;
            if (mode == Mode::Bundled)
                r = dashboard::exportBundled(state, dest, appVersion, cb);
            else
                r = dashboard::exportLinked(state, dest, appVersion, resora::RelativeBase::None, QString(), cb);
            o.ok = r.ok();
            o.cancelled = r.error == resora::ExportError::Cancelled;
            o.message = o.ok ? QString() : (o.cancelled ? tr("Export cancelled. No file was written.") : describe(r));
            o.images = r.imageCount;
            o.fileBytes = r.fileBytes;
            result_ = o;
        },
        [this] { emit finished(result_); });
}

// ---- import ---------------------------------------------------------------------------------------

bool ImportJob::start(const QString &path, const resora::OpenOptions &opts,
                      std::shared_ptr<dashboard::CacheCleanupManager> cleanup)
{
    return launch(
        [this, path, opts, cleanup] {
            int last = -1;
            QString lastStage;
            const resora::ImportProgressFn cb = [this, &last, &lastStage](const resora::ImportProgress &p) {
                const QString stage = stageText(p);
                if (p.permille != last || stage != lastStage) {
                    last = p.permille;
                    lastStage = stage;
                    post([this, p = p.permille, stage] { emit progress(p, stage); });
                }
                return !cancel_.load();
            };
            ImportOutcome o;
            auto session = std::make_shared<dashboard::Session>(cleanup);
            const dashboard::Session::Outcome r = session->open(path, opts, cb);
            o.ok = r.ok;
            o.cancelled = r.cancelled;
            o.message = r.message;
            o.error = r.open.error;
            o.problems = r.open.problems;
            o.linkCount = int(r.open.links.size());
            if (r.ok)
                o.session = session;  // else: the failed session is empty and its cache already removed
            if (r.cancelled)
                o.message = tr("Opening cancelled. The current dashboard was not changed.");
            result_ = o;
        },
        [this] { emit finished(result_); });
}

}  // namespace resoraui
