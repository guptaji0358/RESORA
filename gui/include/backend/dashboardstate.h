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
    bool operator==(const State &o) const { return viewIndex == o.viewIndex && result == o.result; }
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
    QList<ElementImage> elementImages;
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
    void close();  // drops the active dashboard and removes its cache folder

private:
    Restored current_;
    resora::OpenedDocument doc_;
    bool has_ = false;
};

}  // namespace dashboard
