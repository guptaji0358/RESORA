// Linked Data Export tests (Phase 3). Temporary data only; links resora_format + QtTest (no widgets).
#include "resora/linkedexport.h"
#include "resora/resorareader.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    QDir().mkpath(QFileInfo(p).absolutePath());
    QFile f(p);
    return f.open(QIODevice::WriteOnly) && f.write(d) == d.size();
}
QByteArray hexOf(const QByteArray &d) { return QCryptographicHash::hash(d, QCryptographicHash::Sha256).toHex(); }
QJsonObject jsonRes(Reader &r, const QString &id)
{
    QByteArray b;
    return r.readResource(id, &b).ok() ? QJsonDocument::fromJson(b).object() : QJsonObject();
}
QStringList tmpLeftovers(const QString &dir) { return QDir(dir).entryList({QStringLiteral("*.tmp")}, QDir::Files); }
}  // namespace

class TstLinkedExport : public QObject
{
    Q_OBJECT
    QTemporaryDir dir_;
    QString p(const QString &n) const { return dir_.filePath(n); }
    LinkedExportInput base() const
    {
        LinkedExportInput in;
        in.appVersion = QStringLiteral("test");
        in.layout = QJsonObject{{"active_view", "dashboard"}};
        in.charts = QJsonArray{QJsonObject{{"id", "c1"}, {"kind", "bar"}}};
        in.analytics = QJsonObject{{"total", 2}};
        in.engineProtocol = 1;
        in.config = QJsonObject{{"recursive", true}};
        return in;
    }
    LinkedImage img(const QString &path, const QByteArray &thumb = {}) const
    {
        LinkedImage e;
        e.sourcePath = path;
        e.thumbnail = thumb;
        e.metadata = QJsonObject{{"format", "PNG"}, {"width", 640}, {"height", 480}, {"file_size", 1}, {"status", "ok"}};
        return e;
    }

private slots:
    void initTestCase() { QVERIFY(dir_.isValid()); }

    void referencesThumbnailsAndNoImageBytes()  // 1-4, 13, 14
    {
        const QByteArray a = pattern(5000, 1), b = pattern(7000, 2);
        QVERIFY(put(p("imgs/a.png"), a));
        QVERIFY(put(p("imgs/b.png"), b));
        auto in = base();
        in.images = {img(p("imgs/a.png"), "T1"), img(p("imgs/b.png"), "T2")};
        const auto r = exportLinked(in, p("out1.resora"));
        QVERIFY2(r.ok(), qPrintable(r.message));
        QCOMPARE(r.imageBytes, quint64(12000));
        Reader rd;
        QVERIFY(rd.open(p("out1.resora")).ok());  // integrity (header/dir/file/resource hashes)
        QCOMPARE(rd.resources().size(), 8);
        for (const auto &e : rd.resources())
            QVERIFY(e.type != TypeImage);
        QVERIFY(!rd.contains("image-0001"));
        QByteArray whole;
        { QFile f(p("out1.resora")); QVERIFY(f.open(QIODevice::ReadOnly)); whole = f.readAll(); }
        QVERIFY(!whole.contains(a.left(256)));  // original bytes absent
        QVERIFY(!whole.contains(b.left(256)));
        QVERIFY(whole.size() < 12000);
        QCOMPARE(jsonRes(rd, "manifest").value("mode").toString(), QString("linked"));
        QCOMPARE(jsonRes(rd, "analytics").value("data").toObject().value("total").toInt(), 2);
        QByteArray t;
        QVERIFY(rd.readResource("thumb-0002", &t).ok());
        QCOMPARE(t, QByteArray("T2"));
        const auto recs = jsonRes(rd, "images-index").value("images").toArray();
        QCOMPARE(recs.size(), 2);
        const auto l = recs[0].toObject().value("linked").toObject();
        QCOMPARE(l.value("path").toString(), QDir::cleanPath(QFileInfo(p("imgs/a.png")).absoluteFilePath()));
        QCOMPARE(l.value("sha256").toString().toLatin1(), hexOf(a));
        QCOMPARE(l.value("size").toInt(), 5000);
        QCOMPARE(l.value("width").toInt(), 640);
        QCOMPARE(l.value("file_name").toString(), QString("a.png"));
        QVERIFY(l.value("relative_path").isNull());
        QVERIFY(recs[0].toObject().value("bundled").isUndefined());
        QCOMPARE(tmpLeftovers(dir_.path()), QStringList());
    }

    void commonResourcesMatchBundled()
    {
        QVERIFY(put(p("c.png"), pattern(100, 3)));
        auto in = base();
        in.images = {img(p("c.png"))};
        BundledExportInput bi;
        bi.appVersion = in.appVersion; bi.layout = in.layout; bi.charts = in.charts;
        bi.analytics = in.analytics; bi.engineProtocol = in.engineProtocol; bi.config = in.config;
        ExportImage ei; ei.sourcePath = p("c.png"); ei.metadata = in.images[0].metadata;
        bi.images = {ei};
        QVERIFY(exportLinked(in, p("cl.resora")).ok());
        QVERIFY(exportBundled(bi, p("cb.resora")).ok());
        Reader a, b;
        QVERIFY(a.open(p("cl.resora")).ok());
        QVERIFY(b.open(p("cb.resora")).ok());
        for (const char *id : {"layout", "charts", "analytics", "config"})
            QCOMPARE(a.info(id).sha256, b.info(id).sha256);
    }

    void duplicateFilenames()  // 5
    {
        QVERIFY(put(p("d1/x.png"), pattern(50, 1)));
        QVERIFY(put(p("d2/x.png"), pattern(60, 2)));
        auto in = base();
        in.images = {img(p("d1/x.png")), img(p("d2/x.png")), img(p("d1/x.png"))};
        const auto r = exportLinked(in, p("dup.resora"));
        QVERIFY2(r.ok(), qPrintable(r.message));
        QCOMPARE(r.duplicateNames.size(), 1);
        QCOMPARE(r.duplicateNames[0].indexes.size(), 3);
        QVERIFY(!r.duplicateNames[0].samePath);
        Reader rd;
        QVERIFY(rd.open(p("dup.resora")).ok());
        const auto recs = jsonRes(rd, "images-index").value("images").toArray();
        QSet<QString> ids, paths;
        for (const auto &v : recs) {
            QVERIFY(v.toObject().value("linked").toObject().value("name_collision").toBool());
            ids.insert(v.toObject().value("image_id").toString());
            paths.insert(v.toObject().value("linked").toObject().value("path").toString());
        }
        QCOMPARE(ids.size(), 3);
        QCOMPARE(paths.size(), 2);
    }

    void missingUnreadableAndChanged()  // 6, 7, 8
    {
        QVERIFY(put(p("ok.png"), pattern(10, 5)));
        QVERIFY(put(p("mod.png"), pattern(10, 6)));
        QVERIFY(QDir().mkpath(p("adir.png")));
        auto in = base();
        auto m = img(p("mod.png"));
        m.expectedSize = 10;
        m.expectedSha256 = QString::fromLatin1(hexOf(pattern(10, 7)));  // different content, same size
        in.images = {img(p("ok.png")), img(p("gone.png")), img(p("adir.png")), m};
        const auto r = exportLinked(in, p("bad.resora"));
        QCOMPARE(r.error, ExportError::SourceProblem);
        QCOMPARE(r.issues.size(), 3);
        QCOMPARE(r.issues[0].kind, SourceIssue::Missing);
        QCOMPARE(r.issues[1].kind, SourceIssue::Unreadable);
        QCOMPARE(r.issues[2].kind, SourceIssue::Changed);
        QVERIFY(!QFile::exists(p("bad.resora")));
        in.images = {m};
        m.expectedSha256.clear();
        m.expectedSize = 11;  // size differs
        in.images = {m};
        QCOMPARE(exportLinked(in, p("bad.resora")).issues.value(0).kind, SourceIssue::Changed);
        QCOMPARE(tmpLeftovers(dir_.path()), QStringList());
    }

    void relativeAndAbsolutePaths()  // 9
    {
        QVERIFY(put(p("root/sub/r.png"), pattern(20, 1)));
        QVERIFY(put(p("elsewhere/o.png"), pattern(20, 2)));
        auto in = base();
        in.images = {img(p("root/sub/r.png")), img(p("elsewhere/o.png"))};
        in.relativeBase = RelativeBase::Folder;
        in.baseDir = p("root");
        const auto r = exportLinked(in, p("rel.resora"));
        QVERIFY2(r.ok(), qPrintable(r.message));
        QCOMPARE(r.relativeCount, 1);
        Reader rd;
        QVERIFY(rd.open(p("rel.resora")).ok());
        const auto recs = jsonRes(rd, "images-index").value("images").toArray();
        QCOMPARE(recs[0].toObject().value("linked").toObject().value("relative_path").toString(), QString("sub/r.png"));
        QVERIFY(recs[1].toObject().value("linked").toObject().value("relative_path").isNull());  // never "../"
        QVERIFY(QDir::isAbsolutePath(recs[1].toObject().value("linked").toObject().value("path").toString()));
        const auto base = jsonRes(rd, "manifest").value("path_resolution").toObject().value("base").toObject();
        QCOMPARE(base.value("kind").toString(), QString("folder"));
        // document-relative: only when explicitly chosen
        in.relativeBase = RelativeBase::Document;
        QVERIFY(exportLinked(in, p("root/doc.resora")).ok());
        Reader d2;
        QVERIFY(d2.open(p("root/doc.resora")).ok());
        QCOMPARE(jsonRes(d2, "images-index").value("images").toArray()[0].toObject().value("linked").toObject()
                     .value("relative_path").toString(), QString("sub/r.png"));
        QCOMPARE(jsonRes(d2, "manifest").value("path_resolution").toObject().value("base").toObject().value("kind").toString(),
                 QString("document"));
        // invalid inputs
        in.relativeBase = RelativeBase::Folder;
        in.baseDir = p("nonexistent");
        QCOMPARE(exportLinked(in, p("x.resora")).error, ExportError::InvalidInput);
        in.relativeBase = RelativeBase::None;
        in.images = {img(QStringLiteral("rel/path.png"))};
        QCOMPARE(exportLinked(in, p("x.resora")).error, ExportError::InvalidInput);
    }

    void largeCollection()  // 10
    {
        auto in = base();
        QVERIFY(put(p("many/seed.png"), pattern(300, 9)));
        for (int i = 0; i < 1500; ++i)
            in.images.append(img(p("many/seed.png"), "t"));
        const auto r = exportLinked(in, p("large.resora"));
        QVERIFY2(r.ok(), qPrintable(r.message));
        Reader rd;
        QVERIFY(rd.open(p("large.resora")).ok());
        QCOMPARE(rd.resources().size(), 6 + 1500);
        QCOMPARE(jsonRes(rd, "images-index").value("images").toArray().size(), 1500);
    }

    void cancelAndExistingDestination()  // 11, 12
    {
        QVERIFY(put(p("big.png"), pattern(3 << 20, 4)));
        QVERIFY(put(p("keep.resora"), "previous"));
        auto in = base();
        in.images = {img(p("big.png"))};
        for (auto stage : {ExportProgress::Validating, ExportProgress::Writing, ExportProgress::Verifying}) {
            const auto r = exportLinked(in, p("keep.resora"), [&](const ExportProgress &pr) { return pr.stage != stage; });
            QCOMPARE(r.error, ExportError::Cancelled);
            QCOMPARE(tmpLeftovers(dir_.path()), QStringList());
        }
        { QFile f(p("keep.resora")); QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.readAll(), QByteArray("previous")); }
        QVERIFY(!QFile::exists(p("cancel.resora")));
        // failure keeps it too
        in.images = {img(p("nope.png"))};
        QCOMPARE(exportLinked(in, p("keep.resora")).error, ExportError::SourceProblem);
        { QFile f(p("keep.resora")); QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.readAll(), QByteArray("previous")); }
        // success replaces
        in.images = {img(p("big.png"))};
        int last = 0;
        bool monotonic = true;
        const auto ok = exportLinked(in, p("keep.resora"), [&](const ExportProgress &pr) {
            if (pr.permille < last)
                monotonic = false;
            last = pr.permille;
            return true;
        });
        QVERIFY2(ok.ok(), qPrintable(ok.message));
        QVERIFY(monotonic);
        QCOMPARE(last, 1000);
        Reader rd;
        QVERIFY(rd.open(p("keep.resora")).ok());
        QCOMPARE(tmpLeftovers(dir_.path()), QStringList());
    }

    // Same-size content change + restored mtime AFTER the initial hash: only the final rehash can see it.
    void finalRehashDetectsSameSizeEditWithRestoredMtime()
    {
        const QByteArray orig = pattern(4096, 11);
        QByteArray edited = orig;
        edited[100] = char(edited[100] ^ 0x5A);  // same size, different content
        QVERIFY(put(p("r1.png"), orig));
        QVERIFY(put(p("r2.png"), pattern(2048, 12)));
        QVERIFY(put(p("prev.resora"), "previous"));
        auto in = base();
        in.images = {img(p("r1.png")), img(p("r2.png"))};
        const QDateTime mtime = QFileInfo(p("r1.png")).lastModified();
        bool tampered = false;
        bool sawVerifyAfterTamper = false;
        int last = 0;
        bool monotonic = true, earlyFull = false;
        const auto r = exportLinked(in, p("prev.resora"), [&](const ExportProgress &pr) {
            if (pr.permille < last)
                monotonic = false;
            last = pr.permille;
            if (pr.stage != ExportProgress::Done && pr.permille >= 1000)
                earlyFull = true;
            if (!tampered && pr.stage == ExportProgress::Writing) {  // validation (initial hash) is finished
                QFile f(p("r1.png"));
                if (!f.open(QIODevice::WriteOnly) || f.write(edited) != edited.size())
                    return false;
                f.flush();
                f.setFileTime(mtime, QFileDevice::FileModificationTime);
                f.close();
                tampered = true;
            }
            if (tampered && pr.stage == ExportProgress::Verifying)
                sawVerifyAfterTamper = true;
            return true;
        });
        QVERIFY(tampered);
        QCOMPARE(QFileInfo(p("r1.png")).size(), qint64(orig.size()));
        QCOMPARE(QFileInfo(p("r1.png")).lastModified(), mtime);  // size and mtime are indistinguishable from before
        QCOMPARE(r.error, ExportError::SourceProblem);
        QCOMPARE(r.issues.size(), 1);
        QCOMPARE(r.issues[0].kind, SourceIssue::Changed);
        QCOMPARE(r.issues[0].index, 0);
        QVERIFY(sawVerifyAfterTamper);
        QVERIFY(monotonic);
        QVERIFY(!earlyFull);
        QVERIFY(last < 1000);
        { QFile f(p("prev.resora")); QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.readAll(), QByteArray("previous")); }
        QCOMPARE(tmpLeftovers(dir_.path()), QStringList());
    }

    void cancelDuringFinalRehash()
    {
        QVERIFY(put(p("cr.png"), pattern(3 << 20, 13)));
        QVERIFY(put(p("keepcr.resora"), "previous"));
        auto in = base();
        in.images = {img(p("cr.png"))};
        bool inRehash = false;
        const auto r = exportLinked(in, p("keepcr.resora"), [&](const ExportProgress &pr) {
            if (pr.stage == ExportProgress::Verifying && pr.done > 0) {  // bytes of the rehash, not the 0/1 tick
                inRehash = true;
                return false;
            }
            return true;
        });
        QVERIFY(inRehash);
        QCOMPARE(r.error, ExportError::Cancelled);
        { QFile f(p("keepcr.resora")); QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.readAll(), QByteArray("previous")); }
        QCOMPARE(tmpLeftovers(dir_.path()), QStringList());
    }

    void sourcesUnchangedAndDestinationNotASource()
    {
        const QByteArray d = pattern(400, 8);
        QVERIFY(put(p("u.png"), d));
        auto in = base();
        in.images = {img(p("u.png"))};
        QVERIFY(exportLinked(in, p("u.resora")).ok());
        QFile f(p("u.png"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), d);
        f.close();
        QCOMPARE(exportLinked(in, p("u.png")).error, ExportError::InvalidInput);
    }
};

QTEST_APPLESS_MAIN(TstLinkedExport)
#include "tst_linkedexport.moc"
