#pragma once
// .resora <-> Analytics dashboard adapter (Phase 5). Qt Core only (no widgets). Converts the dashboard's
// real state to the inputs of the Phase 2/3 export engines and rebuilds a state from an opened document.
// Mapping and rules: docs/RESORA_FORMAT.md ("Dashboard Integration (Phase 5)").
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include "resora/linkedexport.h"
#include "resora/resoraimport.h"

namespace dashboard {

// The serializable state of the Analytics dashboard: exactly what AnalyticsPanel::showAnalytics()
// receives plus its view switch. Charts and image links are derived from it.
struct State
{
    QJsonObject result;  // scan/analytics result: source info + "analytics" (engine object, unmodified)
    int viewIndex = 0;   // 0 = Dashboard, 1 = Detailed View
    // Roles whose image association the user removed (Open flow, "Remove from Dashboard"). The analytics
    // JSON is untouched; an export omits these associations and records them in layout.omitted_roles.
    QStringList removedRoles;
    // Restored dashboards only: role -> validated runtime image file (extracted copy / validated or relinked
    // original). When `restored` is true an export takes image files ONLY from here; a remaining role without
    // an entry is an error (no fallback to the persisted analytics paths, which may be stale).
    bool restored = false;
    QHash<QString, QString> runtimePaths;
    // Equality is the LOGICAL dashboard state; `restored`/`runtimePaths` are runtime transport and are excluded.
    bool operator==(const State &o) const
    {
        return viewIndex == o.viewIndex && result == o.result && removedRoles == o.removedRoles;
    }
};

// Dashboard element -> image link. Roles: largest, smallest, highest, lowest.
struct ElementImage
{
    QString role, imageId, name, path;  // path: extracted copy (Bundled) or validated original (Linked); empty before export
};

struct Restored
{
    State state;
    QJsonArray charts;                // validated chart definitions (equal to chartsFor(state))
    QList<ElementImage> elementImages;  // only the associations that remain (removed roles are absent)
    QStringList removedRoles;           // associations omitted by the user / recorded as removed in the file
    QStringList warnings;             // unknown fields found and NOT restored (never silent)
    QString documentId;
    bool linked = false;
};

struct Plan  // everything the export engines need, built from a State
{
    bool ok = false;
    QString error;  // all problems, one per line
    QJsonObject source, layout, config, analytics;
    QJsonArray charts;
    QList<resora::ExportImage> images;
};

// Validates `s` and builds export inputs. Never invents data; invalid/unsupported states fail with reasons.
Plan plan(const State &s);
QJsonArray chartsFor(const QJsonObject &analyticsData);

resora::ExportResult exportBundled(const State &s, const QString &dest, const QString &appVersion,
                                   const resora::ExportProgressFn &progress = {});
resora::LinkedExportResult exportLinked(const State &s, const QString &dest, const QString &appVersion,
                                        resora::RelativeBase base = resora::RelativeBase::None,
                                        const QString &baseDir = {}, const resora::ExportProgressFn &progress = {});

// Rebuilds a dashboard from an opened document. On false, `*error` says why and `*out` is untouched.
bool restore(const resora::OpenedDocument &doc, Restored *out, QString *error);

// Runtime image associations (role -> resolved file) of a restored dashboard. Kept apart from
// Restored::state.result, whose serialized analytics JSON (and its original paths) stays unchanged.
QHash<QString, QString> runtimePaths(const Restored &r);

// Transactional holder: the active dashboard is replaced only after open + restore both succeeded.
class Session
{
public:
    struct Outcome
    {
        bool ok = false, cancelled = false;
        QString message;
        resora::OpenResult open;  // problems/links for LinksBlocked
    };
    ~Session();
    const Restored *current() const { return has_ ? &current_ : nullptr; }
    Outcome open(const QString &path, const resora::OpenOptions &opts = {}, const resora::ImportProgressFn &progress = {});
    // Drops the active dashboard and removes its cache folder. Returns false when the folder could not be
    // removed (in use, read-only, or not provably ours -> kept untouched); it is then remembered in
    // pendingCleanup() instead of being forgotten, and retryCleanup() tries again.
    bool close();
    bool retryCleanup();                                     // true when nothing is pending afterwards
    QStringList pendingCleanup() const { return pending_; }  // cache folders still on disk

private:
    void release(resora::OpenedDocument *d);  // closeDocument + record a failure
    QStringList pending_;
    Restored current_;
    resora::OpenedDocument doc_;
    bool has_ = false;
};

}  // namespace dashboard
