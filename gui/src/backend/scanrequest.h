#pragma once
// What the user asked for; turns into `scan` command arguments for the engine.
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

struct ScanRequest
{
    QString folder;       // folder source (empty when `files` is used)
    QStringList files;    // selected-image source; sent in the params file, never on the command line
    QString paramsFile;   // path of the JSON params file that carries `files` (written by the caller)
    bool recursive = true;  // folder source only
    QString csvPath;  // empty = no CSV report
    QString txtPath;  // empty = no TXT report

    bool usesParamsFile() const { return !files.isEmpty(); }

    // Contents of the params file: {"files": [...]} (UTF-8 JSON object, see BACKEND_CONTRACT.md).
    QByteArray paramsJson() const
    {
        return QJsonDocument(QJsonObject{{QStringLiteral("files"), QJsonArray::fromStringList(files)}})
            .toJson(QJsonDocument::Compact);
    }

    // Arguments after `-m scripts.engine --cancel-on-stdin`. Passed as a list, never a shell string.
    QStringList commandArguments() const
    {
        QStringList args;
        if (usesParamsFile()) {
            args << QStringLiteral("--params-file") << paramsFile << QStringLiteral("scan");
        } else {
            args << QStringLiteral("scan") << folder;
            if (recursive)
                args << QStringLiteral("--recursive");
        }
        if (!csvPath.isEmpty())
            args << QStringLiteral("--csv") << csvPath;
        if (!txtPath.isEmpty())
            args << QStringLiteral("--txt") << txtPath;
        return args;
    }
};
