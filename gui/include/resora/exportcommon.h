#pragma once
// Internal helpers shared by Bundled and Linked export (Qt Core only). Not part of the public API.
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <functional>

#include "resora/resorawriter.h"

namespace resora::detail {

#ifdef Q_OS_WIN
constexpr Qt::CaseSensitivity kPathCase = Qt::CaseInsensitive;
#else
constexpr Qt::CaseSensitivity kPathCase = Qt::CaseSensitive;
#endif

// Identity of a source file taken during validation.
struct Source
{
    QByteArray sha;  // SHA-256 (raw)
    quint64 size = 0;
    qint64 mtimeMs = 0;
};

QString num(int index);                       // 0-based index -> "0001"
QByteArray toJson(const QJsonObject &o);      // compact UTF-8 JSON
QJsonObject withSchema(QJsonObject o);        // adds schema_version 1 when missing

// Streams the file once (read-only, 1 MiB chunks): size + SHA-256. `keepGoing(bytes)` is polled per
// chunk; returning false aborts with an empty `why`.
bool hashFile(const QString &path, Source *out, QString *why, const std::function<bool(quint64)> &keepGoing);

// True when the path is still a regular file with the recorded size and mtime.
bool sameSource(const QString &path, const Source &s);

// Adds a JSON resource and records it in the manifest inventory. False => writer refused the id.
bool addJsonResource(Writer &w, QJsonArray &inventory, const QString &id, const QJsonObject &o);

// The mode-independent dashboard resources: layout, charts, analytics, config. Both export modes call
// this so the same input yields byte-identical common resources.
bool addCommonResources(Writer &w, QJsonArray &inventory, const QJsonObject &layout, const QJsonArray &charts,
                        const QJsonObject &analytics, int engineProtocol, const QJsonObject &config);

}  // namespace resora::detail
