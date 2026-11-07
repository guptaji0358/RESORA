#include "backend/backendprocess.h"

#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>

namespace {
constexpr int kStderrKeep = 64 * 1024;
}

BackendProcess::BackendProcess(QObject *parent) : QObject(parent), parser_(this)
{
    killTimer_.setSingleShot(true);
    stallTimer_.setSingleShot(true);

    connect(&killTimer_, &QTimer::timeout, this, [this] {
        if (process_ && process_->state() != QProcess::NotRunning)
            process_->kill();  // terminate() cannot stop a console-less Windows process
    });
    connect(&stallTimer_, &QTimer::timeout, this, [this] {
        timedOut_ = true;
        requestStop();
    });

    connect(&parser_, &ProtocolParser::progress, this, &BackendProcess::progress);
    connect(&parser_, &ProtocolParser::result, this, [this](const QJsonValue &data) {
        gotResult_ = true;
        resultData_ = data;
    });
    connect(&parser_, &ProtocolParser::errorEnvelope, this, [this](const QString &c, const QString &m) {
        errorCode_ = c;
        errorMessage_ = m;
    });
    connect(&parser_, &ProtocolParser::violation, this, [this](const QString &what) {
        violation_ = what;
        if (process_ && process_->state() != QProcess::NotRunning)
            process_->kill();  // a backend that breaks the protocol cannot be trusted further
    });
}

bool BackendProcess::isRunning() const
{
    return started_ && !done_;
}

void BackendProcess::start(const BackendLocation &location, const QStringList &commandArguments)
{
    if (started_)
        return;
    started_ = true;

    process_ = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    process_->setProcessEnvironment(env);
    process_->setWorkingDirectory(location.workDir);
    process_->setProgram(location.python);
    process_->setArguments(QStringList{QStringLiteral("-m"), QStringLiteral("scripts.engine"),
                                       QStringLiteral("--cancel-on-stdin")}
                           + commandArguments);

    connect(process_, &QProcess::readyReadStandardOutput, this, &BackendProcess::onReadyStdout);
    connect(process_, &QProcess::readyReadStandardError, this, [this] {
        stderr_.append(process_->readAllStandardError());  // diagnostics only, never parsed
        if (stderr_.size() > kStderrKeep)
            stderr_.remove(0, stderr_.size() - kStderrKeep);
    });
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            fail(QStringLiteral("spawn_failed"),
                 tr("Could not start the backend (%1): %2")
                     .arg(QDir::toNativeSeparators(process_->program()), process_->errorString()),
                 -1);
    });
    connect(process_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st) {
        onFinished(code, st);
    });

    process_->start();
    stallTimer_.start(stallMs_);
}

void BackendProcess::onReadyStdout()
{
    stallTimer_.start(stallMs_);
    parser_.feed(process_->readAllStandardOutput());
}

void BackendProcess::cancel()
{
    if (isRunning())
        requestStop();
}

void BackendProcess::requestStop()
{
    if (done_ || stopRequested_ || !process_)
        return;
    stopRequested_ = true;
    stallTimer_.stop();
    if (process_->state() == QProcess::Running) {
        process_->write("cancel\n");
        process_->closeWriteChannel();  // stdin EOF also means cancel
    }
    killTimer_.start(graceMs_);
}

QString BackendProcess::stderrTail() const
{
    return QString::fromUtf8(stderr_).trimmed();
}

void BackendProcess::fail(const QString &code, const QString &message, int exitCode)
{
    if (done_)
        return;
    done_ = true;
    killTimer_.stop();
    stallTimer_.stop();
    emit failed(code, message, exitCode, stderrTail());
}

void BackendProcess::finishCancelled()
{
    if (done_)
        return;
    done_ = true;
    killTimer_.stop();
    stallTimer_.stop();
    emit cancelled();
}

void BackendProcess::onFinished(int exitCode, int exitStatus)
{
    if (done_)
        return;
    const QByteArray rest = process_->readAllStandardOutput();
    if (!rest.isEmpty())
        parser_.feed(rest);
    stderr_.append(process_->readAllStandardError());
    parser_.finish();

    const bool crashed = exitStatus == QProcess::CrashExit;
    if (timedOut_)
        return fail(QStringLiteral("timeout"),
                    tr("The backend produced no output for %1 s and was stopped.").arg(stallMs_ / 1000),
                    exitCode);
    // A scan that completed before the cancel arrived is still a valid, finished scan.
    if (gotResult_ && !crashed && exitCode == 0 && violation_.isEmpty()) {
        done_ = true;
        killTimer_.stop();
        stallTimer_.stop();
        emit succeeded(resultData_);
        return;
    }
    if (stopRequested_ || errorCode_ == QLatin1String("cancelled"))
        return finishCancelled();
    if (!violation_.isEmpty())
        return fail(QStringLiteral("protocol_error"), violation_, exitCode);
    if (!errorCode_.isEmpty())
        return fail(errorCode_, errorMessage_, exitCode);
    if (crashed)
        return fail(QStringLiteral("crashed"), tr("The backend process crashed."), exitCode);
    fail(QStringLiteral("crashed"),
         tr("The backend exited (code %1) without reporting a result.").arg(exitCode), exitCode);
}
