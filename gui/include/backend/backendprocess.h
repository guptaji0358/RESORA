#pragma once
// One backend run (contract "BackendJob"): QProcess + NDJSON parser + cancel/stall handling.
// Single use. Exactly one of succeeded / failed / cancelled is emitted. Never blocks.
#include <QByteArray>
#include <QJsonValue>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include "backend/backendlocator.h"
#include "backend/protocolparser.h"

class QProcess;

class BackendProcess : public QObject
{
    Q_OBJECT
public:
    explicit BackendProcess(QObject *parent = nullptr);

    void setGracePeriodMs(int ms) { graceMs_ = ms; }   // cancel -> kill (contract: 3 s)
    void setStallTimeoutMs(int ms) { stallMs_ = ms; }  // no stdout for this long -> timeout (60 s)

    // `commandArguments` follow `-m scripts.engine --cancel-on-stdin`, e.g. {"scan", folder, ...}.
    void start(const BackendLocation &location, const QStringList &commandArguments);
    // Cooperative cancel: "cancel\n" on stdin, then kill() after the grace period. Idempotent.
    void cancel();
    bool isRunning() const;

signals:
    void progress(qint64 done, qint64 total, const QString &message);  // total -1 = unknown
    void succeeded(const QJsonValue &data);
    // code: engine error code, or spawn_failed / crashed / timeout / protocol_error.
    void failed(const QString &code, const QString &message, int exitCode, const QString &stderrTail);
    void cancelled();

private:
    void onReadyStdout();
    void onFinished(int exitCode, int exitStatus);
    void requestStop();
    void fail(const QString &code, const QString &message, int exitCode);
    void finishCancelled();
    QString stderrTail() const;

    QProcess *process_ = nullptr;
    ProtocolParser parser_;
    QTimer killTimer_;
    QTimer stallTimer_;
    int graceMs_ = 3000;
    int stallMs_ = 60000;

    bool started_ = false;
    bool done_ = false;
    bool stopRequested_ = false;
    bool timedOut_ = false;
    bool gotResult_ = false;
    QJsonValue resultData_;
    QString errorCode_, errorMessage_, violation_;
    QByteArray stderr_;
};
