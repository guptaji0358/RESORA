// Low-disk and storage-failure reliability tests for Bundled and Linked export (Phase 7).
// Failures are injected through detail::ExportSeams (simulated capacity, damaged temp file, scripted
// replacement), never by filling a real disk. Temporary data only; links resora_format + QtTest.
#include "resora/bundledexport.h"
#include "resora/destinationfinalizer.h"
#include "resora/exportinternal.h"
#include "resora/exportseams.h"
#include "resora/linkedexport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace resora;

namespace {

QByteArray pattern(qint64 n, int seed)
{
    QByteArray b(int(n), '\0');
    for (int i = 0; i < b.size(); ++i)
        b[i] = char((i * 31 + seed) & 0xFF);
    return b;
}

bool put(const QString &p, const QByteArray &d)
{
    QDir().mkpath(QFileInfo(p).absolutePath());
    QFile f(p);
    return f.open(QIODevice::WriteOnly) && f.write(d) == d.size();
}

QByteArray slurp(const QString &p)
{
    QFile f(p);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray("<missing>");
}

// Names of everything in `dir` (no . or ..), sorted: the exact folder contents.
QStringList entries(const QString &dir)
{
    QStringList e = QDir(dir).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    e.sort();
    return e;
}

// Records every progress event; never cancels.
struct Recorder
{
    QList<ExportProgress> events;
    ExportProgressFn fn()
    {
        return [this](const ExportProgress &p) {
            events.append(p);
            return true;
        };
    }
    bool saw(ExportProgress::Stage s) const
    {
        for (const ExportProgress &p : events)
            if (p.stage == s)
                return true;
        return false;
    }
    bool sawDone() const { return saw(ExportProgress::Done); }
    int maxPermille() const
    {
        int m = 0;
        for (const ExportProgress &p : events)
            m = qMax(m, p.permille);
        return m;
    }
};

// Scripted destination swap for the finalizer seam: the replace step renames the old file to the backup
// name, removes the temp (as if lost) and reports ReplaceFileW 1177; the restore step fails. The old
// content then survives only under the backup name, and the state is uncertain.
FinalizeResult uncertainSwap(const QString &tmp, const QString &target)
{
    detail::FinalizeOps o;
    o.attempts = 1;
    o.moveNew = [](const QString &from, const QString &to) -> unsigned long {
        return QFile::exists(to) ? 183ul : (QFile::rename(from, to) ? 0ul : 5ul);
    };
    o.replace = [](const QString &dst, const QString &src, const QString &bak) -> unsigned long {
        QFile::rename(dst, bak);
        QFile::remove(src);
        return 1177ul;
    };
    o.restore = [](const QString &, const QString &) -> unsigned long { return 5ul; };
    return detail::finalizeWith(o, tmp, target);
}

// Replace is refused and nothing changes on disk (documented 1176 shape): clean failure.
FinalizeResult refusedSwap(const QString &tmp, const QString &target)
{
    detail::FinalizeOps o;
    o.attempts = 1;
    o.moveNew = [](const QString &from, const QString &to) -> unsigned long {
        return QFile::exists(to) ? 183ul : (QFile::rename(from, to) ? 0ul : 5ul);
    };
    o.replace = [](const QString &, const QString &, const QString &) -> unsigned long { return 1176ul; };
    o.restore = o.moveNew;
    return detail::finalizeWith(o, tmp, target);
}

// A volume with plenty of room (the real disk is never measured by these tests).
detail::ExportSeams roomy()
{
    detail::ExportSeams s;
    s.availableBytes = [](const QString &) -> qint64 { return qint64(1) << 40; };
    return s;
}

}  // namespace

class TstExportLowDisk : public QObject
{
    Q_OBJECT
    QTemporaryDir dir_;
    QString p(const QString &n) const { return dir_.filePath(n); }
    // Every case starts with an empty destination folder so exact folder listings stay meaningful.
    void resetOut() { QDir(p(QStringLiteral("out"))).removeRecursively(); QDir().mkpath(p(QStringLiteral("out"))); }

    BundledExportInput bundledBase(const QList<ExportImage> &imgs) const
    {
        BundledExportInput in;
        in.appVersion = QStringLiteral("test");
        in.layout = QJsonObject{{"active_view", "dashboard"}};
        in.charts = QJsonArray{QJsonObject{{"id", "c1"}, {"kind", "bar"}, {"series", QJsonArray()}}};
        in.analytics = QJsonObject{{"total", 1}};
        in.engineProtocol = 1;
        in.config = QJsonObject{{"recursive", true}};
        in.images = imgs;
        return in;
    }
    ExportImage bundledImg(const QString &path) const
    {
        ExportImage e;
        e.sourcePath = path;
        e.metadata = QJsonObject{{"format", "PNG"}, {"width", 1}, {"height", 1}, {"file_size", 0}, {"status", "ok"}};
        return e;
    }
    LinkedExportInput linkedBase(const QList<LinkedImage> &imgs) const
    {
        LinkedExportInput in;
        in.appVersion = QStringLiteral("test");
        in.layout = QJsonObject{{"active_view", "dashboard"}};
        in.charts = QJsonArray{QJsonObject{{"id", "c1"}, {"kind", "bar"}}};
        in.analytics = QJsonObject{{"total", 1}};
        in.engineProtocol = 1;
        in.config = QJsonObject{{"recursive", true}};
        in.images = imgs;
        return in;
    }
    LinkedImage linkedImg(const QString &path) const
    {
        LinkedImage e;
        e.sourcePath = path;
        e.metadata = QJsonObject{{"format", "PNG"}, {"width", 640}, {"height", 480}, {"file_size", 1}, {"status", "ok"}};
        return e;
    }

private slots:
    void initTestCase() { QVERIFY(dir_.isValid()); }

    // ---------------- Bundled ----------------

    void bundledInsufficientSpaceDetectedBeforeWriting()
    {
        resetOut();
        QVERIFY(put(p("src/a.png"), pattern(1 << 20, 1)));
        QVERIFY(put(p("out/keep.resora"), "previous"));
        auto seams = roomy();
        seams.availableBytes = [](const QString &) -> qint64 { return 0; };
        Recorder rec;
        const auto in = bundledBase({bundledImg(p("src/a.png"))});
        const auto r = detail::exportBundledWith(in, p("out/keep.resora"), rec.fn(), seams);
        QCOMPARE(r.error, ExportError::InsufficientStorage);
        QVERIFY(!rec.saw(ExportProgress::Writing));  // refused before any byte was written
        QCOMPARE(slurp(p("out/keep.resora")), QByteArray("previous"));
        QCOMPARE(entries(p("out")), QStringList{"keep.resora"});
    }

    void bundledStorageExhaustedDuringPayloadWriting()
    {
        resetOut();
        QVERIFY(put(p("src/big.png"), pattern(4 << 20, 2)));
        QVERIFY(put(p("out/keep.resora"), "previous"));
        auto seams = roomy();  // pre-check passes, the device then runs out
        seams.writeLimit = 1 << 20;
        Recorder rec;
        const auto r = detail::exportBundledWith(bundledBase({bundledImg(p("src/big.png"))}), p("out/keep.resora"),
                                                 rec.fn(), seams);
        QCOMPARE(r.error, ExportError::InsufficientStorage);
        QVERIFY(rec.saw(ExportProgress::Writing));
        QVERIFY(rec.maxPermille() < 1000);
        QVERIFY(!rec.sawDone());
        QCOMPARE(slurp(p("out/keep.resora")), QByteArray("previous"));
        QCOMPARE(entries(p("out")), QStringList{"keep.resora"});  // partial temp removed
    }

    void bundledStorageExhaustedInTrailer()
    {
        resetOut();
        QVERIFY(put(p("src/t.png"), pattern(64 << 10, 3)));
        const auto in = bundledBase({bundledImg(p("src/t.png"))});
        const auto probe = detail::exportBundledWith(in, p("out/probe.resora"), {}, roomy());
        QVERIFY2(probe.ok(), qPrintable(probe.message));
        QVERIFY(put(p("out/keep.resora"), "previous"));
        auto seams = roomy();
        seams.writeLimit = probe.fileBytes - 1;  // everything fits except the last bytes (trailer)
        const auto r = detail::exportBundledWith(in, p("out/keep.resora"), {}, seams);
        QCOMPARE(r.error, ExportError::InsufficientStorage);
        QCOMPARE(slurp(p("out/keep.resora")), QByteArray("previous"));
        QCOMPARE(entries(p("out")), (QStringList{"keep.resora", "probe.resora"}));
    }

    void bundledSilentDataLossCaughtByVerification()
    {
        resetOut();
        QVERIFY(put(p("src/v.png"), pattern(256 << 10, 4)));
        QVERIFY(put(p("out/keep.resora"), "previous"));
        auto seams = roomy();
        seams.afterWrite = [](const QString &tmp) { QFile::resize(tmp, QFileInfo(tmp).size() / 2); };
        Recorder rec;
        const auto r = detail::exportBundledWith(bundledBase({bundledImg(p("src/v.png"))}), p("out/keep.resora"),
                                                 rec.fn(), seams);
        QCOMPARE(r.error, ExportError::VerifyFailed);
        QVERIFY(!rec.sawDone());
        QCOMPARE(slurp(p("out/keep.resora")), QByteArray("previous"));
        QCOMPARE(entries(p("out")), QStringList{"keep.resora"});
    }

    void bundledCleanReplaceRefusalKeepsPrevious()
    {
        resetOut();
        QVERIFY(put(p("src/r.png"), pattern(32 << 10, 5)));
        QVERIFY(put(p("out/keep.resora"), "previous"));
        auto seams = roomy();
        seams.finalize = refusedSwap;
        Recorder rec;
        const auto r = detail::exportBundledWith(bundledBase({bundledImg(p("src/r.png"))}), p("out/keep.resora"),
                                                 rec.fn(), seams);
        QCOMPARE(r.error, ExportError::FinalizeFailed);
        QVERIFY(!rec.sawDone());
        QVERIFY(rec.maxPermille() < 1000);
        QCOMPARE(slurp(p("out/keep.resora")), QByteArray("previous"));
        QCOMPARE(entries(p("out")), QStringList{"keep.resora"});  // temp removed: state was clean
    }

    void bundledUncertainReplaceKeepsEverySurvivingFile()
    {
        resetOut();
        QVERIFY(put(p("src/u.png"), pattern(32 << 10, 6)));
        QVERIFY(put(p("out/keep.resora"), "previous"));
        auto seams = roomy();
        seams.finalize = uncertainSwap;
        Recorder rec;
        const auto r = detail::exportBundledWith(bundledBase({bundledImg(p("src/u.png"))}), p("out/keep.resora"),
                                                 rec.fn(), seams);
        QCOMPARE(r.error, ExportError::FinalizeFailed);
        QVERIFY(r.message.contains(QStringLiteral("Nothing was deleted")) || r.message.contains(QStringLiteral("unexpected")));
        QVERIFY(!rec.sawDone());
        // The old content was never deleted: it survives under the backup name.
        const QStringList left = entries(p("out"));
        QCOMPARE(left.size(), 1);
        QVERIFY(left.first().startsWith(QStringLiteral("keep.resora.")));
        QCOMPARE(slurp(p("out/") + left.first()), QByteArray("previous"));
    }

    void bundledSuccessAfterFailuresLeavesOnlyDestination()
    {
        resetOut();
        QVERIFY(put(p("src/s.png"), pattern(32 << 10, 7)));
        QVERIFY(put(p("out/fresh.resora"), "previous"));
        Recorder rec;
        const auto r = detail::exportBundledWith(bundledBase({bundledImg(p("src/s.png"))}), p("out/fresh.resora"),
                                                 rec.fn(), roomy());
        QVERIFY2(r.ok(), qPrintable(r.message));
        QVERIFY(rec.sawDone());
        QCOMPARE(rec.events.last().stage, ExportProgress::Done);
        QCOMPARE(rec.events.last().permille, 1000);
        QCOMPARE(rec.events.first(rec.events.size() - 1).size() > 0, true);
        for (int i = 0; i + 1 < rec.events.size(); ++i)
            QVERIFY(rec.events[i].permille < 1000);  // 100% only with the final Done event
        QCOMPARE(entries(p("out")), QStringList{"fresh.resora"});
    }

    // ---------------- Linked ----------------

    void linkedNeedsOnlyContainerSpaceNotImageBytes()
    {
        resetOut();
        // Same tight volume: Bundled must store ~16 MiB of images and is refused; Linked stores none.
        QVERIFY(put(p("src/l1.png"), pattern(8 << 20, 8)));
        QVERIFY(put(p("src/l2.png"), pattern(8 << 20, 9)));
        auto seams = roomy();
        seams.reserveBytes = 1 << 20;
        seams.availableBytes = [](const QString &) -> qint64 { return 2 << 20; };
        const QList<ExportImage> bImgs{bundledImg(p("src/l1.png")), bundledImg(p("src/l2.png"))};
        QCOMPARE(detail::exportBundledWith(bundledBase(bImgs), p("out/b.resora"), {}, seams).error,
                 ExportError::InsufficientStorage);
        QCOMPARE(entries(p("out")), QStringList());
        Recorder rec;
        const auto r = detail::exportLinkedWith(linkedBase({linkedImg(p("src/l1.png")), linkedImg(p("src/l2.png"))}),
                                                p("out/l.resora"), rec.fn(), seams);
        QVERIFY2(r.ok(), qPrintable(r.message));
        QCOMPARE(r.fileBytes < (1u << 20), true);  // container is metadata only
        QCOMPARE(entries(p("out")), QStringList{"l.resora"});
        QCOMPARE(QFileInfo(p("src/l1.png")).size(), qint64(8 << 20));  // sources untouched
    }

    void linkedInsufficientSpaceDetectedBeforeWriting()
    {
        resetOut();
        QVERIFY(put(p("src/x.png"), pattern(64 << 10, 10)));
        QVERIFY(put(p("out/keep.resora"), "previous"));
        auto seams = roomy();
        seams.availableBytes = [](const QString &) -> qint64 { return 0; };
        Recorder rec;
        const auto r = detail::exportLinkedWith(linkedBase({linkedImg(p("src/x.png"))}), p("out/keep.resora"),
                                                rec.fn(), seams);
        QCOMPARE(r.error, ExportError::InsufficientStorage);
        QVERIFY(!rec.saw(ExportProgress::Writing));
        QCOMPARE(slurp(p("out/keep.resora")), QByteArray("previous"));
        QCOMPARE(entries(p("out")), QStringList{"keep.resora"});
    }

    void linkedStorageExhaustedDuringWriting()
    {
        resetOut();
        QVERIFY(put(p("src/w.png"), pattern(64 << 10, 11)));
        QVERIFY(put(p("out/keep.resora"), "previous"));
        LinkedImage thumbed = linkedImg(p("src/w.png"));
        thumbed.thumbnail = pattern(512 << 10, 12);  // container big enough to exhaust mid-stream
        auto seams = roomy();
        seams.writeLimit = 128 << 10;
        Recorder rec;
        const auto r = detail::exportLinkedWith(linkedBase({thumbed}), p("out/keep.resora"), rec.fn(), seams);
        QCOMPARE(r.error, ExportError::InsufficientStorage);
        QVERIFY(rec.maxPermille() < 1000);
        QVERIFY(!rec.sawDone());
        QCOMPARE(slurp(p("out/keep.resora")), QByteArray("previous"));
        QCOMPARE(entries(p("out")), QStringList{"keep.resora"});
    }

    void linkedSilentDataLossCaughtByVerification()
    {
        resetOut();
        QVERIFY(put(p("src/y.png"), pattern(64 << 10, 13)));
        QVERIFY(put(p("out/keep.resora"), "previous"));
        auto seams = roomy();
        seams.afterWrite = [](const QString &tmp) { QFile::resize(tmp, QFileInfo(tmp).size() - 10); };
        Recorder rec;
        const auto r = detail::exportLinkedWith(linkedBase({linkedImg(p("src/y.png"))}), p("out/keep.resora"),
                                                rec.fn(), seams);
        QCOMPARE(r.error, ExportError::VerifyFailed);
        QVERIFY(!rec.sawDone());
        QCOMPARE(slurp(p("out/keep.resora")), QByteArray("previous"));
        QCOMPARE(entries(p("out")), QStringList{"keep.resora"});
    }

    void linkedReplaceRefusalKeepsPrevious()
    {
        resetOut();
        QVERIFY(put(p("src/z.png"), pattern(64 << 10, 14)));
        QVERIFY(put(p("out/keep.resora"), "previous"));
        auto seams = roomy();
        seams.finalize = refusedSwap;
        Recorder rec;
        const auto r = detail::exportLinkedWith(linkedBase({linkedImg(p("src/z.png"))}), p("out/keep.resora"),
                                                rec.fn(), seams);
        QCOMPARE(r.error, ExportError::FinalizeFailed);
        QVERIFY(!rec.sawDone());
        QCOMPARE(slurp(p("out/keep.resora")), QByteArray("previous"));
        QCOMPARE(entries(p("out")), QStringList{"keep.resora"});
    }

    void linkedUncertainReplaceKeepsEverySurvivingFile()
    {
        resetOut();
        QVERIFY(put(p("src/q.png"), pattern(64 << 10, 15)));
        QVERIFY(put(p("out/keep.resora"), "previous"));
        auto seams = roomy();
        seams.finalize = uncertainSwap;
        Recorder rec;
        const auto r = detail::exportLinkedWith(linkedBase({linkedImg(p("src/q.png"))}), p("out/keep.resora"),
                                                rec.fn(), seams);
        QCOMPARE(r.error, ExportError::FinalizeFailed);
        QVERIFY(!rec.sawDone());
        const QStringList left = entries(p("out"));
        QCOMPARE(left.size(), 1);
        QCOMPARE(slurp(p("out/") + left.first()), QByteArray("previous"));
    }

    void linkedDistinctErrorCodesPerFailure()
    {
        resetOut();
        // Cancel, space and verification failures must stay distinguishable for the UI.
        QVERIFY(put(p("src/d.png"), pattern(64 << 10, 16)));
        const auto in = linkedBase({linkedImg(p("src/d.png"))});
        auto cancel = roomy();
        const auto c = detail::exportLinkedWith(in, p("out/d.resora"),
                                                [](const ExportProgress &pr) { return pr.stage != ExportProgress::Writing; },
                                                cancel);
        QCOMPARE(c.error, ExportError::Cancelled);
        auto space = roomy();
        space.writeLimit = 1;
        const auto s = detail::exportLinkedWith(in, p("out/d.resora"), {}, space);
        QCOMPARE(s.error, ExportError::InsufficientStorage);
        auto verify = roomy();
        verify.afterWrite = [](const QString &tmp) { QFile::resize(tmp, 16); };
        const auto v = detail::exportLinkedWith(in, p("out/d.resora"), {}, verify);
        QCOMPARE(v.error, ExportError::VerifyFailed);
        QCOMPARE(entries(p("out")), QStringList());
    }
};

QTEST_APPLESS_MAIN(TstExportLowDisk)
#include "tst_exportlowdisk.moc"
