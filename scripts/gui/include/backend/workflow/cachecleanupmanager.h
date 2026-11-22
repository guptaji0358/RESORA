#pragma once
// Application-lifetime owner of cache folders whose removal failed (Phase 7). Qt Core only, no UI.
// Owned by MainWindow as a std::shared_ptr and shared with every dashboard::Session, so a pending folder
// survives Session replacement/destruction and is retried later. Thread-safe: a Session may be destroyed on
// an import worker thread. Holds path strings only; it never deletes anything on its own: every removal goes
// through resora::closeDocument (same ownership rules as a normal close, re-validated on every attempt).
#include <QMutex>
#include <QStringList>

namespace dashboard {

class CacheCleanupManager
{
public:
    // Registers a cache folder that could not be removed. Duplicates (same cleaned path) are ignored.
    void add(const QString &cacheDir);

    // One pass over the pending folders; never waits. Each folder is revalidated by the close rules: a folder
    // that is not provably ours, belongs to another RESORA process, or still fails stays pending untouched.
    // Folders that are gone or were removed leave the list. Returns how many remain pending.
    int retryAll();

    QStringList pending() const;

private:
    mutable QMutex mutex_;
    QStringList pending_;
};

}  // namespace dashboard
