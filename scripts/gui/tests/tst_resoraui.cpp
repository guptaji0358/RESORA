// Export/Import UI workflow tests (Phase 6). Offscreen, temporary data only; no coordinates or timing.
#include <QDir>
#include <QTableWidget>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "backend/dashboardstate.h"
#include "backend/resoraworkflow.h"
#include "widgets/analyticspanel.h"
#include "widgets/exportdialog.h"
#include "widgets/openresoradialog.h"
#include "widgets/relinkpanel.h"

using namespace dashboard;
Q_DECLARE_METATYPE(resoraui::ExportOutcome)
Q_DECLARE_METATYPE(resoraui::ImportOutcome)

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

class TstResoraUi : public QObject
{
    Q_OBJECT
    QTemporaryDir dir_;
    QString p(const QString &n) const { return dir_.filePath(n); }

    State capture()
    {
        QByteArray j(kTemplate);
        j.replace("C:/pics", dir_.path().toUtf8() + "/pics");
        AnalyticsPanel panel;
        panel.showAnalytics(QJsonDocument::fromJson(j).object());
        return State{panel.result(), panel.viewIndex()};
    }
    void exportTo(resoraui::Mode m, const QString &file)
    {
        ExportDialog dlg(capture(), "t");
        dlg.setMode(m);
        dlg.setDestination(file);
        QVERIFY(dlg.startExport());
        QTRY_VERIFY(!dlg.isRunning());
        QVERIFY2(dlg.outcome().ok, qPrintable(dlg.outcome().message));
    }

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init()
    {
        put(p("pics/big.png"), QByteArray(5000, 'a'));
        put(p("pics/s.jpg"), QByteArray(900, 'b'));
    }

    void modeSelection()
    {
        ExportDialog dlg(capture(), "t");
        QCOMPARE(dlg.mode(), resoraui::Mode::Bundled);
        dlg.setMode(resoraui::Mode::Linked);
        QCOMPARE(dlg.mode(), resoraui::Mode::Linked);
        dlg.setMode(resoraui::Mode::Bundled);
        QCOMPARE(dlg.mode(), resoraui::Mode::Bundled);
    }

    void destinationValidation()
    {
        const QStringList src{p("pics/big.png")};
        QVERIFY(!resoraui::checkDestination("", src).ok);
        QVERIFY(!resoraui::checkDestination(p("pics"), src).ok);
        QVERIFY(!resoraui::checkDestination(p("a.txt"), src).ok);
        QVERIFY(!resoraui::checkDestination(p("nodir/a.resora"), src).ok);
        QVERIFY(!resoraui::checkDestination(p("pics/big.png"), src).ok);
        const auto ok = resoraui::checkDestination(p("a"), src);
        QVERIFY(ok.ok);
        QVERIFY(ok.path.endsWith(".resora"));
        QVERIFY(!ok.exists);
        QVERIFY(resoraui::checkDestination(p("A.RESORA"), src).path.endsWith(".resora"));
    }

    void overwriteRequiresConfirmation()
    {
        put(p("x.resora"), "old");
        ExportDialog dlg(capture(), "t");
        int asked = 0;
        dlg.setOverwriteConfirmer([&](const QString &) { ++asked; return false; });
        dlg.setDestination(p("x.resora"));
        QVERIFY(!dlg.startExport());
        QCOMPARE(asked, 1);
        QFile f(p("x.resora"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("old"));  // declined: untouched
        f.close();
        dlg.setOverwriteConfirmer([&](const QString &) { ++asked; return true; });
        QVERIFY(dlg.startExport());
        QTRY_VERIFY(!dlg.isRunning());
        QVERIFY(dlg.outcome().ok);
        QCOMPARE(asked, 2);
    }

    void exportDuplicateRequestRefusedAndProgress()
    {
        ExportDialog dlg(capture(), "t");
        dlg.setDestination(p("d.resora"));
        QVERIFY(dlg.startExport());
        QVERIFY(!dlg.startExport());  // refused while running
        QTRY_VERIFY(!dlg.isRunning());
        QVERIFY(dlg.outcome().ok);
        QVERIFY(dlg.outcome().fileBytes > 0);
        QCOMPARE(dlg.outcome().images, 2);
        QVERIFY(QFileInfo(p("d.resora")).isFile());
    }

    void exportFailureKeepsDestination()
    {
        put(p("keep.resora"), "old");
        QFile::remove(p("pics/s.jpg"));
        ExportDialog dlg(capture(), "t");
        dlg.setOverwriteConfirmer([](const QString &) { return true; });
        dlg.setDestination(p("keep.resora"));
        QVERIFY(!dlg.exportButton()->isEnabled());  // missing original reported up front
        QVERIFY(!dlg.startExport());
        QFile f(p("keep.resora"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("old"));
    }

    void exportCancellation()
    {
        resoraui::ExportJob job;
        QSignalSpy done(&job, &resoraui::ExportJob::finished);
        QVERIFY(job.start(capture(), resoraui::Mode::Bundled, p("c.resora"), "t"));
        job.cancel();
        QTRY_COMPARE(done.count(), 1);
        const auto o = done.first().first().value<resoraui::ExportOutcome>();
        // Cancel may race with a very fast export: either way no partial file may exist on cancel.
        if (o.cancelled)
            QVERIFY(!QFileInfo(p("c.resora")).exists());
        else
            QVERIFY(o.ok);
        QVERIFY(!job.isRunning());
    }

    void closeWhileRunningCancelsSafely()
    {
        auto *dlg = new ExportDialog(capture(), "t");
        dlg->setDestination(p("z.resora"));
        QVERIFY(dlg->startExport());
        dlg->close();  // close -> reject(); must not destroy under the worker
        delete dlg;     // destructor waits for the worker
        QTest::qWait(50);
    }

    void importBundled()
    {
        exportTo(resoraui::Mode::Bundled, p("b.resora"));
        QFile::remove(p("pics/big.png"));
        OpenResoraDialog dlg;
        dlg.setFile(p("b.resora"));
        QVERIFY(dlg.startOpen());
        QVERIFY(!dlg.startOpen());  // duplicate refused
        QTRY_VERIFY(!dlg.isRunning());
        QCOMPARE(dlg.state(), OpenResoraDialog::State::Done);
        QVERIFY(dlg.session() && dlg.session()->current());
        const QString cached = runtimePaths(*dlg.session()->current()).values().value(0);
        QVERIFY(QFileInfo(cached).isFile());
        auto s = dlg.session();
        s->close();
        QVERIFY(!QFileInfo(cached).exists());
    }

    void importLinkedMissingReportedTogetherAndRelink()
    {
        exportTo(resoraui::Mode::Linked, p("l.resora"));
        QVERIFY(QFile::rename(p("pics/big.png"), p("moved_big.png")));
        QVERIFY(QFile::rename(p("pics/s.jpg"), p("s.jpg.bak")));
        OpenResoraDialog dlg;
        dlg.setFile(p("l.resora"));
        QVERIFY(dlg.startOpen());
        QTRY_VERIFY(!dlg.isRunning());
        QCOMPARE(dlg.state(), OpenResoraDialog::State::NeedsRelink);
        QVERIFY(!dlg.session());
        RelinkPanel *rp = dlg.relinkPanel();
        QCOMPARE(rp->rowCount(), 2);  // both listed, not just the first
        QVERIFY(!dlg.openButton()->isEnabled());
        QVERIFY(!dlg.retryWithRelinks());  // blocked until all assigned

        const QString idA = rp->model().rows()[0].imageId, idB = rp->model().rows()[1].imageId;
        rp->assign(idA, p(rp->model().rows()[0].name == "big.png" ? "moved_big.png" : "s.jpg.bak"));
        QVERIFY(!rp->complete());
        QVERIFY(!dlg.retryWithRelinks());
        // wrong content with the right name must be rejected by the engine
        put(p("fake.bin"), QByteArray(900, 'z'));
        rp->assign(idB, p("fake.bin"));
        QVERIFY(rp->complete());
        QVERIFY(dlg.retryWithRelinks());
        QTRY_VERIFY(!dlg.isRunning());
        QCOMPARE(dlg.state(), OpenResoraDialog::State::NeedsRelink);
        QVERIFY(!dlg.session());
        QCOMPARE(rp->rowCount(), 2);  // table keeps every image
        QCOMPARE(rp->model().count(resoraui::RelinkModel::Status::WrongImage), 1);
        QCOMPARE(rp->model().count(resoraui::RelinkModel::Status::Resolved), 1);

        const auto probs = rp->model().unresolved();
        for (const auto &pr : probs)
            rp->assign(pr.imageId, p(pr.name == "big.png" ? "moved_big.png" : "s.jpg.bak"));
        QVERIFY(dlg.retryWithRelinks());
        QTRY_VERIFY(!dlg.isRunning());
        QCOMPARE(dlg.state(), OpenResoraDialog::State::Done);
        QVERIFY(dlg.session() && dlg.session()->current());
    }

    void corruptFileFailsAndKeepsPreviousSession()
    {
        exportTo(resoraui::Mode::Bundled, p("good.resora"));
        OpenResoraDialog first;
        first.setFile(p("good.resora"));
        QVERIFY(first.startOpen());
        QTRY_VERIFY(!first.isRunning());
        auto old = first.session();
        QVERIFY(old && old->current());
        const QString oldFile = runtimePaths(*old->current()).values().value(0);

        put(p("bad.resora"), QByteArray(300, 'q'));
        OpenResoraDialog bad;
        bad.setFile(p("bad.resora"));
        QVERIFY(bad.startOpen());
        QTRY_VERIFY(!bad.isRunning());
        QCOMPARE(bad.state(), OpenResoraDialog::State::Failed);
        QVERIFY(!bad.session());
        QVERIFY(!bad.message().isEmpty());
        QVERIFY(old->current() && QFileInfo(oldFile).isFile());  // previous dashboard untouched

        OpenResoraDialog second;  // successful import yields a distinct session; old one released by caller
        second.setFile(p("good.resora"));
        QVERIFY(second.startOpen());
        QTRY_VERIFY(!second.isRunning());
        QVERIFY(second.session() && second.session() != old);
        old->close();
        QVERIFY(!QFileInfo(oldFile).exists());
        QVERIFY(second.session()->current());
        QVERIFY(QFileInfo(runtimePaths(*second.session()->current()).values().value(0)).isFile());
    }

    void importCancellationAndCleanup()
    {
        exportTo(resoraui::Mode::Bundled, p("cc.resora"));
        resoraui::ImportJob job;
        QSignalSpy done(&job, &resoraui::ImportJob::finished);
        QVERIFY(job.start(p("cc.resora"), resora::OpenOptions()));
        job.cancel();
        QTRY_COMPARE(done.count(), 1);
        const auto o = done.first().first().value<resoraui::ImportOutcome>();
        QVERIFY(o.cancelled || o.ok);
        if (o.cancelled)
            QVERIFY(!o.session);
        QVERIFY(!job.isRunning());
    }

    void missingFileCannotStart()
    {
        OpenResoraDialog dlg;
        dlg.setFile(p("nope.resora"));
        QVERIFY(!dlg.openButton()->isEnabled());
        QVERIFY(!dlg.startOpen());
    }

    // ---- Relinking table / Remove from Dashboard (Phase 6 correction) --------------------------------
    static QByteArray slurp(const QString &f)
    {
        QFile x(f);
        return x.open(QIODevice::ReadOnly) ? x.readAll() : QByteArray();
    }
    // Opens a Linked dashboard whose two images are both gone; leaves the dialog in NeedsRelink.
    void blockBoth(OpenResoraDialog &dlg)
    {
        for (const char *f : {"l.resora", "moved_big.png", "moved_s.jpg"})  // an existing destination would ask to overwrite
            QFile::remove(p(f));
        exportTo(resoraui::Mode::Linked, p("l.resora"));
        QVERIFY(QFile::rename(p("pics/big.png"), p("moved_big.png")));
        QVERIFY(QFile::rename(p("pics/s.jpg"), p("moved_s.jpg")));
        dlg.setFile(p("l.resora"));
        QVERIFY(dlg.startOpen());
        QTRY_VERIFY(!dlg.isRunning());
        QCOMPARE(dlg.state(), OpenResoraDialog::State::NeedsRelink);
    }
    QString idOf(RelinkPanel *rp, const QString &name)
    {
        for (const auto &r : rp->model().rows())
            if (r.name == name)
                return r.imageId;
        return {};
    }

    void tableShowsAllMissingImagesAtOnce()
    {
        OpenResoraDialog dlg;
        blockBoth(dlg);
        RelinkPanel *rp = dlg.relinkPanel();
        QCOMPARE(rp->rowCount(), 2);
        QCOMPARE(rp->table()->rowCount(), 2);
        QCOMPARE(rp->table()->columnCount(), 4);
        for (const auto &r : rp->model().rows()) {
            QCOMPARE(r.status, resoraui::RelinkModel::Status::Missing);
            QVERIFY(!r.roles.isEmpty());
        }
        const auto *big = rp->model().find(idOf(rp, "big.png"));
        QVERIFY(big && big->roles.contains("largest") && big->roles.contains("highest"));
        QCOMPARE(rp->table()->item(0, 2)->text(), QString("Missing"));
    }

    void removeOneThenResolveOtherOpensWithoutIt()
    {
        OpenResoraDialog dlg;
        blockBoth(dlg);
        RelinkPanel *rp = dlg.relinkPanel();
        const QByteArray src = slurp(p("l.resora")), img = slurp(p("moved_big.png"));
        int asked = 0;
        rp->setRemoveConfirmer([&](const QString &, const QStringList &) { ++asked; return false; });
        QVERIFY(!rp->requestRemove(idOf(rp, "big.png")));  // declined: nothing changes
        QCOMPARE(rp->model().count(resoraui::RelinkModel::Status::Removed), 0);
        rp->setRemoveConfirmer([&](const QString &, const QStringList &) { ++asked; return true; });
        QVERIFY(rp->requestRemove(idOf(rp, "big.png")));
        QCOMPARE(asked, 2);
        for (int i = 0; i < rp->model().rows().size(); ++i)
            if (rp->model().rows().at(i).name == "big.png")
                QCOMPARE(rp->table()->item(i, 2)->text(), QString("Removed from Dashboard"));
        QVERIFY(!rp->complete());          // the other image is still unresolved
        QVERIFY(!dlg.retryWithRelinks());  // still blocked
        QVERIFY(!rp->assign(idOf(rp, "big.png"), p("moved_big.png")));  // a removed row takes no replacement
        rp->assign(idOf(rp, "s.jpg"), p("moved_s.jpg"));
        QVERIFY(rp->complete());
        QVERIFY(dlg.retryWithRelinks());
        QTRY_VERIFY(!dlg.isRunning());
        QCOMPARE(dlg.state(), OpenResoraDialog::State::Done);
        const dashboard::Restored *r = dlg.session()->current();
        QVERIFY(r);
        QCOMPARE(r->removedRoles, (QStringList{"largest", "highest"}));
        QCOMPARE(r->elementImages.size(), 2);
        QCOMPARE(slurp(p("l.resora")), src);
        QCOMPARE(slurp(p("moved_big.png")), img);  // the image file itself is untouched
        QVERIFY(QFileInfo(p("moved_big.png")).isFile());
    }

    void removeAllAssociationsOpens()
    {
        OpenResoraDialog dlg;
        blockBoth(dlg);
        RelinkPanel *rp = dlg.relinkPanel();
        rp->setRemoveConfirmer([](const QString &, const QStringList &) { return true; });
        QVERIFY(rp->requestRemove(idOf(rp, "big.png")));
        QVERIFY(rp->requestRemove(idOf(rp, "s.jpg")));
        QVERIFY(rp->complete());
        QVERIFY(dlg.retryWithRelinks());
        QTRY_VERIFY(!dlg.isRunning());
        QCOMPARE(dlg.state(), OpenResoraDialog::State::Done);
        QCOMPARE(dlg.session()->current()->removedRoles.size(), 4);
        QVERIFY(dlg.session()->current()->elementImages.isEmpty());
    }

    void wrongReplacementRejectedAcceptedOneKeptAndClearRestores()
    {
        OpenResoraDialog dlg;
        blockBoth(dlg);
        RelinkPanel *rp = dlg.relinkPanel();
        put(p("fake.bin"), QByteArray(900, 'z'));
        rp->assign(idOf(rp, "big.png"), p("moved_big.png"));
        rp->assign(idOf(rp, "s.jpg"), p("fake.bin"));
        QVERIFY(dlg.retryWithRelinks());
        QTRY_VERIFY(!dlg.isRunning());
        QCOMPARE(dlg.state(), OpenResoraDialog::State::NeedsRelink);
        using S = resoraui::RelinkModel::Status;
        QCOMPARE(rp->model().find(idOf(rp, "s.jpg"))->status, S::WrongImage);  // rejected by size/hash
        QCOMPARE(rp->model().find(idOf(rp, "big.png"))->status, S::Resolved);  // accepted stays accepted
        QCOMPARE(rp->chosen().value(idOf(rp, "big.png")), p("moved_big.png"));
        QVERIFY(!rp->complete());
        // Clear Replacement returns an accepted association to unresolved (not removed)
        QVERIFY(rp->clearReplacement(idOf(rp, "big.png")));
        QCOMPARE(rp->model().find(idOf(rp, "big.png"))->status, S::Missing);
        QVERIFY(rp->chosen().isEmpty());
        QVERIFY(rp->omitted().isEmpty());
        QVERIFY(!rp->clearReplacement(idOf(rp, "big.png")));  // nothing left to clear
        // a pending choice can be cleared too
        rp->assign(idOf(rp, "big.png"), p("moved_big.png"));
        QCOMPARE(rp->model().find(idOf(rp, "big.png"))->status, S::ReplacementChosen);
        QVERIFY(rp->clearReplacement(idOf(rp, "big.png")));
        QVERIFY(!dlg.retryWithRelinks());  // still blocked while anything is unresolved
        rp->assign(idOf(rp, "big.png"), p("moved_big.png"));
        rp->assign(idOf(rp, "s.jpg"), p("moved_s.jpg"));
        QVERIFY(dlg.retryWithRelinks());
        QTRY_VERIFY(!dlg.isRunning());
        QCOMPARE(dlg.state(), OpenResoraDialog::State::Done);
    }
};

QTEST_MAIN(TstResoraUi)
#include "tst_resoraui.moc"
