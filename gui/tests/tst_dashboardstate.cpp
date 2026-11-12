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

    // ---- Remove from Dashboard (Phase 6 correction) -------------------------------------------------
    static QByteArray slurp(const QString &f)
    {
        QFile x(f);
        return x.open(QIODevice::ReadOnly) ? x.readAll() : QByteArray();
    }
    resora::OpenResult blockedLinked(QSet<QString> *ids = nullptr)
    {
        const State s = capture(0);
        if (!exportLinked(s, p("l.resora"), "t").ok())
            return {};
        QDir(p("pics")).removeRecursively();
        const resora::OpenResult b = resora::openResora(p("l.resora"), resora::OpenOptions{p("cache")});
        if (ids)
            for (const auto &x : b.problems)
                ids->insert(x.imageId);
        return b;
    }

    void omittedImagesAreScopedAndNeverSilent()
    {
        const resora::OpenResult b = blockedLinked();
        QCOMPARE(b.error, resora::OpenError::LinksBlocked);
        QCOMPARE(b.problems.size(), 2);  // every missing image reported together
        for (const auto &x : b.problems)
            QVERIFY(!x.roles.isEmpty());
        const QByteArray before = slurp(p("l.resora"));
        resora::OpenOptions o{p("cache")};
        o.omitImages.insert(b.problems.at(0).imageId);  // only one omitted: the other still blocks
        const resora::OpenResult one = resora::openResora(p("l.resora"), o);
        QCOMPARE(one.error, resora::OpenError::LinksBlocked);
        QCOMPARE(one.problems.size(), 1);
        QCOMPARE(one.problems.at(0).imageId, b.problems.at(1).imageId);
        o.omitImages.insert("img-nope");  // unknown id is an error, never ignored
        QCOMPARE(resora::openResora(p("l.resora"), o).error, resora::OpenError::InvalidInput);
        QCOMPARE(slurp(p("l.resora")), before);
        // the default path is unchanged
        QCOMPARE(resora::openResora(p("l.resora"), resora::OpenOptions{p("cache")}).error, resora::OpenError::LinksBlocked);
    }

    void removeOneAssociationKeepsTheRest()
    {
        const resora::OpenResult b = blockedLinked();
        QString bigId, sId;
        for (const auto &x : b.problems)
            (x.name == "big.png" ? bigId : sId) = x.imageId;
        const QByteArray src = slurp(p("l.resora"));
        QVERIFY(put(p("moved/s.jpg"), QByteArray(900, 'b')));
        resora::OpenOptions o{p("cache")};
        o.omitImages.insert(bigId);
        o.relinks.insert(sId, p("moved/s.jpg"));
        const resora::OpenResult r = resora::openResora(p("l.resora"), o);
        QVERIFY2(r.ok(), qPrintable(r.message));
        QCOMPARE(r.document.images.size(), 1);
        QCOMPARE(r.document.omittedImageIds, QStringList{bigId});
        Restored got;
        QString err;
        QVERIFY2(restore(r.document, &got, &err), qPrintable(err));
        QCOMPARE(got.removedRoles, (QStringList{"largest", "highest"}));
        QCOMPARE(got.elementImages.size(), 2);
        const QHash<QString, QString> paths = runtimePaths(got);
        QVERIFY(!paths.contains("largest") && !paths.contains("highest"));
        QCOMPARE(paths.value("smallest"), p("moved/s.jpg"));
        QCOMPARE(paths.value("lowest"), p("moved/s.jpg"));
        for (const auto &e : got.elementImages)  // no stale path of the removed image anywhere
            QVERIFY(!e.path.contains("big.png"));
        QCOMPARE(got.state.result, result());  // analytics JSON untouched
        QCOMPARE(slurp(p("l.resora")), src);   // source file untouched
        AnalyticsPanel panel;
        QVERIFY2(panel.showRestored(got.state.result, paths, &err, got.removedRoles), qPrintable(err));
        QCOMPARE(panel.removedRoles(), got.removedRoles);
        QCOMPARE(panel.result(), result());
        resora::OpenedDocument d = r.document;
        resora::closeDocument(&d);
    }

    void removeAllAndExportAgainKeepsRemoval()
    {
        QSet<QString> ids;
        blockedLinked(&ids);
        resora::OpenOptions o{p("cache")};
        o.omitImages = ids;
        const resora::OpenResult r = resora::openResora(p("l.resora"), o);
        QVERIFY2(r.ok(), qPrintable(r.message));
        QCOMPARE(r.document.images.size(), 0);
        Restored got;
        QString err;
        QVERIFY2(restore(r.document, &got, &err), qPrintable(err));
        QCOMPARE(got.removedRoles.size(), 4);
        QVERIFY(got.elementImages.isEmpty());
        const QByteArray src = slurp(p("l.resora"));
        QVERIFY2(exportLinked(got.state, p("n.resora"), "t").ok(), "re-export");
        QCOMPARE(slurp(p("l.resora")), src);
        Restored again;
        QVERIFY2(openRestore(p("n.resora"), &again, &err), qPrintable(err));
        QCOMPARE(again.removedRoles, got.removedRoles);
        QVERIFY(again.elementImages.isEmpty());
        QCOMPARE(again.state.result, result());
        QVERIFY(again.charts == got.charts);
        // partial removal also survives a Bundled export
        makeImages();
        State partial = capture(0);
        partial.removedRoles = {"largest", "highest"};
        QVERIFY2(exportBundled(partial, p("pb.resora"), "t").ok(), "bundled partial");
        Restored pb;
        QVERIFY2(openRestore(p("pb.resora"), &pb, &err), qPrintable(err));
        QCOMPARE(pb.removedRoles, (QStringList{"largest", "highest"}));
        QCOMPARE(pb.elementImages.size(), 2);
        QVERIFY(QFileInfo(p("pics/big.png")).isFile());  // original image untouched
        partial.removedRoles = {"bogus"};
        QVERIFY(!plan(partial).ok);
    }

    // ---- Re-export from runtime paths (Phase 6 final review) ----------------------------------------
    void bundledReExportWorksAfterSourceDirectoryDeleted()
    {
        const State s = capture(1);
        QVERIFY(exportBundled(s, p("b.resora"), "t").ok());
        const QByteArray original = slurp(p("b.resora"));
        QVERIFY(QDir(p("pics")).removeRecursively());
        resora::OpenResult o = resora::openResora(p("b.resora"), resora::OpenOptions{p("cache")});
        QVERIFY2(o.ok(), qPrintable(o.message));
        Restored got;
        QString err;
        QVERIFY2(restore(o.document, &got, &err), qPrintable(err));
        QVERIFY(got.state.restored);
        // the state the real panel hands to the export dialog carries the same runtime paths
        AnalyticsPanel panel;
        QVERIFY2(panel.showRestored(got.state.result, runtimePaths(got), &err, got.removedRoles), qPrintable(err));
        QVERIFY(panel.isRestored());
        panel.setViewIndex(got.state.viewIndex);
        const State fromPanel{panel.result(), panel.viewIndex(), panel.removedRoles(), panel.isRestored(), panel.runtimePaths()};
        QVERIFY(fromPanel == got.state);
        QVERIFY(fromPanel.restored && fromPanel.runtimePaths == got.state.runtimePaths);
        QCOMPARE(fromPanel.runtimePaths.size(), 4);
        QVERIFY(!QFileInfo(p("pics")).exists());  // the original directory stays deleted
        QVERIFY(exportBundled(fromPanel, p("b2.resora"), "t").ok());
        for (const resora::ExportImage &im : plan(fromPanel).images)
            QVERIFY(im.sourcePath.startsWith(o.document.cacheDir));  // extracted copies, never the stale paths
        QCOMPARE(slurp(p("b.resora")), original);   // original document untouched
        QCOMPARE(got.state.result, result());       // persisted analytics JSON untouched
        resora::closeDocument(&o.document);
        // the new document opens (cache of the first one is gone) and its images are intact
        resora::OpenResult o2 = resora::openResora(p("b2.resora"), resora::OpenOptions{p("cache2")});
        QVERIFY2(o2.ok(), qPrintable(o2.message));
        Restored again;
        QVERIFY2(restore(o2.document, &again, &err), qPrintable(err));
        QCOMPARE(again.elementImages.size(), 4);
        for (const ElementImage &e : again.elementImages)
            QCOMPARE(slurp(e.path), e.name == "big.png" ? QByteArray(5000, 'a') : QByteArray(900, 'b'));
        QCOMPARE(again.state.result, result());
        resora::closeDocument(&o2.document);
    }

    void restoredExportNeverFallsBackToStalePaths()
    {
        makeImages();  // the persisted paths exist and WOULD work for a non-restored export
        State s = capture(0);
        QVERIFY(plan(s).ok);
        s.restored = true;  // restored but no runtime path: must fail rather than use the persisted ones
        const Plan bad = plan(s);
        QVERIFY(!bad.ok);
        QVERIFY(bad.error.contains("runtime path"));
        QVERIFY(!exportBundled(s, p("x.resora"), "t").ok());
        QVERIFY(!QFileInfo(p("x.resora")).exists());
        s.runtimePaths = {{"largest", p("pics/big.png")}, {"highest", p("pics/big.png")}, {"smallest", p("pics/s.jpg")}};
        QVERIFY(!plan(s).ok);  // one role still missing
        s.runtimePaths.insert("lowest", p("pics/s.jpg"));
        QVERIFY(plan(s).ok);
    }

    void linkedReExportAfterRelinkKeepsRemovals()
    {
        QSet<QString> ids;
        const resora::OpenResult b = blockedLinked(&ids);  // originals deleted
        QCOMPARE(b.error, resora::OpenError::LinksBlocked);
        QString bigId, sId;
        for (const auto &x : b.problems)
            (x.name == "big.png" ? bigId : sId) = x.imageId;
        const QByteArray src = slurp(p("l.resora"));
        QVERIFY(put(p("moved/s.jpg"), QByteArray(900, 'b')));
        resora::OpenOptions opts{p("cache")};
        opts.omitImages.insert(bigId);
        opts.relinks.insert(sId, p("moved/s.jpg"));
        const resora::OpenResult r = resora::openResora(p("l.resora"), opts);
        QVERIFY2(r.ok(), qPrintable(r.message));
        Restored got;
        QString err;
        QVERIFY2(restore(r.document, &got, &err), qPrintable(err));
        const resora::LinkedExportResult ex = exportLinked(got.state, p("l2.resora"), "t");
        QVERIFY2(ex.ok(), qPrintable(ex.message));
        const Plan pl = plan(got.state);
        QCOMPARE(pl.images.size(), 1);
        QCOMPARE(pl.images.at(0).sourcePath, p("moved/s.jpg"));  // the relinked path, not pics/s.jpg
        QCOMPARE(slurp(p("l.resora")), src);
        QCOMPARE(got.state.result, result());
        resora::OpenedDocument d = r.document;
        resora::closeDocument(&d);
        Restored again;
        QVERIFY2(openRestore(p("l2.resora"), &again, &err), qPrintable(err));
        QCOMPARE(again.removedRoles, (QStringList{"largest", "highest"}));  // removed associations stay removed
        QCOMPARE(again.elementImages.size(), 2);
        for (const ElementImage &e : again.elementImages)
            QCOMPARE(e.path, p("moved/s.jpg"));
    }

    void omittedRolesFieldIsValidated()
    {
        QVERIFY(exportBundled(capture(0), p("b.resora"), "t").ok());
        resora::OpenResult o = resora::openResora(p("b.resora"), resora::OpenOptions{p("cache")});
        QVERIFY(o.ok());
        QVERIFY(!o.document.layout.contains("omitted_roles"));  // absent when nothing was removed
        Restored got;
        QString err;
        const auto tryRoles = [&](const QJsonValue &v, bool dropWidget) {
            resora::OpenedDocument d = o.document;
            if (dropWidget) {
                QJsonArray w;
                for (const QJsonValue &x : d.layout.value("widgets").toArray())
                    if (x.toObject().value("role").toString() != "largest")
                        w << x;
                d.layout["widgets"] = w;
            }
            d.layout["schema_version"] = 2;  // omitted_roles is a version 2 field
            d.layout["omitted_roles"] = v;
            return restore(d, &got, &err);
        };
        QVERIFY(!tryRoles("largest", false) && err.contains("not an array"));
        QVERIFY(!tryRoles(QJsonValue(QJsonValue::Null), false) && err.contains("not an array"));
        QVERIFY(!tryRoles(QJsonArray{"nope"}, true) && err.contains("unsupported role"));
        QVERIFY(!tryRoles(QJsonArray{1}, true) && err.contains("unsupported role"));
        QVERIFY(!tryRoles(QJsonArray{"largest", "largest"}, true) && err.contains("twice"));
        QVERIFY(!tryRoles(QJsonArray{"largest"}, false) && err.contains("both as a widget and as omitted"));
        QVERIFY2(tryRoles(QJsonArray{"largest"}, true), qPrintable(err));  // valid: removed and no widget
        QCOMPARE(got.removedRoles, QStringList{"largest"});
        resora::closeDocument(&o.document);
    }

    // Regenerates the committed compatibility fixtures in gui/tests/fixtures/dashboard. Skipped unless
    // RESORA_WRITE_FIXTURES names an existing folder (manifest timestamps differ per run; the tests never compare bytes).
    void writeCompatibilityFixtures()
    {
        const QString dir = QString::fromLocal8Bit(qgetenv("RESORA_WRITE_FIXTURES"));
        if (dir.isEmpty())
            QSKIP("set RESORA_WRITE_FIXTURES=<folder> to regenerate the compatibility fixtures");
        QVERIFY(exportBundled(capture(0), dir + "/v1_no_omitted_roles.resora", "fixture-1").ok());
        State removed = capture(0);
        removed.removedRoles = {"largest"};
        QVERIFY(exportBundled(removed, dir + "/v2_omitted_largest.resora", "fixture-2").ok());
    }

    void compatibilityFixturesRestore()  // real versioned files on disk (v1 = Phase 5 shape, v2 = omitted_roles)
    {
        const QString dir = QFileInfo(QStringLiteral(__FILE__)).absolutePath() + "/fixtures/dashboard";
        const auto open = [&](const QString &name, Restored *got) {
            const resora::OpenResult o = resora::openResora(dir + "/" + name, resora::OpenOptions{p("fx_cache")});
            QVERIFY2(o.ok(), qPrintable(o.message));
            QString err;
            const bool ok = restore(o.document, got, &err);
            resora::OpenedDocument d = o.document;
            resora::closeDocument(&d);
            QVERIFY2(ok, qPrintable(err));
        };
        Restored v1, v2;
        open("v1_no_omitted_roles.resora", &v1);
        QVERIFY(v1.warnings.isEmpty());
        QVERIFY(v1.removedRoles.isEmpty());
        QCOMPARE(v1.elementImages.size(), 4);
        open("v2_omitted_largest.resora", &v2);
        QVERIFY(v2.warnings.isEmpty());
        QCOMPARE(v2.removedRoles, QStringList{"largest"});
        QCOMPARE(v2.elementImages.size(), 3);
        // association data survives: every remaining role keeps the same image file name as in v1
        // (imageIds are positional and shift when an image is removed, so they are not compared)
        for (const ElementImage &e : v2.elementImages) {
            bool same = false;
            for (const ElementImage &f : v1.elementImages)
                same = same || (f.role == e.role && f.name == e.name);
            QVERIFY2(same, qPrintable(e.role));
        }
    }

    void omittedRolesNeedVersionTwo()  // version 1 must not carry omitted_roles; version 2 without it is fine
    {
        const QString dir = QFileInfo(QStringLiteral(__FILE__)).absolutePath() + "/fixtures/dashboard";
        resora::OpenResult o = resora::openResora(dir + "/v1_no_omitted_roles.resora", resora::OpenOptions{p("fx_cache")});
        QVERIFY(o.ok());
        Restored got;
        QString err;
        resora::OpenedDocument d = o.document;
        d.layout["omitted_roles"] = QJsonArray{"largest"};
        QVERIFY(!restore(d, &got, &err) && err.contains("requires layout schema_version 2"));
        d.layout["schema_version"] = 2;
        QVERIFY(!restore(d, &got, &err) && err.contains("both as a widget and as omitted"));  // version 2 accepted, rule still enforced
        d = o.document;
        d.layout["schema_version"] = 2;
        QVERIFY2(restore(d, &got, &err), qPrintable(err));
        QVERIFY(got.removedRoles.isEmpty());
        resora::closeDocument(&o.document);
    }

    void reexportPreservesVersionAndAssociations()  // no migration: versions are kept exactly as written
    {
        const QString dir = QFileInfo(QStringLiteral(__FILE__)).absolutePath() + "/fixtures/dashboard";
        for (const char *name : {"v1_no_omitted_roles.resora", "v2_omitted_largest.resora"}) {
            const resora::OpenResult o = resora::openResora(dir + "/" + name, resora::OpenOptions{p("fx_cache")});
            QVERIFY(o.ok());
            // the fixture's on-disk versions, asserted explicitly before any re-export
            const int fixtureLayout = std::string(name) == "v1_no_omitted_roles.resora" ? 1 : 2;
            QCOMPARE(o.document.layout.value("schema_version").toInt(), fixtureLayout);
            QCOMPARE(o.document.analytics.value("schema_version").toInt(), 1);
            QCOMPARE(o.document.config.value("schema_version").toInt(), 1);
            QCOMPARE(o.document.manifest.value("schema_version").toInt(), 1);
            Restored got;
            QString err;
            QVERIFY2(restore(o.document, &got, &err), qPrintable(err));
            const QString out = p(QStringLiteral("re_") + name);
            QVERIFY2(exportBundled(got.state, out, "re").ok(), name);
            const resora::OpenResult again = resora::openResora(out, resora::OpenOptions{p("fx_cache")});
            QVERIFY(again.ok());
            const int expectVersion = std::string(name) == "v1_no_omitted_roles.resora" ? 1 : 2;
            QCOMPARE(again.document.layout.value("schema_version").toInt(), expectVersion);
            Restored back;
            QVERIFY2(restore(again.document, &back, &err), qPrintable(err));
            QCOMPARE(back.removedRoles, got.removedRoles);
            QCOMPARE(back.elementImages.size(), got.elementImages.size());
            // every remaining role keeps its image identity (role -> image filename), not only the count
            const auto identities = [](const Restored &r) {
                QStringList ids;
                for (const ElementImage &e : r.elementImages)
                    ids << e.role + QLatin1Char('|') + e.name;
                ids.sort();
                return ids;
            };
            QCOMPARE(identities(back), identities(got));
            QVERIFY(!identities(got).isEmpty());
            resora::OpenedDocument d1 = o.document, d2 = again.document;
            resora::closeDocument(&d1);
            resora::closeDocument(&d2);
        }
    }

    void versionAndUnknownFieldPolicy()  // Phase 7 compatibility policy
    {
        QVERIFY(exportBundled(capture(0), p("v.resora"), "t").ok());
        resora::OpenResult o = resora::openResora(p("v.resora"), resora::OpenOptions{p("cache")});
        QVERIFY(o.ok());
        Restored got;
        QString err;
        QVERIFY2(restore(o.document, &got, &err), qPrintable(err));
        QVERIFY(got.warnings.isEmpty());  // a current file restores with no warnings and no omitted_roles
        const auto with = [&](const char *res, const QJsonValue &v) {
            resora::OpenedDocument d = o.document;
            QJsonObject &obj = std::string(res) == "layout" ? d.layout : std::string(res) == "config" ? d.config : d.analytics;
            obj["schema_version"] = v;
            return restore(d, &got, &err);
        };
        for (const char *res : {"layout", "config", "analytics"}) {
            const int newer = std::string(res) == "layout" ? 3 : 2;  // layout accepts 1 and 2 (Phase 7 bump)
            QVERIFY(!with(res, newer) && err.contains("newer RESORA") && err.contains(QStringLiteral("schema_version %1").arg(newer)));
            QVERIFY(!with(res, 0) && err.contains("no valid schema_version"));
            QVERIFY(!with(res, "1") && err.contains("no valid schema_version"));
            QVERIFY(!with(res, 1.5) && err.contains("no valid schema_version"));
            QVERIFY(with(res, 1));
        }
        // unknown optional fields are never silent: restored with a warning that names them
        resora::OpenedDocument d = o.document;
        d.layout["future_feature"] = 1;
        d.config["future_option"] = true;
        QVERIFY2(restore(d, &got, &err), qPrintable(err));
        QVERIFY(got.warnings.join('|').contains("layout.future_feature"));
        QVERIFY(got.warnings.join('|').contains("config.future_option"));
        resora::closeDocument(&o.document);
    }

    void omitRejectedForBundled()
    {
        QVERIFY(exportBundled(capture(0), p("b.resora"), "t").ok());
        resora::OpenOptions o{p("cache")};
        o.omitImages.insert("img-0000");
        QCOMPARE(resora::openResora(p("b.resora"), o).error, resora::OpenError::InvalidInput);
    }
};

QTEST_MAIN(TstDashboardState)
#include "tst_dashboardstate.moc"
