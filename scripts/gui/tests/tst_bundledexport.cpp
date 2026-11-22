// Bundled Export tests (Phase 2). Temporary data only; links resora_format + QtTest (no widgets).
#include "resora/bundledexport.h"
#include "resora/destinationfinalizer.h"
#include "resora/resorareader.h"

#include <QThread>
#include <thread>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
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
    QFile f(p);
    return f.open(QIODevice::WriteOnly) && f.write(d) == d.size();
}
QByteArray hashOf(const QString &p)
{
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData(&f);
    return h.result();
}
QJsonObject jsonRes(Reader &r, const QString &id)
{
    QByteArray b;
    return r.readResource(id, &b).ok() ? QJsonDocument::fromJson(b).object() : QJsonObject();
}
QStringList tmpLeftovers(const QString &dir)
{
    return QDir(dir).entryList({QStringLiteral("*.tmp")}, QDir::Files);
}
detail::FinalizeOps fakeOps(std::function<unsigned long(const QString &, const QString &, const QString &)> replace,
                            int *calls = nullptr, unsigned long restoreError = 0)
{
    detail::FinalizeOps o;
    o.attempts = 3;
    o.moveNew = [](const QString &from, const QString &to) -> unsigned long {
        return QFile::exists(to) ? 183ul : (QFile::rename(from, to) ? 0ul : 5ul);
    };
    o.restore = [restoreError](const QString &from, const QString &to) -> unsigned long {
        return restoreError ? restoreError : (QFile::rename(from, to) ? 0ul : 5ul);
    };
    o.replace = [replace, calls](const QString &d, const QString &t, const QString &b) {
        if (calls) ++*calls;
        return replace(d, t, b);
    };
    return o;
}
QByteArray slurp(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray("<missing>");
}
}  // namespace

class TstBundledExport : public QObject
{
    Q_OBJECT
    QTemporaryDir dir_;
    QString p(const QString &n) const { return dir_.filePath(n); }
    BundledExportInput base() const
    {
        BundledExportInput in;
        in.appVersion = QStringLiteral("test");
        in.layout = QJsonObject{{"active_view", "dashboard"}};
        in.charts = QJsonArray{QJsonObject{{"id", "c1"}, {"kind", "bar"}, {"series", QJsonArray()}},
                               QJsonObject{{"id", "c2"}, {"kind", "donut"}, {"series", QJsonArray()}},
                               QJsonObject{{"id", "c3"}, {"kind", "bar"}, {"series", QJsonArray()}}};
        in.analytics = QJsonObject{{"total", 3}};
        in.engineProtocol = 1;
        in.config = QJsonObject{{"recursive", true}};
        return in;
    }
    ExportImage img(const QString &path, const QByteArray &thumb = {}) const
    {
        ExportImage e;
        e.sourcePath = path;
        e.thumbnail = thumb;
        e.metadata = QJsonObject{{"format", "PNG"}, {"width", 1}, {"height", 1}, {"file_size", 0}, {"status", "ok"}};
        return e;
    }

private slots:
    void initTestCase() { QVERIFY(dir_.isValid()); }

    void multipleChartsAndImages()  // 1, 2, 6, 11, 12
    {
        QVERIFY(put(p("a.png"), pattern(5000, 1)));
        QVERIFY(put(p("b.png"), pattern(7000, 2)));
        auto in = base();
        in.images = {img(p("a.png"), "T1"), img(p("b.png"), "T2")};
        const auto r = exportBundled(in, p("out1.resora"));
        QVERIFY2(r.ok(), qPrintable(r.message));
        QCOMPARE(r.imageCount, 2);
        QCOMPARE(r.imageBytes, quint64(12000));
        Reader rd;
        QVERIFY(rd.open(p("out1.resora")).ok());
        const QStringList want{"manifest", "layout", "charts", "analytics", "config", "images-index",
                               "image-0001", "image-0002", "thumb-0001", "thumb-0002"};
        QCOMPARE(rd.resources().size(), want.size());
        for (const QString &id : want)
            QVERIFY2(rd.contains(id), qPrintable(id));
        QCOMPARE(jsonRes(rd, "charts").value("charts").toArray().size(), 3);
        QCOMPARE(jsonRes(rd, "manifest").value("mode").toString(), QString("bundled"));
        QCOMPARE(jsonRes(rd, "manifest").value("document_id").toString(), r.documentId);
        QCOMPARE(jsonRes(rd, "analytics").value("data").toObject().value("total").toInt(), 3);
        // checksums: stored resource hash == source hash, and the index records it
        const auto recs = jsonRes(rd, "images-index").value("images").toArray();
        QCOMPARE(recs.size(), 2);
        const QString srcs[2] = {p("a.png"), p("b.png")};
        for (int i = 0; i < 2; ++i) {
            const auto b = recs[i].toObject().value("bundled").toObject();
            QCOMPARE(b.value("resource").toString(), imageResourceId(i));
            QCOMPARE(rd.info(imageResourceId(i)).sha256, hashOf(srcs[i]));
            QCOMPARE(b.value("sha256").toString(), QString::fromLatin1(hashOf(srcs[i]).toHex()));
            QByteArray bytes;
            QVERIFY(rd.readResource(imageResourceId(i), &bytes).ok());
            QCOMPARE(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256), hashOf(srcs[i]));
        }
        QByteArray t;
        QVERIFY(rd.readResource("thumb-0002", &t).ok());
        QCOMPARE(t, QByteArray("T2"));
        QCOMPARE(tmpLeftovers(dir_.path()), QStringList());
    }

    void duplicateFilenames()  // 3
    {
        QVERIFY(QDir().mkpath(p("d1")) && QDir().mkpath(p("d2")));
        QVERIFY(put(p("d1/photo.png"), pattern(300, 3)));
        QVERIFY(put(p("d2/photo.png"), pattern(400, 4)));
        auto in = base();
        in.images = {img(p("d1/photo.png")), img(p("d2/photo.png"))};
        QVERIFY(exportBundled(in, p("dup.resora")).ok());
        Reader rd;
        QVERIFY(rd.open(p("dup.resora")).ok());
        const auto recs = jsonRes(rd, "images-index").value("images").toArray();
        QCOMPARE(recs[0].toObject().value("name").toString(), QString("photo.png"));
        QCOMPARE(recs[1].toObject().value("name").toString(), QString("photo.png"));
        QVERIFY(recs[0].toObject().value("image_id") != recs[1].toObject().value("image_id"));
        QCOMPARE(rd.info("image-0001").storedLength, quint64(300));
        QCOMPARE(rd.info("image-0002").storedLength, quint64(400));
        QCOMPARE(rd.info("image-0001").sha256, hashOf(p("d1/photo.png")));
        QCOMPARE(rd.info("image-0002").sha256, hashOf(p("d2/photo.png")));
    }

    void largeImageStreams()  // 4, 7
    {
        const qint64 n = 24 << 20;
        {
            QFile f(p("big.png"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            const QByteArray chunk = pattern(1 << 20, 9);
            for (int i = 0; i < 24; ++i)
                QCOMPARE(f.write(chunk), qint64(chunk.size()));
        }
        const QByteArray before = hashOf(p("big.png"));
        auto in = base();
        in.images = {img(p("big.png"))};
        int calls = 0;
        const auto r = exportBundled(in, p("big.resora"), [&](const ExportProgress &) { ++calls; return true; });
        QVERIFY2(r.ok(), qPrintable(r.message));
        QVERIFY(calls >= 24);  // reported chunk by chunk, so it was streamed rather than slurped
        QCOMPARE(r.imageBytes, quint64(n));
        Reader rd;
        QVERIFY(rd.open(p("big.resora")).ok());
        QCOMPARE(rd.info("image-0001").sha256, before);
        QCOMPARE(hashOf(p("big.png")), before);  // original unchanged
    }

    void emptyAndOptional()  // 5
    {
        QVERIFY(put(p("zero.png"), QByteArray()));
        auto in = base();
        in.layout = {};
        in.config = {};
        in.charts = {};
        in.images = {img(p("zero.png"))};  // empty file, no thumbnail
        QVERIFY(exportBundled(in, p("empty.resora")).ok());
        Reader rd;
        QVERIFY(rd.open(p("empty.resora")).ok());
        QVERIFY(rd.contains("image-0001"));
        QVERIFY(!rd.contains("thumb-0001"));
        QCOMPARE(rd.info("image-0001").storedLength, quint64(0));
        QVERIFY(jsonRes(rd, "images-index").value("images").toArray()[0].toObject().value("thumbnail").isNull());
        QCOMPARE(jsonRes(rd, "layout").value("schema_version").toInt(), 1);
        QVERIFY(jsonRes(rd, "manifest").value("source").isNull());
        // no images at all
        BundledExportInput none;
        QVERIFY(exportBundled(none, p("none.resora")).ok());
        Reader r2;
        QVERIFY(r2.open(p("none.resora")).ok());
        QCOMPARE(r2.resources().size(), 6);
    }

    void missingSource()  // 8
    {
        QVERIFY(put(p("ok.png"), pattern(10, 5)));
        auto in = base();
        in.images = {img(p("ok.png")), img(p("gone.png"))};
        const auto r = exportBundled(in, p("missing.resora"));
        QCOMPARE(r.error, ExportError::SourceProblem);
        QCOMPARE(r.issues.size(), 1);
        QCOMPARE(r.issues[0].index, 1);
        QCOMPARE(r.issues[0].kind, SourceIssue::Missing);
        QVERIFY(!QFile::exists(p("missing.resora")));
        QCOMPARE(tmpLeftovers(dir_.path()), QStringList());
    }

    void unreadableSource()  // 9
    {
        QVERIFY(QDir().mkpath(p("adir.png")));  // not a regular file => cannot be packaged
        auto in = base();
        in.images = {img(p("adir.png"))};
        const auto r = exportBundled(in, p("unread.resora"));
        QCOMPARE(r.error, ExportError::SourceProblem);
        QCOMPARE(r.issues[0].kind, SourceIssue::Unreadable);
        QVERIFY(!QFile::exists(p("unread.resora")));
    }

    void cancelLeavesNoPartialFile()  // 10
    {
        QVERIFY(put(p("c.png"), pattern(3 << 20, 6)));
        auto in = base();
        in.images = {img(p("c.png"))};
        for (auto stage : {ExportProgress::Validating, ExportProgress::Writing, ExportProgress::Verifying}) {
            const auto r = exportBundled(in, p("cancel.resora"),
                                         [&](const ExportProgress &pr) { return pr.stage != stage; });
            QCOMPARE(r.error, ExportError::Cancelled);
            QVERIFY(!QFile::exists(p("cancel.resora")));
            QCOMPARE(tmpLeftovers(dir_.path()), QStringList());
        }
    }

    void failedExportKeepsExistingTarget()  // 10
    {
        QVERIFY(put(p("keep.resora"), "previous"));
        auto in = base();
        in.images = {img(p("nope.png"))};
        QCOMPARE(exportBundled(in, p("keep.resora")).error, ExportError::SourceProblem);
        {
            QFile f(p("keep.resora"));  // closed before the replacing export (Windows locks open files)
            QVERIFY(f.open(QIODevice::ReadOnly));
            QCOMPARE(f.readAll(), QByteArray("previous"));
        }
        QCOMPARE(tmpLeftovers(dir_.path()), QStringList());
        // a successful export replaces it
        QVERIFY(put(p("n.png"), pattern(20, 7)));
        in.images = {img(p("n.png"))};
        const auto again = exportBundled(in, p("keep.resora"));
        QVERIFY2(again.ok(), qPrintable(again.message));
        Reader rd;
        QVERIFY(rd.open(p("keep.resora")).ok());
        QCOMPARE(tmpLeftovers(dir_.path()), QStringList());
        QCOMPARE(QDir(dir_.path()).entryList({"keep.resora.*"}, QDir::Files), QStringList());
    }

    void sourceChangedDuringExportFails()  // same-size content change with restored mtime, and a size change
    {
        for (int mode = 0; mode < 2; ++mode) {
            const QString src = p(QString("chg%1.png").arg(mode));
            const QString out = p(QString("chg%1.resora").arg(mode));
            QVERIFY(put(src, pattern(3 << 20, 11)));
            QVERIFY(put(out, "previous"));
            auto in = base();
            in.images = {img(src)};
            bool touched = false;
            const auto r = exportBundled(in, out, [&](const ExportProgress &pr) {
                if (pr.stage == ExportProgress::Writing && !touched) {
                    touched = true;  // after the first 1 MiB chunk was streamed: alter the tail
                    QFile f(src);
                    const QDateTime mt = QFileInfo(src).lastModified();
                    if (!f.open(QIODevice::ReadWrite)) return false;
                    f.seek(f.size() - 4);
                    f.write(mode == 0 ? "ZZZZ" : "ZZZZ-longer");  // mode 0: same size
                    f.setFileTime(mt, QFileDevice::FileModificationTime);  // hide the mtime change
                    f.close();
                }
                return true;
            });
            QVERIFY(touched);
            QCOMPARE(r.error, ExportError::SourceProblem);
            QCOMPARE(r.issues.size(), 1);
            QCOMPARE(r.issues[0].kind, SourceIssue::Changed);
            QCOMPARE(QFile(out).size(), qint64(8));  // previous destination intact
            QCOMPARE(tmpLeftovers(dir_.path()), QStringList());
        }
    }

    void lockedDestinationKeepsPrevious()  // Windows: an open file cannot be renamed away
    {
        QVERIFY(put(p("lock.png"), pattern(100, 12)));
        QVERIFY(put(p("lock.resora"), "previous"));
        auto in = base();
        in.images = {img(p("lock.png"))};
        {
            QFile held(p("lock.resora"));
            QVERIFY(held.open(QIODevice::ReadOnly));
            const auto r = exportBundled(in, p("lock.resora"));
#ifdef Q_OS_WIN
            QCOMPARE(r.error, ExportError::FinalizeFailed);
            QCOMPARE(held.readAll(), QByteArray("previous"));
#else
            QVERIFY(r.ok());  // POSIX can replace an open file
#endif
        }
#ifdef Q_OS_WIN
        QCOMPARE(QFile(p("lock.resora")).size(), qint64(8));
#endif
        QCOMPARE(tmpLeftovers(dir_.path()), QStringList());
        QVERIFY(QDir(dir_.path()).entryList({"lock.resora.*"}, QDir::Files).isEmpty());
    }

    void cancelKeepsExistingTargetAtEveryStage()
    {
        QVERIFY(put(p("cx.png"), pattern(3 << 20, 13)));
        auto in = base();
        in.images = {img(p("cx.png"))};
        for (auto stage : {ExportProgress::Validating, ExportProgress::Writing, ExportProgress::Verifying}) {
            QVERIFY(put(p("cx.resora"), "previous"));
            const auto r = exportBundled(in, p("cx.resora"), [&](const ExportProgress &pr) { return pr.stage != stage; });
            QCOMPARE(r.error, ExportError::Cancelled);
            QCOMPARE(QFile(p("cx.resora")).size(), qint64(8));
            QCOMPARE(tmpLeftovers(dir_.path()), QStringList());
        }
    }

    void progressIsMonotonicAndFinishesLast()
    {
        QVERIFY(put(p("pg.png"), pattern(5 << 20, 14)));
        auto in = base();
        in.images = {img(p("pg.png"), "T"), img(p("pg.png"))};
        int last = -1, doneCalls = 0;
        bool afterDone = false, fileExistedAtDone = false, early1000 = false;
        const auto r = exportBundled(in, p("pg.resora"), [&](const ExportProgress &pr) {
            if (pr.permille < last)
                last = -1000;  // flags a regression
            else
                last = pr.permille;
            if (afterDone)
                last = -1000;
            if (pr.stage == ExportProgress::Done) {
                ++doneCalls;
                afterDone = true;
                fileExistedAtDone = QFile::exists(p("pg.resora"));
            } else if (pr.permille >= 1000) {
                early1000 = true;
            }
            return true;
        });
        QVERIFY2(r.ok(), qPrintable(r.message));
        QVERIFY(last == 1000);  // never decreased, ended at 100%
        QCOMPARE(doneCalls, 1);
        QVERIFY(fileExistedAtDone);
        QVERIFY(!early1000);
    }

    void destinationMustNotBeASource()
    {
        QVERIFY(put(p("src.png"), pattern(20, 8)));
        const QByteArray before = hashOf(p("src.png"));
        auto in = base();
        in.images = {img(p("src.png"))};
        QCOMPARE(exportBundled(in, p("src.png")).error, ExportError::InvalidInput);
        QCOMPARE(hashOf(p("src.png")), before);
        QCOMPARE(exportBundled(in, p("nofolder/x.resora")).error, ExportError::InvalidInput);
    }

    // ---- destination finalization (ReplaceFileW / MoveFileExW) ----
    void finalizeCreatesNewDestination()
    {
        QVERIFY(put(p("fn.tmp"), "fresh"));
        const auto r = finalizeDestination(p("fn.tmp"), p("fn.resora"));
        QVERIFY2(r.ok(), qPrintable(r.message));
        QVERIFY(!QFile::exists(p("fn.tmp")));
        QCOMPARE(QFile(p("fn.resora")).size(), qint64(5));
    }

    void finalizeReplacesExistingWithoutLeftovers()
    {
        QVERIFY(put(p("fr.tmp"), "new-content"));
        QVERIFY(put(p("fr.resora"), "old"));
        const auto r = finalizeDestination(p("fr.tmp"), p("fr.resora"));
        QVERIFY2(r.ok(), qPrintable(r.message));
        QVERIFY(!QFile::exists(p("fr.tmp")));
        QFile f(p("fr.resora"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("new-content"));
        f.close();
        QCOMPARE(QDir(dir_.path()).entryList({"fr.resora.*"}, QDir::Files), QStringList());  // no backup left
    }

    void finalizeLockedDestinationKeepsPreviousAndTemp()
    {
        QVERIFY(put(p("fl.tmp"), "new"));
        QVERIFY(put(p("fl.resora"), "previous"));
        QFile held(p("fl.resora"));
        QVERIFY(held.open(QIODevice::ReadOnly));
        const auto r = finalizeDestination(p("fl.tmp"), p("fl.resora"));
#ifdef Q_OS_WIN
        QCOMPARE(r.status, FinalizeStatus::DestinationLocked);
        QVERIFY(!r.message.isEmpty());
        QVERIFY(r.osError != 0);
        QVERIFY(QFile::exists(p("fl.tmp")));  // caller owns cleanup of the temp
        QCOMPARE(held.readAll(), QByteArray("previous"));
#else
        QVERIFY(r.ok());
#endif
        held.close();
        QCOMPARE(QDir(dir_.path()).entryList({"fl.resora.*"}, QDir::Files), QStringList());  // no stray backup
    }

    void finalizeRetriesTransientLock()
    {
        QVERIFY(put(p("ft.tmp"), "new"));
        QVERIFY(put(p("ft.resora"), "previous"));
        QFile held(p("ft.resora"));
        QVERIFY(held.open(QIODevice::ReadOnly));
        std::thread releaser([&] {
            QThread::msleep(80);
            held.close();
        });
        const auto r = finalizeDestination(p("ft.tmp"), p("ft.resora"));
        releaser.join();
        QVERIFY2(r.ok(), qPrintable(r.message));
        QCOMPARE(QFile(p("ft.resora")).size(), qint64(3));
        QVERIFY(!QFile::exists(p("ft.tmp")));
    }

    void finalizeReadOnlyDestinationKeepsPrevious()
    {
        QVERIFY(put(p("fo.tmp"), "new"));
        QVERIFY(put(p("fo.resora"), "previous"));
        QVERIFY(QFile::setPermissions(p("fo.resora"), QFileDevice::ReadOwner));
        const auto r = finalizeDestination(p("fo.tmp"), p("fo.resora"));
        QFile::setPermissions(p("fo.resora"), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#ifdef Q_OS_WIN
        QVERIFY(!r.ok());
        QCOMPARE(QFile(p("fo.resora")).size(), qint64(8));
        QVERIFY(QFile::exists(p("fo.tmp")));
#else
        Q_UNUSED(r);
#endif
        QVERIFY(QFile::exists(p("fo.resora")));
    }

    // ---- documented ReplaceFileW failure shapes (1175/1176/1177), simulated through the ops seam ----
    bool setupPair(const char *name)
    {
        return put(p(QString("%1.tmp").arg(name)), "NEW") && put(p(QString("%1.resora").arg(name)), "OLD");
    }

    void finalize1175And1176KeepEverythingAtOriginalNames()
    {
        for (unsigned long code : {1175ul, 1176ul}) {
            const QString n = QString("c%1").arg(code);
            setupPair(qPrintable(n));
            const auto r = detail::finalizeWith(fakeOps([code](auto &, auto &, auto &) { return code; }),
                                                p(n + ".tmp"), p(n + ".resora"));
            QCOMPARE(r.status, FinalizeStatus::DestinationLocked);
            QVERIFY(!r.keepTemp);
            QCOMPARE(slurp(p(n + ".resora")), QByteArray("OLD"));
            QCOMPARE(slurp(p(n + ".tmp")), QByteArray("NEW"));
            QCOMPARE(QDir(dir_.path()).entryList({n + ".resora.*"}, QDir::Files), QStringList());
        }
    }

    void finalize1177RestoresOldDestinationFromBackupName()
    {
        setupPair("c1177");
        const auto r = detail::finalizeWith(
            fakeOps([](const QString &d, auto &, const QString &b) { QFile::rename(d, b); return 1177ul; }),  // documented: replaced file moved to backup name
            p("c1177.tmp"), p("c1177.resora"));
        QCOMPARE(r.status, FinalizeStatus::DestinationLocked);
        QVERIFY(!r.keepTemp);
        QCOMPARE(slurp(p("c1177.resora")), QByteArray("OLD"));  // restored
        QCOMPARE(slurp(p("c1177.tmp")), QByteArray("NEW"));     // new export preserved
        QCOMPARE(QDir(dir_.path()).entryList({"c1177.resora.*"}, QDir::Files), QStringList());
    }

    void finalize1177WithFailedRestorePreservesBothAndKeepsTemp()
    {
        setupPair("c1177b");
        int calls = 0;
        const auto r = detail::finalizeWith(
            fakeOps([](const QString &d, auto &, const QString &b) { QFile::rename(d, b); return 1177ul; }, &calls, 5),
            p("c1177b.tmp"), p("c1177b.resora"));
        QCOMPARE(r.status, FinalizeStatus::NeedsAttention);
        QVERIFY(r.keepTemp);
        QCOMPARE(calls, 1);  // never retried from an uncertain state
        QCOMPARE(slurp(p("c1177b.tmp")), QByteArray("NEW"));
        const QStringList bak = QDir(dir_.path()).entryList({"c1177b.resora.*"}, QDir::Files);
        QCOMPARE(bak.size(), 1);
        QCOMPARE(slurp(p(bak[0])), QByteArray("OLD"));  // only copy of the previous export survives
        QVERIFY(r.message.contains(bak[0]));
        QVERIFY(r.message.contains("c1177b.tmp"));
    }

    void finalizeUnexpectedStatesAreNeverDeleted()
    {
        // (a) both the destination and a backup exist after a failure: roles not provable
        setupPair("ua");
        auto r = detail::finalizeWith(
            fakeOps([](const QString &, auto &, const QString &b) { return put(b, "BAK") ? 1176ul : 1ul; }),
            p("ua.tmp"), p("ua.resora"));
        QCOMPARE(r.status, FinalizeStatus::NeedsAttention);
        QVERIFY(r.keepTemp);
        QCOMPARE(slurp(p("ua.resora")), QByteArray("OLD"));
        QCOMPARE(slurp(p("ua.tmp")), QByteArray("NEW"));
        QCOMPARE(QDir(dir_.path()).entryList({"ua.resora.*"}, QDir::Files).size(), 1);
        // (b) destination vanished and no backup exists
        setupPair("ub");
        r = detail::finalizeWith(fakeOps([](const QString &d, auto &, auto &) { QFile::remove(d); return 1177ul; }),
                                 p("ub.tmp"), p("ub.resora"));
        QCOMPARE(r.status, FinalizeStatus::NeedsAttention);
        QVERIFY(r.keepTemp);
        QCOMPARE(slurp(p("ub.tmp")), QByteArray("NEW"));
        // (c) the temp file vanished too
        setupPair("uc");
        r = detail::finalizeWith(fakeOps([](const QString &d, const QString &t, const QString &b) {
                                     QFile::rename(d, b); QFile::remove(t); return 1177ul; }),
                                 p("uc.tmp"), p("uc.resora"));
        QCOMPARE(r.status, FinalizeStatus::NeedsAttention);
        QVERIFY(r.keepTemp);
        QCOMPARE(QDir(dir_.path()).entryList({"uc.resora.*"}, QDir::Files).size(), 1);  // backup kept
    }

    void finalizeSuccessRemovesOnlyTheBackup()
    {
        setupPair("ok");
        const auto r = detail::finalizeWith(
            fakeOps([](const QString &d, const QString &t, const QString &b) { QFile::rename(d, b); QFile::rename(t, d); return 0ul; }),
            p("ok.tmp"), p("ok.resora"));
        QVERIFY(r.ok());
        QCOMPARE(slurp(p("ok.resora")), QByteArray("NEW"));
        QCOMPARE(QDir(dir_.path()).entryList({"ok.resora.*"}, QDir::Files), QStringList());
    }

    void finalizeTransientErrorWithCleanStateIsRetried()
    {
        setupPair("rt");
        int calls = 0;
        const auto r = detail::finalizeWith(
            fakeOps([&calls](const QString &d, const QString &t, const QString &b) {
                if (calls == 1) return 32ul;  // sharing violation, nothing changed
                QFile::rename(d, b); QFile::rename(t, d); return 0ul; }, &calls),
            p("rt.tmp"), p("rt.resora"));
        QVERIFY2(r.ok(), qPrintable(r.message));
        QCOMPARE(calls, 2);
        QCOMPARE(slurp(p("rt.resora")), QByteArray("NEW"));
    }

    void finalizeRejectsMissingTempAndFolderDestination()
    {
        QVERIFY(put(p("fm.resora"), "previous"));
        QCOMPARE(finalizeDestination(p("absent.tmp"), p("fm.resora")).status, FinalizeStatus::TempMissing);
        QCOMPARE(QFile(p("fm.resora")).size(), qint64(8));
        QVERIFY(put(p("fd.tmp"), "x"));
        QVERIFY(QDir(dir_.path()).mkpath("adir"));
        QCOMPARE(finalizeDestination(p("fd.tmp"), p("adir")).status, FinalizeStatus::DestinationInvalid);
        QVERIFY(QFile::exists(p("fd.tmp")));
    }
};

QTEST_APPLESS_MAIN(TstBundledExport)
#include "tst_bundledexport.moc"
