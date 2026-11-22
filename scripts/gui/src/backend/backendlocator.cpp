#include "backend/backendlocator.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
bool isBackendDir(const QString &dir)
{
    return QFileInfo::exists(QDir(dir).filePath(QStringLiteral("scripts/engine/__main__.py")));
}

QString findBackendDir(const QProcessEnvironment &env, const QString &appDir, QString *error)
{
    const QString fromEnv = env.value(QStringLiteral("IMGAN_BACKEND_DIR"));
    if (!fromEnv.isEmpty()) {
        if (!isBackendDir(fromEnv))
            *error = QStringLiteral("IMGAN_BACKEND_DIR does not contain scripts/engine: %1").arg(fromEnv);
        return fromEnv;
    }
    QDir dir(appDir);
    for (int i = 0; i < 6; ++i) {
        if (isBackendDir(dir.absolutePath()))
            return dir.absolutePath();
        if (!dir.cdUp())
            break;
    }
    *error = QStringLiteral("Backend not found: scripts/engine is not above %1 and IMGAN_BACKEND_DIR "
                            "is not set. Start the app with run.bat.").arg(appDir);
    return {};
}

QString findPython(const QProcessEnvironment &env, const QString &workDir, QString *error)
{
    const QString fromEnv = env.value(QStringLiteral("IMGAN_PYTHON"));
    if (!fromEnv.isEmpty()) {
        if (!QFileInfo(fromEnv).isFile())
            *error = QStringLiteral("IMGAN_PYTHON does not exist: %1").arg(fromEnv);
        return fromEnv;
    }
    for (const char *rel : {".venv/Scripts/python.exe", ".venv/bin/python"}) {
        const QString candidate = QDir(workDir).filePath(QString::fromLatin1(rel));
        if (QFileInfo(candidate).isFile())
            return candidate;
    }
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("python"));
    if (!onPath.isEmpty())
        return onPath;
    *error = QStringLiteral("Python not found. Run run.bat once to create .venv, or set IMGAN_PYTHON.");
    return {};
}
} // namespace

BackendLocation locateBackend(const QProcessEnvironment &env, const QString &appDir)
{
    BackendLocation loc;
    loc.workDir = findBackendDir(env, appDir, &loc.error);
    if (!loc.ok())
        return loc;
    loc.python = findPython(env, loc.workDir, &loc.error);
    return loc;
}
