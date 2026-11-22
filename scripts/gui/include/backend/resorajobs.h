#pragma once
// Background jobs for the .resora export/open dialogs (Phase 6). Each job runs the blocking Phase 2-5 API
// on one QThreadPool worker and reports back on the GUI thread. Lifetime rule: the destructor cancels and
// WAITS for the worker, so a worker never outlives the job and no callback reaches a destroyed object.
#include <QMutex>
#include <QObject>
#include <QWaitCondition>
#include <atomic>
#include <memory>

#include "backend/cachecleanupmanager.h"
#include "backend/resoraworkflow.h"

namespace resoraui {

class JobBase : public QObject
{
    Q_OBJECT
public:
    ~JobBase() override { shutdown(); }
    bool isRunning() const { return running_; }
    bool cancelRequested() const { return cancel_.load(); }
    void cancel() { cancel_ = true; }
    // Cancel and wait for the worker. Derived classes call it first in their destructor (their members are
    // used by the worker).
    void shutdown();

protected:
    explicit JobBase(QObject *parent = nullptr) : QObject(parent) {}
    // Runs `work` on the pool; `deliver` (GUI thread) is called once afterwards. False if already running.
    bool launch(std::function<void()> work, std::function<void()> deliver);
    // Worker -> GUI hand-over of a progress notification.
    void post(std::function<void()> fn);

    std::atomic<bool> cancel_{false};

private:
    bool running_ = false;  // GUI thread only
    QMutex mutex_;
    QWaitCondition idle_;
    bool workerBusy_ = false;
};

struct ExportOutcome
{
    bool ok = false, cancelled = false;
    QString message;
    QString path;
    int images = 0;
    quint64 fileBytes = 0;
};

class ExportJob : public JobBase
{
    Q_OBJECT
public:
    explicit ExportJob(QObject *parent = nullptr) : JobBase(parent) {}
    ~ExportJob() override { shutdown(); }
    // `state` is copied; false when a job is already running (duplicate requests are refused).
    bool start(const dashboard::State &state, Mode mode, const QString &dest, const QString &appVersion);
signals:
    void progress(int permille, const QString &stage);
    void finished(const resoraui::ExportOutcome &outcome);

private:
    ExportOutcome result_;
};

struct ImportOutcome
{
    bool ok = false, cancelled = false;
    QString message;
    resora::OpenError error = resora::OpenError::None;
    QList<resora::LinkStatus> problems;  // LinksBlocked: EVERY failing image
    int linkCount = 0;
    // On success: a fresh session that already owns the document and its cache folder. It replaces the
    // active one only when the caller decides (the old session stays untouched until then).
    std::shared_ptr<dashboard::Session> session;
};

class ImportJob : public JobBase
{
    Q_OBJECT
public:
    explicit ImportJob(QObject *parent = nullptr) : JobBase(parent) {}
    ~ImportJob() override { shutdown(); }
    // `cleanup` is the application's shared owner; failed cache removals on the worker are registered there.
    bool start(const QString &path, const resora::OpenOptions &opts,
               std::shared_ptr<dashboard::CacheCleanupManager> cleanup = {});
signals:
    void progress(int permille, const QString &stage);
    void finished(const resoraui::ImportOutcome &outcome);

private:
    ImportOutcome result_;
};

}  // namespace resoraui
