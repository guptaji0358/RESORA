#pragma once
// Incremental parser for the backend's NDJSON stdout (protocol v1, docs/BACKEND_CONTRACT.md).
#include <QByteArray>
#include <QJsonValue>
#include <QObject>
#include <QString>

class ProtocolParser : public QObject
{
    Q_OBJECT
public:
    static constexpr int kProtocolVersion = 1;
    static constexpr int kMaxLineBytes = 4 * 1024 * 1024;

    explicit ProtocolParser(QObject *parent = nullptr);

    // Append a stdout chunk; emits a signal per complete line, keeps the partial tail.
    void feed(const QByteArray &chunk);
    // Call at end of stream: a non-empty unterminated tail is parsed as a last line, or reported.
    void finish();

    bool hasFinal() const { return final_; }
    bool hasViolation() const { return failed_; }

signals:
    // total is -1 when the backend reports it as unknown (null).
    void progress(qint64 done, qint64 total, const QString &message);
    void result(const QJsonValue &data);
    void errorEnvelope(const QString &code, const QString &message);
    void unknownType(const QString &type);
    // Not valid protocol (bad JSON, wrong version, envelope after the final one...). Parsing stops.
    void violation(const QString &description);

private:
    void handleLine(const QByteArray &line);
    void fail(const QString &description);

    QByteArray buffer_;
    bool final_ = false;
    bool failed_ = false;
    bool atEof_ = false;
};
