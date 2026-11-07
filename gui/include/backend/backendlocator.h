#pragma once
// Finds the Python interpreter and engine directory (dev layout; see docs/ARCHITECTURE.md).
#include <QProcessEnvironment>
#include <QString>

struct BackendLocation
{
    QString python;   // interpreter (or, later, the packaged engine executable)
    QString workDir;  // directory containing scripts/engine
    QString error;    // non-empty when discovery failed

    bool ok() const { return error.isEmpty(); }
};

// Order: IMGAN_BACKEND_DIR / IMGAN_PYTHON (set by run.bat), else search upwards from appDir for
// scripts/engine and use <dir>/.venv, else `python` on PATH. Never installs or downloads anything.
BackendLocation locateBackend(const QProcessEnvironment &env, const QString &appDir);
