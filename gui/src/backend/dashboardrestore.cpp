#include "backend/dashboardstate.h"

#include <QFileInfo>
#include <QSet>

#include "backend/analyticsmodel.h"

namespace dashboard {
namespace {

const char *const kRoleNames[] = {"largest", "smallest", "highest", "lowest"};

const AnalyticsImage &imageOf(const AnalyticsData &d, const QString &role)
{
    if (role == "largest") return d.largest;
    if (role == "smallest") return d.smallest;
    if (role == "highest") return d.highest;
    return d.lowest;
}

void unknownKeys(const QJsonObject &o, const QSet<QString> &known, const QString &where, QStringList *warn)
{
    for (auto it = o.begin(); it != o.end(); ++it)
        if (!known.contains(it.key()))
            warn->append(QStringLiteral("%1.%2 is not understood and was not restored").arg(where, it.key()));
}

}  // namespace

bool restore(const resora::OpenedDocument &doc, Restored *out, QString *error)
{
    const auto fail = [&](const QString &m) {
        *error = m;
        return false;
    };
    Restored r;
    r.documentId = doc.documentId;
    r.linked = doc.linked;

    // analytics
    if (const QString why = resora::schemaVersionProblem(doc.analytics.value("schema_version"), 1, "analytics resource"); !why.isEmpty())
        return fail(why);
    if (!doc.analytics.value("data").isObject())
        return fail("analytics resource is missing its data");
    const QJsonObject data = doc.analytics.value("data").toObject();
    const AnalyticsData d = AnalyticsModel::fromJson(data);
    if (!d.valid)
        return fail("analytics resource does not contain a usable analytics object");

    // layout
    const QJsonObject &lay = doc.layout;
    if (const QString why = resora::schemaVersionProblem(lay.value("schema_version"), 1, "layout"); !why.isEmpty())
        return fail(why);
    const QString view = lay.value("active_view").toString();
    if (view == "dashboard") r.state.viewIndex = 0;
    else if (view == "detailed") r.state.viewIndex = 1;
    else return fail(QStringLiteral("layout has an unsupported active_view '%1'").arg(view));
    unknownKeys(lay, {"schema_version", "active_view", "visible_sections", "widgets", "omitted_roles"}, "layout", &r.warnings);
    if (!lay.value("visible_sections").isNull() && !lay.value("visible_sections").isUndefined())
        r.warnings << "layout.visible_sections is not supported by this dashboard and was not restored";
    if (!lay.value("widgets").isArray())
        return fail("layout.widgets is missing");

    QHash<QString, const resora::ImageRef *> byId;
    for (const resora::ImageRef &im : doc.images)
        byId.insert(im.imageId, &im);
    QSet<QString> widgetIds, roles;
    const QSet<QString> omittedImages(doc.omittedImageIds.begin(), doc.omittedImageIds.end());
    QSet<QString> removed;  // roles whose association is absent by explicit, recorded removal
    if (lay.contains("omitted_roles")) {  // optional; when present it must be an array (null is rejected)
        if (!lay.value("omitted_roles").isArray())
            return fail("layout.omitted_roles is not an array");
        for (const QJsonValue &v : lay.value("omitted_roles").toArray()) {
            bool known = false;
            for (const char *n : kRoleNames)
                known = known || (v.isString() && v.toString() == n);
            if (!known)
                return fail(QStringLiteral("layout.omitted_roles lists an unsupported role '%1'").arg(v.toString()));
            if (removed.contains(v.toString()))
                return fail(QStringLiteral("layout.omitted_roles lists role '%1' twice").arg(v.toString()));
            removed.insert(v.toString());
        }
    }
    for (const QJsonValue &v : lay.value("widgets").toArray()) {
        const QJsonObject w = v.toObject();
        const QString id = w.value("id").toString(), role = w.value("role").toString();
        const QString imageId = w.value("image_id").toString();
        if (id.isEmpty() || widgetIds.contains(id))
            return fail("layout has a widget without id or with a duplicate id");
        widgetIds.insert(id);
        if (roles.contains(role))
            return fail(QStringLiteral("layout lists role '%1' twice").arg(role));
        bool known = false;
        for (const char *n : kRoleNames)
            known = known || role == n;
        if (!known || id != QStringLiteral("extreme.") + role)
            return fail(QStringLiteral("layout widget '%1' has an unsupported role '%2'").arg(id, role));
        unknownKeys(w, {"id", "role", "image_id", "row", "col", "span"}, "layout.widgets", &r.warnings);
        if (removed.contains(role))
            return fail(QStringLiteral("layout lists role '%1' both as a widget and as omitted").arg(role));
        if (omittedImages.contains(imageId)) {  // the user removed this image for this open: no association, no path
            roles.insert(role);
            removed.insert(role);
            continue;
        }
        const auto it = byId.constFind(imageId);
        if (it == byId.constEnd())
            return fail(QStringLiteral("layout widget '%1' refers to unknown image '%2'").arg(id, imageId));
        const AnalyticsImage &expected = imageOf(d, role);
        if (!expected.valid || expected.name != (*it)->name)
            return fail(QStringLiteral("layout widget '%1' does not match the analytics image for '%2'").arg(id, role));
        // The runtime association must point at a readable, validated file (extracted copy / linked original).
        if ((*it)->path.isEmpty() || !QFileInfo((*it)->path).isFile())
            return fail(QStringLiteral("image '%1' for '%2' was not resolved to a readable file").arg(imageId, role));
        roles.insert(role);
        r.elementImages << ElementImage{role, imageId, (*it)->name, (*it)->path};
    }
    for (const char *n : kRoleNames)
        if (imageOf(d, n).valid && !roles.contains(n) && !removed.contains(n))
            return fail(QStringLiteral("layout has no image association for '%1'").arg(n));
    for (const char *n : kRoleNames)
        if (removed.contains(n))
            r.removedRoles << QString::fromLatin1(n);
    r.state.removedRoles = r.removedRoles;
    r.state.restored = true;
    for (const ElementImage &e : r.elementImages)
        r.state.runtimePaths.insert(e.role, e.path);

    // charts: stored definitions must be structurally valid and agree with the analytics data
    QJsonArray charts = doc.charts;
    QSet<QString> chartIds;
    for (const QJsonValue &v : charts) {
        const QJsonObject c = v.toObject();
        const QString id = c.value("id").toString(), kind = c.value("kind").toString();
        if (id.isEmpty() || chartIds.contains(id))
            return fail("charts contain a missing or duplicate id");
        chartIds.insert(id);
        if (kind != "bar" && kind != "donut" && kind != "hbar")
            return fail(QStringLiteral("chart '%1' has an unsupported kind '%2'").arg(id, kind));
        const int n = c.value("categories").toArray().size();
        for (const QJsonValue &s : c.value("series").toArray())
            if (s.toObject().value("values").toArray().size() != n)
                return fail(QStringLiteral("chart '%1' has a series whose length differs from its categories").arg(id));
    }
    if (charts != chartsFor(data))
        return fail("charts do not match the analytics data");
    r.charts = charts;

    // config -> result
    const QJsonObject &cfg = doc.config;
    if (const QString why = resora::schemaVersionProblem(cfg.value("schema_version"), 1, "config"); !why.isEmpty())
        return fail(why);
    if (!cfg.value("result_context").isObject())
        return fail("config is missing result_context");
    unknownKeys(cfg, {"schema_version", "recursive", "report_options", "scan_filters", "result_context"}, "config", &r.warnings);
    for (const char *k : {"report_options", "scan_filters"})
        if (cfg.value(k).isObject())
            r.warnings << QStringLiteral("config.%1 is not supported by this dashboard and was not restored").arg(k);
    r.state.result = cfg.value("result_context").toObject();
    r.state.result.insert("analytics", data);

    *out = r;
    return true;
}

QHash<QString, QString> runtimePaths(const Restored &r)
{
    QHash<QString, QString> h;
    for (const ElementImage &e : r.elementImages)
        h.insert(e.role, e.path);
    return h;
}

}  // namespace dashboard
