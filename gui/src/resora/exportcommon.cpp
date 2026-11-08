#include "resora/exportcommon.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

namespace resora::detail {

namespace {
constexpr qint64 kChunk = 1 << 20;
}

QString num(int index) { return QStringLiteral("%1").arg(index + 1, 4, 10, QLatin1Char('0')); }

QByteArray toJson(const QJsonObject &o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }

QJsonObject withSchema(QJsonObject o)
{
    if (!o.contains(QStringLiteral("schema_version")))
        o.insert(QStringLiteral("schema_version"), 1);
    return o;
}

bool hashFile(const QString &path, Source *out, QString *why, const std::function<bool(quint64)> &keepGoing)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *why = f.errorString();
        return false;
    }
    QCryptographicHash h(QCryptographicHash::Sha256);
    QByteArray buf(int(kChunk), '\0');
    quint64 total = 0;
    for (;;) {
        const qint64 n = f.read(buf.data(), kChunk);
        if (n < 0) {
            *why = f.errorString();
            return false;
        }
        if (n == 0)
            break;
        h.addData(QByteArrayView(buf.constData(), n));
        total += quint64(n);
        if (!keepGoing(quint64(n))) {
            *why = QString();
            return false;
        }
    }
    out->sha = h.result();
    out->size = total;
    return true;
}

bool sameSource(const QString &path, const Source &s)
{
    const QFileInfo fi(path);
    return fi.isFile() && quint64(fi.size()) == s.size && fi.lastModified().toMSecsSinceEpoch() == s.mtimeMs;
}

bool addJsonResource(Writer &w, QJsonArray &inventory, const QString &id, const QJsonObject &o)
{
    inventory.append(QJsonObject{{"id", id}, {"type", "json"}});
    return w.addBytes(id, TypeJson, toJson(o)).ok();
}

bool addCommonResources(Writer &w, QJsonArray &inventory, const QJsonObject &layout, const QJsonArray &charts,
                        const QJsonObject &analytics, int engineProtocol, const QJsonObject &config)
{
    const QJsonObject chartsDoc{{"schema_version", 1}, {"charts", charts}};
    const QJsonObject analyticsDoc{{"schema_version", 1},
                                   {"data", analytics},
                                   {"engine_protocol", engineProtocol < 0 ? QJsonValue(QJsonValue::Null) : QJsonValue(engineProtocol)}};
    return addJsonResource(w, inventory, QStringLiteral("layout"), withSchema(layout))
        && addJsonResource(w, inventory, QStringLiteral("charts"), chartsDoc)
        && addJsonResource(w, inventory, QStringLiteral("analytics"), analyticsDoc)
        && addJsonResource(w, inventory, QStringLiteral("config"), withSchema(config));
}

}  // namespace resora::detail
