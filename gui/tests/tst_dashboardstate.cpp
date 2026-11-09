// Dashboard <-> .resora integration tests (Phase 5). Temporary data only; real AnalyticsPanel model.
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

#include "backend/dashboardstate.h"
#include "widgets/analyticspanel.h"

using namespace dashboard;

namespace {
const char *kTemplate = R"JSON({"source":"folder","folder":"C:/pics","recursive":true,"duplicates_ignored":0,"analytics":{
 "denominator":3,
 "collection":{"total":5,"analyzed":3,"failed":2,"failed_by_status":{"corrupted":1,"invalid_metadata":1},"duplicates_ignored":0,
  "total_bytes":9000000000,"size_known":3,"size_unknown":0,"failed_bytes":10,"average_bytes":3000000000.0,
  "largest":{"path":"C:/pics/big.png","name":"big.png","width":3840,"height":2160,"megapixels":8.294,"file_size":6000000000,"format":"PNG","resolution":"UHD 4K"},
  "smallest":{"path":"C:/pics/s.jpg","name":"s.jpg","width":100,"height":100,"megapixels":0.01,"file_size":1000000000,"format":"JPEG","resolution":"Custom (below HD)"}},
 "resolution":{"total_pixels":8314400,"total_megapixels":8.314,"average_width":1980.0,"average_height":1420.0,"average_megapixels":2.771,
  "highest":{"path":"C:/pics/big.png","name":"big.png","width":3840,"height":2160,"megapixels":8.294,"file_size":6000000000,"format":"PNG","resolution":"UHD 4K","tied":1},
  "lowest":{"path":"C:/pics/s.jpg","name":"s.jpg","width":100,"height":100,"megapixels":0.01,"file_size":1000000000,"format":"JPEG","resolution":"x","tied":1},
  "most_common":{"width":3840,"height":2160,"count":1,"percent":33.33,"tied":3}},
 "categories":[{"name":"HD","count":0,"percent":0.0},{"name":"Full HD","count":1,"percent":33.33},{"name":"UHD 4K","count":1,"percent":33.33},{"name":"Below HD / Other","count":1,"percent":33.33}],
 "orientation":[{"name":"landscape","count":2,"percent":66.67},{"name":"portrait","count":0,"percent":0.0},{"name":"square","count":1,"percent":33.33}],
 "formats":[{"name":"PNG","count":2,"percent":66.67,"bytes":8000000000,"bytes_percent":88.89},{"name":"Other","count":1,"percent":33.33,"bytes":1000000000,"bytes_percent":11.11}],
 "other_formats":{"ICO":1},
 "failures":[{"path":"C:/pics/bad.jpg","status":"corrupted","error":"not a valid image"}],"failures_truncated":false}})JSON";

bool put(const QString &p, const QByteArray &d)
{
    QDir().mkpath(QFileInfo(p).absolutePath());
    QFile f(p);
    return f.open(QIODevice::WriteOnly) && f.write(d) == d.size();
}
}  // namespace

class TstDashboardState : public QObject
{
    Q_OBJECT
    QTemporaryDir dir_;
    QString root() const { return dir_.path(); }
    QString p(const QString &n) const { return dir_.filePath(n); }

    QJsonObject result() const
    {
        QByteArray j(kTemplate);
        j.replace("C:/pics", root().toUtf8() + "/pics");
        return QJsonDocument::fromJson(j).object();
    }
    void makeImages() { put(p("pics/big.png"), QByteArray(5000, 'a')); put(p("pics/s.jpg"), QByteArray(900, 'b')); }
    State capture(int view = 1)
    {
        AnalyticsPanel panel;
        panel.showAnalytics(result());
        panel.setViewIndex(view);
        return State{panel.result(), panel.viewIndex()};
    }
    bool openRestore(const QString &file, Restored *r, QString *err)
    {
        const resora::OpenResult o = resora::openResora(file, resora::OpenOptions{p("cache")});
        if (!o.ok()) { *err = o.message; return false; }
        const bool ok = restore(o.document, r, err);
        resora::OpenedDocument d = o.document;
        resora::closeDocument(&d);
        return ok;
    }

private slots:
    void init() { makeImages(); }

    void panelExposesItsState()
    {
        const State s = capture(1);
        QCOMPARE(s.viewIndex, 1);
        QCOMPARE(s.result, result());
        AnalyticsPanel empty;
        QVERIFY(empty.result().isEmpty());
    }

    void planMapsRealStateAndIsDeterministic()
    {
        const Plan a = plan(capture()), b = plan(capture());
        QVERIFY2(a.ok, qPrintable(a.error));
        QCOMPARE(a.images.size(), 2);  // largest==highest, smallest==lowest share a file
        QCOMPARE(a.layout.value("active_view").toString(), QString("detailed"));
        QCOMPARE(a.layout.value("widgets").toArray().size(), 4);
        QCOMPARE(a.charts.size(), 4);
        QCOMPARE(a.charts, b.charts);
        QCOMPARE(a.layout, b.layout);
        QCOMPARE(a.config.value("result_context").toObject().value("folder").toString(), root() + "/pics");
        QVERIFY(!a.config.value("result_context").toObject().contains("analytics"));
        QCOMPARE(a.charts[0].toObject().value("id").toString(), QString("chart.categories"));
    }

    void bundledRoundTrip()
    {
        const State s = capture(1);
        const auto r = exportBundled(s, p("b.resora"), "t");
        QVERIFY2(r.ok(), qPrintable(r.message));
        QFile::remove(p("pics/big.png"));  // independent of the original locations
        QFile::remove(p("pics/s.jpg"));
        Restored got;
        QString err;
        resora::OpenResult o = resora::openResora(p("b.resora"), resora::OpenOptions{p("cache")});
        QVERIFY2(o.ok(), qPrintable(o.message));
        QVERIFY2(restore(o.document, &got, &err), qPrintable(err));
        QVERIFY(got.state == s);
        QCOMPARE(got.charts, chartsFor(s.result.value("analytics").toObject()));
        QCOMPARE(got.elementImages.size(), 4);
        for (const ElementImage &e : got.elementImages) {
            QVERIFY(QFileInfo(e.path).isFile());
            QVERIFY(e.path.startsWith(o.document.cacheDir));
        }
        QCOMPARE(got.elementImages[0].imageId, got.elementImages[2].imageId);  // largest == highest
        QVERIFY(got.warnings.isEmpty());
        resora::closeDocument(&o.document);
    }

    void linkedRoundTrip()
    {
        const State s = capture(0);
        const auto r = exportLinked(s, p("l.resora"), "t");
        QVERIFY2(r.ok(), qPrintable(r.message));
        QVERIFY(QFileInfo(p("l.resora")).size() < 20000);
        Restored got;
        QString err;
        QVERIFY2(openRestore(p("l.resora"), &got, &err), qPrintable(err));
        QVERIFY(got.linked);
        QVERIFY(got.state == s);
        QCOMPARE(got.elementImages[1].path, p("pics/s.jpg"));
        // the same reopened state drives a second real panel to identical data
        AnalyticsPanel panel;
        panel.showAnalytics(got.state.result);
        panel.setViewIndex(got.state.viewIndex);
        QCOMPARE(panel.data().analyzed, qint64(3));
        QCOMPARE(panel.viewIndex(), 0);
        QCOMPARE(panel.result(), s.result);
    }

    // Shows the restored dashboard in a real panel using the runtime paths and returns the panel's data.
    static bool showInPanel(AnalyticsPanel &panel, const Restored &r, QString *err)
    {
        return panel.showRestored(r.state.result, runtimePaths(r), err);
    }

    void bundledRuntimePathsSurviveDeletedSourceDirectory()
    {
        const State s = capture(1);
        QVERIFY(exportBundled(s, p("b.resora"), "t").ok());
        QVERIFY(QDir(p("pics")).removeRecursively());  // the whole source directory is gone
        resora::OpenResult o = resora::openResora(p("b.resora"), resora::OpenOptions{p("cache")});
        QVERIFY2(o.ok(), qPrintable(o.message));
        Restored got;
        QString err;
        QVERIFY2(restore(o.document, &got, &err), qPrintable(err));
        AnalyticsPanel panel;
        QVERIFY2(showInPanel(panel, got, &err), qPrintable(err));
        const AnalyticsData &d = panel.data();
        for (const AnalyticsImage *im : {&d.largest, &d.smallest, &d.highest, &d.lowest}) {
            QVERIFY(im->path.startsWith(o.document.cacheDir));
            QVERIFY(QFileInfo(im->path).isFile());
            QVERIFY(!im->path.startsWith(root() + "/pics"));
        }
        QCOMPARE(d.largest.path, d.highest.path);
        QVERIFY(d.largest.path != d.smallest.path);
        QCOMPARE(d.largest.name, QString("big.png"));
        // persisted JSON unchanged: it still carries the (now stale) original paths
        QCOMPARE(panel.result(), s.result);
        QCOMPARE(panel.result().value("analytics").toObject().value("collection").toObject()
                     .value("largest").toObject().value("path").toString(), root() + "/pics/big.png");
        QVERIFY(got.state == s);
        resora::closeDocument(&o.document);
    }

    void linkedRuntimePathsAreTheValidatedLinks()
    {
        const State s = capture(0);
        QVERIFY(exportLinked(s, p("l.resora"), "t").ok());
        // files moved: the originals are gone, identical copies are relinked
        QVERIFY(put(p("moved/big.png"), QByteArray(5000, 'a')) && put(p("moved/s.jpg"), QByteArray(900, 'b')));
        QVERIFY(QDir(p("pics")).removeRecursively());
        resora::OpenOptions opts{p("cache")};
        const resora::OpenResult blocked = resora::openResora(p("l.resora"), opts);
        QCOMPARE(blocked.error, resora::OpenError::LinksBlocked);
        resora::OpenResult o = resora::openResora(p("l.resora"), opts);
        const QHash<QString, QString> rel{{blocked.problems.at(0).imageId, p("moved/" + blocked.problems.at(0).name)},
                                          {blocked.problems.at(1).imageId, p("moved/" + blocked.problems.at(1).name)}};
        o = resora::relinkAndOpen(p("l.resora"), opts, rel);
        QVERIFY2(o.ok(), qPrintable(o.message));
        Restored got;
        QString err;
        QVERIFY2(restore(o.document, &got, &err), qPrintable(err));
        QVERIFY(got.linked);
        AnalyticsPanel panel;
        QVERIFY2(showInPanel(panel, got, &err), qPrintable(err));
        const AnalyticsData &d = panel.data();
        QCOMPARE(d.largest.path, p("moved/big.png"));
        QCOMPARE(d.highest.path, p("moved/big.png"));
        QCOMPARE(d.smallest.path, p("moved/s.jpg"));
        QCOMPARE(d.lowest.path, p("moved/s.jpg"));
        QCOMPARE(panel.result(), s.result);  // serialized JSON still has the stale pics/ paths
        QVERIFY(panel.result().value("analytics").toObject().value("collection").toObject()
                    .value("largest").toObject().value("path").toString().contains("/pics/"));
    }

    void unresolvedImagesCannotProduceARestoredDashboard()
    {
        QVERIFY(exportBundled(capture(), p("b.resora"), "t").ok());
        resora::OpenResult o = resora::openResora(p("b.resora"), resora::OpenOptions{p("cache")});
        QVERIFY(o.ok());
        Restored got;
        QString err;
        // an image reference with no resolved path
        resora::OpenedDocument d = o.document;
        for (resora::ImageRef &im : d.images)
            im.path.clear();
        QVERIFY(!restore(d, &got, &err) && err.contains("not resolved"));
        // a resolved path whose file vanished
        d = o.document;
        QVERIFY(QFile::remove(d.images[0].path));
        QVERIFY(!restore(d, &got, &err) && err.contains("not resolved"));
        // the panel refuses a missing role path and keeps what it showed before; no fallback to JSON paths
        AnalyticsPanel panel;
        panel.showAnalytics(result());
        const AnalyticsData before = panel.data();
        QHash<QString, QString> paths{{"largest", "x"}, {"smallest", "y"}, {"highest", "x"}};  // lowest missing
        QVERIFY(!panel.showRestored(result(), paths, &err) && err.contains("lowest"));
        QCOMPARE(panel.data().lowest.path, before.lowest.path);
        QVERIFY(!panel.showRestored(result(), {}, &err));
        resora::closeDocument(&o.document);
    }

    void invalidStatesAreRejected()
    {
        State bad = capture();
        bad.viewIndex = 7;
        QVERIFY(!plan(bad).ok);
        QVERIFY(plan(bad).error.contains("view index"));
        QVERIFY(!plan(State{}).ok);
        QCOMPARE(exportBundled(State{}, p("x.resora"), "t").error, resora::ExportError::InvalidInput);
        State rel = capture();
        QJsonObject r = rel.result, an = r.value("analytics").toObject(), col = an.value("collection").toObject();
        QJsonObject big = col.value("largest").toObject();
        big["path"] = "relative/big.png";
        col["largest"] = big; an["collection"] = col; r["analytics"] = an; rel.result = r;
        QVERIFY(plan(rel).error.contains("absolute"));
        QVERIFY(!QFileInfo::exists(p("x.resora")));
    }

    void missingSourceImageFailsExportWithoutFile()
    {
        QFile::remove(p("pics/s.jpg"));
        const auto r = exportBundled(capture(), p("m.resora"), "t");
        QCOMPARE(r.error, resora::ExportError::SourceProblem);
        QVERIFY(!QFileInfo::exists(p("m.resora")));
    }

    void unsupportedFieldsAreReportedNotDropped()
    {
        State s = capture();
        s.result["extra_key"] = "kept";  // unknown result keys survive via result_context
        QVERIFY(exportBundled(s, p("e.resora"), "t").ok());
        Restored got;
        QString err;
        QVERIFY2(openRestore(p("e.resora"), &got, &err), qPrintable(err));
        QCOMPARE(got.state.result.value("extra_key").toString(), QString("kept"));
        // layout/config with fields this build cannot restore -> warnings
        resora::OpenResult o = resora::openResora(p("e.resora"), resora::OpenOptions{p("cache")});
        QVERIFY(o.ok());
        o.document.layout["future_grid"] = 3;
        o.document.config["scan_filters"] = QJsonObject{{"min", 1}};
        QVERIFY2(restore(o.document, &got, &err), qPrintable(err));
        QCOMPARE(got.warnings.size(), 2);
        // unsupported values are fatal
        o.document.layout["active_view"] = "grid";
        QVERIFY(!restore(o.document, &got, &err));
        QVERIFY(err.contains("active_view"));
        resora::closeDocument(&o.document);
    }

    void tamperedDocumentPartsAreRejected()
    {
        QVERIFY(exportBundled(capture(), p("t.resora"), "t").ok());
        resora::OpenResult o = resora::openResora(p("t.resora"), resora::OpenOptions{p("cache")});
        QVERIFY(o.ok());
        Restored got;
        QString err;
        resora::OpenedDocument d = o.document;
        d.charts = QJsonArray();
        QVERIFY(!restore(d, &got, &err) && err.contains("charts"));
        d = o.document;
        d.analytics = QJsonObject{{"schema_version", 1}};
        QVERIFY(!restore(d, &got, &err) && err.contains("analytics"));
        d = o.document;
        QJsonArray w = d.layout.value("widgets").toArray();
        QJsonObject w0 = w[0].toObject();
        w0["image_id"] = "img-0099";
        w[0] = w0; d.layout["widgets"] = w;
        QVERIFY(!restore(d, &got, &err) && err.contains("unknown image"));
        d = o.document;
        d.layout["widgets"] = QJsonArray();
        QVERIFY(!restore(d, &got, &err) && err.contains("association"));
        d = o.document;
        d.config = QJsonObject{{"schema_version", 1}};
        QVERIFY(!restore(d, &got, &err) && err.contains("config"));
        resora::closeDocument(&o.document);
    }

    void sessionKeepsStateOnFailureAndCancel()
    {
        const State a = capture(0);
        QVERIFY(exportBundled(a, p("a.resora"), "t").ok());
        QVERIFY(exportLinked(a, p("link.resora"), "t").ok());
        Session ses;
        QVERIFY(!ses.current());
        const resora::OpenOptions opts{p("cache")};
        QVERIFY(ses.open(p("a.resora"), opts).ok);
        QVERIFY(ses.current()->state == a);
        const QString firstCache = ses.current()->elementImages[0].path;
        QVERIFY(QFileInfo(firstCache).isFile());
        // failure: missing file
        QVERIFY(!ses.open(p("nope.resora"), opts).ok);
        QVERIFY(ses.current()->state == a);
        QVERIFY(QFileInfo(firstCache).isFile());
        // failure: linked image modified -> blocked, state unchanged
        put(p("pics/s.jpg"), QByteArray(900, 'z'));
        const Session::Outcome blocked = ses.open(p("link.resora"), opts);
        QVERIFY(!blocked.ok);
        QCOMPARE(blocked.open.error, resora::OpenError::LinksBlocked);
        QVERIFY(ses.current()->state == a && !ses.current()->linked);
        // cancellation
        const Session::Outcome c = ses.open(p("a.resora"), opts, [](const resora::ImportProgress &) { return false; });
        QVERIFY(c.cancelled && !c.ok);
        QVERIFY(ses.current()->state == a);
        QVERIFY(QFileInfo(firstCache).isFile());
        // success replaces and removes the old cache folder
        QVERIFY(ses.open(p("a.resora"), opts).ok);
        QVERIFY(!QFileInfo(firstCache).exists());
        QVERIFY(QFileInfo(ses.current()->elementImages[0].path).isFile());
        ses.close();
        QVERIFY(!ses.current());
        QVERIFY(QDir(p("cache")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    }

    void restoreFailureAfterOpenCleansCache()
    {
        // a valid container whose dashboard parts are unusable must not become the active state
        resora::BundledExportInput in;
        in.appVersion = "t";
        in.layout = QJsonObject{{"active_view", "dashboard"}, {"widgets", QJsonArray()}};
        in.analytics = QJsonObject{{"denominator", 0}};
        in.config = QJsonObject{};
        resora::ExportImage im;
        im.sourcePath = p("pics/big.png");
        im.metadata = QJsonObject{{"format", "PNG"}, {"width", 8}, {"height", 8}, {"file_size", 5000}, {"status", "ok"}};
        in.images << im;
        QVERIFY(resora::exportBundled(in, p("u.resora")).ok());
        Session ses;
        const State a = capture(0);
        QVERIFY(exportBundled(a, p("a.resora"), "t").ok());
        QVERIFY(ses.open(p("a.resora"), resora::OpenOptions{p("cache")}).ok);
        const Session::Outcome o = ses.open(p("u.resora"), resora::OpenOptions{p("cache")});
        QVERIFY(!o.ok && !o.message.isEmpty());
        QVERIFY(ses.current()->state == a);
        QCOMPARE(QDir(p("cache")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size(), 1);  // only the active one
    }
};

QTEST_MAIN(TstDashboardState)
#include "tst_dashboardstate.moc"
