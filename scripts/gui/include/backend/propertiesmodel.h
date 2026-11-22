#pragma once
// Turns one entry of the backend `properties` result into labelled display rows. Qt Core only (no
// widgets), so it is unit-testable. Metadata the image does not carry is shown as "Not available",
// never as 0 (the engine reports null for absent values).
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

struct PropertyRow
{
    QString label;
    QString value;
    bool available = true;  // false: value is the "Not available" placeholder (rendered muted)
};

struct PropertySection
{
    QString title;
    QList<PropertyRow> rows;
};

struct ImageProperties
{
    QString name;
    QString path;
    QString status;  // "ok", or corrupted / unsupported / unreadable
    QString error;   // reason when status != "ok"
    QStringList warnings;
    QList<PropertySection> sections;  // empty unless status == "ok"

    bool ok() const { return status == QLatin1String("ok"); }
    QString toPlainText() const;  // for copy-to-clipboard
};

namespace PropertiesModel {

QString unavailableText();
QString formatBytes(qint64 bytes);                 // 1536 -> "1.50 KB  (1,536 bytes)"
QString formatDimensions(int width, int height);   // "1,920 × 1,080 px"
ImageProperties fromJson(const QJsonObject &image);
QList<ImageProperties> fromResult(const QJsonObject &resultData);

}  // namespace PropertiesModel
