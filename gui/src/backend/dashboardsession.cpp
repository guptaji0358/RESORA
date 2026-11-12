#include "backend/dashboardstate.h"

namespace dashboard {

Session::~Session() { close(); }

void Session::release(resora::OpenedDocument *d)
{
    if (d->cacheDir.isEmpty())
        return;
    const QString dir = d->cacheDir;
    if (!resora::closeDocument(d) && !pending_.contains(dir))
        pending_ << dir;
}

bool Session::close()
{
    if (has_)
        release(&doc_);
    doc_ = resora::OpenedDocument();
    current_ = Restored();
    has_ = false;
    return pending_.isEmpty();
}

bool Session::retryCleanup()
{
    QStringList still;
    const QStringList todo = pending_;
    for (const QString &dir : todo) {
        resora::OpenedDocument stub;
        stub.cacheDir = dir;
        if (!resora::closeDocument(&stub))
            still << dir;
    }
    pending_ = still;
    return pending_.isEmpty();
}

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
