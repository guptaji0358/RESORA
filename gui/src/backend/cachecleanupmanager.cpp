#include "backend/cachecleanupmanager.h"

#include <QDir>

#include "resora/resoraimport.h"

namespace dashboard {

void CacheCleanupManager::add(const QString &cacheDir)
{
    if (cacheDir.isEmpty())
        return;
    const QString dir = QDir::cleanPath(cacheDir);
    QMutexLocker lock(&mutex_);
    if (!pending_.contains(dir))
        pending_ << dir;
}

int CacheCleanupManager::retryAll()
{
    // Copy under the lock, do the filesystem work outside it (a slow disk must not block other threads
    // that register folders), then drop only the entries that were actually removed.
    const QStringList todo = pending();
    QStringList removed;
    for (const QString &dir : todo) {
        resora::OpenedDocument stub;
        stub.cacheDir = dir;
        if (resora::closeDocument(&stub))
            removed << dir;
    }
    QMutexLocker lock(&mutex_);
    for (const QString &dir : removed)
        pending_.removeAll(dir);
    return pending_.size();
}

QStringList CacheCleanupManager::pending() const
{
    QMutexLocker lock(&mutex_);
    return pending_;
}

}  // namespace dashboard
