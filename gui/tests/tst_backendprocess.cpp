// Drives the real Python engine (found like the app does: env vars, else upward search + .venv).
#include <QCoreApplication>
#include <QImage>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend/backendlocator.h"
#include "backend/backendprocess.h"
#include "backend/scanrequest.h"

class TstBackendProcess : public QObject
{
    Q_OBJECT
    BackendLocation loc_;

private slots:
    void initTestCase()
    {
        loc_ = locateBackend(QProcessEnvironment::systemEnvironment(), QCoreApplication::applicationDirPath());
        if (!loc_.ok())
            QSKIP(qPrintable(loc_.error));
    }

    void scanSucceedsWithProgressAndReports()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QImage(64, 36, QImage::Format_RGB32).save(dir.filePath("a.png")));
        ScanRequest req;
        req.folder = dir.path();
        req.csvPath = dir.filePath("out/r.csv");
        req.txtPath = dir.filePath("out/r.txt");  // missing parent: backend creates it

        BackendProcess job;
        QSignalSpy ok(&job, &BackendProcess::succeeded);
        QSignalSpy bad(&job, &BackendProcess::failed);
        QSignalSpy prog(&job, &BackendProcess::progress);
        job.start(loc_, req.commandArguments());
        QVERIFY(ok.wait(30000));
        QCOMPARE(bad.count(), 0);
        QVERIFY(prog.count() >= 1);
        const QJsonObject data = ok[0][0].toJsonValue().toObject();
        QCOMPARE(data["summary"].toObject()["total"].toInt(), 1);
        QCOMPARE(data["summary"].toObject()["analyzed"].toInt(), 1);
        QVERIFY(QFileInfo::exists(data["reports"].toObject()["csv"].toString()));
        QVERIFY(QFileInfo::exists(data["reports"].toObject()["txt"].toString()));
        QVERIFY(!job.isRunning());
    }

    void scanSelectedFilesThroughParamsFile()
    {
        QTemporaryDir dir;
        QVERIFY(QImage(64, 36, QImage::Format_RGB32).save(dir.filePath("a.png")));
        QVERIFY(QImage(32, 32, QImage::Format_RGB32).save(dir.filePath("b.png")));
        QFile bad(dir.filePath("c.png"));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("not an image");
        bad.close();
        ScanRequest req;
        req.files = {dir.filePath("a.png"), dir.filePath("b.png"), dir.filePath("c.png"), dir.filePath("gone.png")};
        req.paramsFile = dir.filePath("params.json");
        QFile pf(req.paramsFile);
        QVERIFY(pf.open(QIODevice::WriteOnly));
        pf.write(req.paramsJson());
        pf.close();
        req.csvPath = dir.filePath("r.csv");

        BackendProcess job;
        QSignalSpy ok(&job, &BackendProcess::succeeded);
        job.start(loc_, req.commandArguments());
        QVERIFY(ok.wait(30000));
        const QJsonObject data = ok[0][0].toJsonValue().toObject();
        QCOMPARE(data["source"].toString(), QString("files"));
        QCOMPARE(data["summary"].toObject()["total"].toInt(), 4);
        QCOMPARE(data["summary"].toObject()["analyzed"].toInt(), 2);
        QVERIFY(QFileInfo::exists(data["reports"].toObject()["csv"].toString()));
    }

    void scanWithoutReportsHasNullReportPaths()
    {
        QTemporaryDir dir;
        ScanRequest req;
        req.folder = dir.path();
        BackendProcess job;
        QSignalSpy ok(&job, &BackendProcess::succeeded);
        job.start(loc_, req.commandArguments());
        QVERIFY(ok.wait(30000));
        const QJsonObject reports = ok[0][0].toJsonValue().toObject()["reports"].toObject();
        QVERIFY(reports["csv"].isNull());
        QVERIFY(reports["txt"].isNull());
    }

    void backendErrorEnvelopeBecomesFailure()
    {
        ScanRequest req;
        req.folder = QDir::tempPath() + "/definitely-missing-folder-imgan";
        BackendProcess job;
        QSignalSpy bad(&job, &BackendProcess::failed);
        job.start(loc_, req.commandArguments());
        QVERIFY(bad.wait(30000));
        QCOMPARE(bad[0][0].toString(), QString("invalid_input"));
        QCOMPARE(bad[0][2].toInt(), 3);
    }

    void startupFailureReported()
    {
        BackendLocation broken = loc_;
        broken.python = loc_.workDir + "/no-such-python.exe";
        BackendProcess job;
        QSignalSpy bad(&job, &BackendProcess::failed);
        job.start(broken, {"selfcheck"});
        QVERIFY(bad.count() == 1 || bad.wait(10000));  // may be emitted synchronously by start()
        QCOMPARE(bad[0][0].toString(), QString("spawn_failed"));
    }

    void cancelIsReportedAsCancelled()
    {
        BackendProcess job;
        QSignalSpy canc(&job, &BackendProcess::cancelled);
        QSignalSpy bad(&job, &BackendProcess::failed);
        QSignalSpy prog(&job, &BackendProcess::progress);
        job.start(loc_, {"_contract-test", "progress", "--steps", "200", "--delay", "0.1"});
        QVERIFY(prog.wait(30000));
        job.cancel();
        QVERIFY(canc.wait(15000));
        QCOMPARE(bad.count(), 0);
        QVERIFY(!job.isRunning());
    }

    void cancelEscalatesToKill()
    {
        BackendProcess job;
        job.setGracePeriodMs(1);  // force the kill path
        QSignalSpy canc(&job, &BackendProcess::cancelled);
        QSignalSpy prog(&job, &BackendProcess::progress);
        job.start(loc_, {"_contract-test", "progress", "--steps", "200", "--delay", "0.1"});
        QVERIFY(prog.wait(30000));
        job.cancel();
        QVERIFY(canc.wait(15000));
    }

    void stallTimeoutReported()
    {
        BackendProcess job;
        job.setStallTimeoutMs(300);
        job.setGracePeriodMs(200);
        QSignalSpy bad(&job, &BackendProcess::failed);
        job.start(loc_, {"_contract-test", "progress", "--steps", "3", "--delay", "30"});
        QVERIFY(bad.wait(20000));
        QCOMPARE(bad[0][0].toString(), QString("timeout"));
    }
};

QTEST_MAIN(TstBackendProcess)
#include "tst_backendprocess.moc"
