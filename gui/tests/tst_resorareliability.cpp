// Phase 7 reliability tests: stale-cache sweep, extraction hardening, repeated session lifecycle.
// Deterministic generated fixtures only; links resora_format + QtTest (no widgets).
#include "resora/bundledexport.h"
#include "resora/resoraimport.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <filesystem>

using namespace resora;

namespace {
bool put(const QString &p, const QByteArray &d)
{
    QDir().mkpath(QFileInfo(p).absolutePath());
    QFile f(p);
    return f.open(QIODevice::WriteOnly) && f.write(d) == d.size();
}
QByteArray pattern(int n, int seed)
{
    QByteArray b(n, '\0');
    for (int i = 0; i < n; ++i)
        b[i] = char((i * 31 + seed) & 0xFF);
    return b;
}
const QString kId = QStringLiteral("0123456789abcdef0123456789abcdef");
QString docName(int n) { return QStringLiteral("doc-%1").arg(QString::number(n).rightJustified(32, '0')); }

qint64 deadPid()  // pid of a process that has certainly exited
{
    QProcess pr;
#ifdef Q_OS_WIN
    pr.start("cmd", {"/c", "exit", "0"});
#else
    pr.start("true", {});
#endif
    pr.waitForStarted();
    const qint64 pid = pr.processId();
    pr.waitForFinished(10000);
    return pid;
}
QByteArray marker(qint64 pid, const char *app = "RESORA")
{
    return QStringLiteral("{\"app\":\"%1\",\"kind\":\"resora-document-cache\",\"version\":1,\"document_id\":\"%2\",\"pid\":%3}")
        .arg(QString::fromLatin1(app), kId).arg(pid).toUtf8();
}

BundledExportInput oneImage(const QString &src)
{
    BundledExportInput in;
    in.appVersion = "t";
    in.analytics = QJsonObject{{"total", 1}};
    ExportImage e;
    e.sourcePath = src;
    e.metadata = QJsonObject{{"format", "PNG"}, {"width", 1}, {"height", 1}};
    e.thumbnail = "TH";
    in.images.append(e);
    return in;
}
}  // namespace

class TstReliability : public QObject
{
    Q_OBJECT
    QTemporaryDir dir_;
    QString p(const QString &n) const { return dir_.filePath(n); }
    bool exists(const QString &n) const { return QFileInfo::exists(p(n)); }

    void mkDoc(const QString &root, const QString &name, const QByteArray &mk, const QStringList &files = {"image-0001.png"})
    {
        if (!mk.isEmpty())
            QVERIFY(put(p(root + "/" + name + "/.resora-cache"), mk));
        for (const QString &f : files)
            QVERIFY(put(p(root + "/" + name + "/" + f), "data"));
    }
    SweepOptions now() const
    {
        SweepOptions o;
        o.minAgeSeconds = 0;
        o.unknownOwnerMinAgeSeconds = 0;
        return o;
    }

private slots:
    void initTestCase() { QVERIFY(dir_.isValid()); }

    void sweepRemovesOnlyDeadOwnerFolders()
    {
        const qint64 dead = deadPid();
        QVERIFY(dead > 0);
        mkDoc("s1", docName(1), marker(dead), {"image-0001.png", "image-0002.jpg.part"});
        const SweepReport r = sweepStaleCache(p("s1"), now());
        QCOMPARE(r.removed, 1);
        QVERIFY(!exists("s1/" + docName(1)));
        QVERIFY(exists("s1"));  // the root itself is never removed
    }

    void sweepKeepsActiveRecentAndUnprovable()
    {
        const qint64 dead = deadPid();
        mkDoc("s2", docName(1), marker(QCoreApplication::applicationPid()));  // this process: active
        mkDoc("s2", docName(2), marker(dead));                                   // dead owner, intact content
        mkDoc("s2", docName(3), {});                                             // no marker
        mkDoc("s2", docName(4), marker(dead, "OtherApp"));                       // foreign marker
        mkDoc("s2", docName(5), "garbage");                                      // unparsable marker
        mkDoc("s2", docName(6), marker(dead), {"image-0001.png", "notes.txt"});  // unexpected file
        mkDoc("s2", docName(7), marker(dead), {"image-0001.png", "sub/nested.png"});  // unexpected subfolder
        mkDoc("s2", "doc-lookalike", marker(dead));                               // name is not doc-<32 hex>
        mkDoc("s2", "other-folder", marker(dead));
        QVERIFY(put(p("s2/stray.txt"), "mine"));
        const SweepReport a = sweepStaleCache(p("s2"), SweepOptions{});  // default 1 h: fresh folders are kept
        QCOMPARE(a.removed, 0);
        const SweepReport b = sweepStaleCache(p("s2"), now());
        QCOMPARE(b.removed, 1);  // only doc 2 once the age requirement is waived
        QVERIFY(!exists("s2/" + docName(2)));
        for (int i : {1, 3, 4, 5, 6, 7})
            QVERIFY2(exists("s2/" + docName(i)), qPrintable(docName(i)));
        QVERIFY(exists("s2/doc-lookalike/.resora-cache") && exists("s2/other-folder") && exists("s2/stray.txt"));
        QVERIFY(exists("s2/" + docName(7) + "/sub/nested.png"));
        QCOMPARE(b.skippedActive, 1);
        QVERIFY(b.skippedUnsafe >= 7);
    }

    void sweepLegacyMarkerNeedsAgeAndMissingRootIsNoop()
    {
        mkDoc("s3", docName(1), kId.toLatin1());  // Phase 4 marker: document id only, owner unknown
        QCOMPARE(sweepStaleCache(p("s3")).removed, 0);  // default: 7 days
        QCOMPARE(sweepStaleCache(p("s3"), now()).removed, 1);
        const SweepReport none = sweepStaleCache(p("does-not-exist"));
        QCOMPARE(none.examined, 0);
        QCOMPARE(none.removed, 0);
    }

    // Directory symlink, or on Windows a junction (needs no privilege). False when neither can be made.
    static bool makeDirLink(const QString &link, const QString &target)
    {
        std::error_code ec;
        std::filesystem::create_directory_symlink(std::filesystem::path(target.toStdWString()),
                                                  std::filesystem::path(link.toStdWString()), ec);
        if (!ec)
            return true;
#ifdef Q_OS_WIN
        return QProcess::execute("cmd", {"/c", "mklink", "/J", QDir::toNativeSeparators(link), QDir::toNativeSeparators(target)}) == 0;
#else
        return false;
#endif
    }

    void sweepNeverFollowsLinks()
    {
        mkDoc("victim", "keep", marker(deadPid()));
        QVERIFY(put(p("victim/keep/image-0001.png"), "precious"));
        QDir().mkpath(p("s4"));
        if (!makeDirLink(p("s4/" + docName(1)), p("victim/keep")))
            QSKIP("cannot create a directory link in this environment");
        QCOMPARE(sweepStaleCache(p("s4"), now()).removed, 0);
        QVERIFY(exists("victim/keep/image-0001.png"));
        // a linked cache root is not followed either
        mkDoc("s4real", docName(2), marker(deadPid()));
        if (makeDirLink(p("rootlink"), p("s4real"))) {
            QCOMPARE(sweepStaleCache(p("rootlink"), now()).removed, 0);
            QVERIFY(exists("s4real/" + docName(2)));
        }
    }

    void sweepDoesNotTouchOpenSession()
    {
        QVERIFY(put(p("src/a.png"), pattern(5000, 3)));
        QVERIFY(exportBundled(oneImage(p("src/a.png")), p("a.resora")).ok());
        OpenOptions o;
        o.cacheRoot = p("s5");
        OpenResult r = openResora(p("a.resora"), o);
        QVERIFY2(r.ok(), qPrintable(r.message));
        const SweepReport rep = sweepStaleCache(p("s5"), now());
        QCOMPARE(rep.removed, 0);
        QCOMPARE(rep.skippedActive, 1);
        QVERIFY(QFileInfo::exists(r.document.images[0].path));
        QVERIFY(closeDocument(&r.document));
    }

    void extractedNamesNeverExecutable()  // display name is untrusted: only known image extensions survive
    {
        const QStringList names{"payload.exe", "run.ps1", "photo.PNG", "noext", "dbl.png.bat"};
        const QStringList want{"img", "img", "png", "img", "img"};
        for (int i = 0; i < names.size(); ++i) {
            const QString src = p("n/" + names[i]);
            QVERIFY(put(src, pattern(2000, i)));
            const QString out = p(QStringLiteral("n%1.resora").arg(i));
            QVERIFY(exportBundled(oneImage(src), out).ok());
            OpenOptions o;
            o.cacheRoot = p("s6");
            OpenResult r = openResora(out, o);
            QVERIFY2(r.ok(), qPrintable(r.message));
            QCOMPARE(QFileInfo(r.document.images[0].path).suffix(), want[i]);
            QVERIFY(closeDocument(&r.document));
        }
    }

    // Non-ASCII names and a long target path. Measured (UTF-16 units): folder `deep` ~266 and target ~283 with a
    // QTemporaryDir under C:\Users\<user>\AppData\Local\Temp\<6 chars>. Lengths depend on the user name and temp path;
    // the target is only beyond MAX_PATH (260) because of that base. The assertions check length > 200, not > 260.
    void unicodeAndLongPathRoundTrip()
    {
        QString deep = p(QStringLiteral("ünïcode 写真 dir"));
        for (int i = 0; i < 6; ++i)
            deep += QStringLiteral("/nested_folder_segment_%1_abcdefghij").arg(i);
        QVERIFY(deep.size() > 200);
        QVERIFY(QDir().mkpath(deep));
        const QString src = deep + QStringLiteral("/источник_画像.png");
        const QByteArray bytes = pattern(5000, 7);
        QVERIFY(put(src, bytes));
        BundledExportInput in;
        in.appVersion = "t";
        in.analytics = QJsonObject{{"total", 1}};
        ExportImage e;
        e.sourcePath = src;
        e.metadata = QJsonObject{{"format", "PNG"}, {"width", 1}, {"height", 1}};
        e.thumbnail = "TH";
        in.images.append(e);
        const QString target = deep + QStringLiteral("/结果_export.resora");
        QVERIFY(exportBundled(in, target).ok());
        OpenOptions o;
        o.cacheRoot = p("uni_cache");
        OpenResult r = openResora(target, o);
        QVERIFY2(r.ok(), qPrintable(r.message));
        QCOMPARE(r.document.images.size(), 1);
        {
            QFile f(r.document.images[0].path);  // scoped: an open handle would block cache removal
            QVERIFY(f.open(QIODevice::ReadOnly));
            QCOMPARE(f.readAll(), bytes);
        }
        QVERIFY(closeDocument(&r.document));
        QVERIFY(QDir(p("uni_cache")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    }

    void repeatedOpenCloseReleasesEverything()  // 25 cycles, alternating documents, nothing left behind
    {
        QStringList outs;
        for (int d = 0; d < 2; ++d) {
            BundledExportInput in;
            in.appVersion = "t";
            in.analytics = QJsonObject{{"total", 3}};
            for (int i = 0; i < 3; ++i) {
                const QString src = p(QStringLiteral("rc%1/i%2.png").arg(d).arg(i));
                QVERIFY(put(src, pattern(4000 + i * 50, d * 10 + i)));
                ExportImage e;
                e.sourcePath = src;
                e.metadata = QJsonObject{{"format", "PNG"}, {"width", 1}, {"height", 1}};
                e.thumbnail = "TH";
                in.images.append(e);
            }
            outs << p(QStringLiteral("rc%1.resora").arg(d));
            QVERIFY(exportBundled(in, outs.last()).ok());
        }
        OpenOptions o;
        o.cacheRoot = p("s7");
        OpenResult active = openResora(outs[0], o);
        QVERIFY(active.ok());
        for (int c = 0; c < 25; ++c) {
            OpenResult next = openResora(outs[(c + 1) % 2], o);
            QVERIFY2(next.ok(), qPrintable(next.message));
            QVERIFY(QFileInfo::exists(active.document.images[0].path));  // old session intact until swapped
            QVERIFY(closeDocument(&active.document));
            active = std::move(next);
        }
        QCOMPARE(QDir(p("s7")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size(), 1);
        QVERIFY(closeDocument(&active.document));
        QVERIFY(QDir(p("s7")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    }
};

QTEST_MAIN(TstReliability)
#include "tst_resorareliability.moc"
