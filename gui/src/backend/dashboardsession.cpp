#include "backend/dashboardstate.h"

namespace dashboard {

Session::Session(std::shared_ptr<CacheCleanupManager> cleanup)
    : cleanup_(cleanup ? std::move(cleanup) : std::make_shared<CacheCleanupManager>())
{
}

Session::~Session() { close(); }

bool Session::release(resora::OpenedDocument *d)
{
    if (resora::closeDocument(d))
        return true;
    cleanup_->add(d->cacheDir);  // closeDocument kept cacheDir on failure, so the folder is still identifiable
    return false;
}

bool Session::close()
{
    const bool removed = !has_ || release(&doc_);
    doc_ = resora::OpenedDocument();
    current_ = Restored();
    has_ = false;
    return removed;
}

bool Session::retryCleanup() { return cleanup_->retryAll() == 0; }

QStringList Session::pendingCleanup() const { return cleanup_->pending(); }

// Transactional: open + restore build a NEW state on the side. The active dashboard (and its cache
// folder) is replaced only when both succeeded; any failure or cancel leaves it untouched and removes
// the cache folder created by the failed attempt.
Session::Outcome Session::open(const QString &path, const resora::OpenOptions &opts,
                               const resora::ImportProgressFn &progress)
{
    Outcome o;
    o.open = resora::openResora(path, opts, progress);
    if (!o.open.ok()) {
        o.cancelled = o.open.error == resora::OpenError::Cancelled;
        o.message = o.open.message;
        return o;
    }
    Restored fresh;
    QString why;
    if (!restore(o.open.document, &fresh, &why)) {
        release(&o.open.document);
        o.message = why;
        return o;
    }
    close();
    doc_ = o.open.document;
    current_ = fresh;
    has_ = true;
    o.ok = true;
    return o;
}

}  // namespace dashboard
