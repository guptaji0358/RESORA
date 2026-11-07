#include "backend/protocolparser.h"

#include <QJsonDocument>
#include <QJsonObject>

ProtocolParser::ProtocolParser(QObject *parent) : QObject(parent) {}

void ProtocolParser::feed(const QByteArray &chunk)
{
    if (failed_)
        return;
    buffer_.append(chunk);
    int start = 0;
    int nl;
    while (!failed_ && (nl = buffer_.indexOf('\n', start)) >= 0) {
        handleLine(buffer_.mid(start, nl - start));
        start = nl + 1;
    }
    buffer_.remove(0, start);
    if (!failed_ && buffer_.size() > kMaxLineBytes)
        fail(QStringLiteral("backend output line exceeds %1 bytes").arg(kMaxLineBytes));
}

void ProtocolParser::finish()
{
    if (failed_)
        return;
    const QByteArray tail = buffer_.trimmed();
    buffer_.clear();
    if (tail.isEmpty())
        return;
    atEof_ = true;
    handleLine(tail);
}

void ProtocolParser::fail(const QString &description)
{
    const bool first = !failed_;
    failed_ = true;
    buffer_.clear();
    if (first)
        emit violation(description);
}

void ProtocolParser::handleLine(const QByteArray &rawLine)
{
    const QByteArray line = rawLine.trimmed();
    if (line.isEmpty())
        return;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        fail(QStringLiteral("%1: %2")
                 .arg(atEof_ ? QStringLiteral("incomplete final line from backend")
                             : QStringLiteral("malformed JSON on backend stdout"),
                      QString::fromUtf8(line.left(120))));
        return;
    }
    const QJsonObject obj = doc.object();

    const QJsonValue version = obj.value(QStringLiteral("v"));
    if (!version.isDouble() || version.toInt(-1) != kProtocolVersion) {
        fail(QStringLiteral("unsupported protocol version (expected %1)").arg(kProtocolVersion));
        return;
    }
    if (final_) {
        fail(QStringLiteral("backend sent an envelope after the final one"));
        return;
    }

    const QString type = obj.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("progress")) {
        const QJsonValue done = obj.value(QStringLiteral("done"));
        const QJsonValue total = obj.value(QStringLiteral("total"));
        if (!done.isDouble() || !(total.isDouble() || total.isNull() || total.isUndefined())) {
            fail(QStringLiteral("malformed progress envelope"));
            return;
        }
        emit progress(static_cast<qint64>(done.toDouble()),
                      total.isDouble() ? static_cast<qint64>(total.toDouble()) : -1,
                      obj.value(QStringLiteral("message")).toString());
    } else if (type == QLatin1String("result")) {
        final_ = true;
        emit result(obj.value(QStringLiteral("data")));
    } else if (type == QLatin1String("error")) {
        final_ = true;
        emit errorEnvelope(obj.value(QStringLiteral("code")).toString(),
                           obj.value(QStringLiteral("message")).toString());
    } else {
        emit unknownType(type);
    }
}
