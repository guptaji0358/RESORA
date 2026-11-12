// Import / opening engine tests (Phase 4). Temporary data only; links resora_format + QtTest (no widgets).
#include "resora/importinternal.h"
#include "resora/linkedexport.h"
#include "resora/resoraimport.h"
#include "resora/resorawriter.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

#ifdef Q_OS_WIN
#include <windows.h>
#include <string>
#endif

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
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
QByteArray hexOf(const QByteArray &d) { return QCryptographicHash::hash(d, QCryptographicHash::Sha256).toHex(); }
QJsonObject meta() { return QJsonObject{{"format", "PNG"}, {"width", 8}, {"height", 8}}; }
}  // namespace

class TstImport : public QObject
{
    Q_OBJECT
    QTemporaryDir dir_;
    QString p(const QString &n) const { return dir_.filePath(n); }

    // Writes N images under imgs/ and returns their paths.
    QStringList makeImages(int n, const QString &sub = QStringLiteral("imgs"))
    {
        QStringList l;
        for (int i = 0; i < n; ++i) {
            const QString f = p(QStringLiteral("%1/i%2.png").arg(sub).arg(i));
            put(f, pattern(3000 + i * 111, i + 1));
            l << f;
        }
        return l;
    }
    bool exportB(const QStringList &imgs, const QString &out)
    {
        BundledExportInput in;
        in.appVersion = "t";
        in.analytics = QJsonObject{{"total", imgs.size()}};
        for (const QString &f : imgs) {
            ExportImage e;
            e.sourcePath = f;
            e.metadata = meta();
            e.thumbnail = "TH";
            in.images.append(e);
        }
        return exportBundled(in, out).ok();
    }
    bool exportL(const QStringList &imgs, const QString &out, RelativeBase rb = RelativeBase::None, const QString &baseDir = {})
    {
        LinkedExportInput in;
        in.appVersion = "t";
        in.analytics = QJsonObject{{"total", imgs.size()}};
        in.relativeBase = rb;
        in.baseDir = baseDir;
        for (const QString &f : imgs) {
            LinkedImage e;
            e.sourcePath = f;
            e.metadata = meta();
            in.images.append(e);
        }
        const auto r = exportLinked(in, out);
        if (!r.ok())
            qWarning() << r.message;
        return r.ok();
    }
    OpenOptions opts(const QString &cache = QStringLiteral("cache")) const
    {
        OpenOptions o;
        o.cacheRoot = dir_.filePath(cache);
        return o;
    }

private slots:
    void initTestCase() { QVERIFY(dir_.isValid()); }

    void bundledImportExtractsAndVerifies()
    {
        const QStringList imgs = makeImages(3, "b1");
        QVERIFY(exportB(imgs, p("b1.resora")));
        const QByteArray before = slurp(p("b1.resora"));
        auto r = openResora(p("b1.resora"), opts("c1"));
        QVERIFY2(r.ok(), qPrintable(r.message));
        QVERIFY(!r.document.linked);
        QCOMPARE(r.document.images.size(), 3);
        QCOMPARE(r.document.analytics.value("data").toObject().value("total").toInt(), 3);
        for (int i = 0; i < 3; ++i) {
            const ImageRef &im = r.document.images[i];
            QVERIFY(im.path.startsWith(QDir::cleanPath(p("c1"))));
            QCOMPARE(slurp(im.path), slurp(imgs[i]));
            QCOMPARE(im.sha256.toLatin1(), hexOf(slurp(imgs[i])));
            QCOMPARE(readThumbnail(r.document, i), QByteArray("TH"));
        }
        QCOMPARE(slurp(p("b1.resora")), before);  // container untouched
        QVERIFY(QDir(r.document.cacheDir).entryList({"*.part"}).isEmpty());
        const QString extracted = r.document.cacheDir;
        QVERIFY(closeDocument(&r.document));
        QVERIFY(!QFileInfo::exists(extracted));
        QVERIFY(QDir(p("c1")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    }

    void linkedAbsoluteSucceeds()
    {
        const QStringList imgs = makeImages(2, "l1");
        QVERIFY(exportL(imgs, p("l1.resora")));
        auto r = openResora(p("l1.resora"), opts());
        QVERIFY2(r.ok(), qPrintable(r.message));
        QVERIFY(r.document.linked);
        QCOMPARE(r.links.size(), 2);
        QCOMPARE(QDir::cleanPath(r.document.images[1].path), QDir::cleanPath(imgs[1]));
        QVERIFY(!r.links[0].viaRelative);
    }

    void linkedRelativeFolderBaseAfterMove()
    {
        const QStringList imgs = makeImages(2, "root1/sub");
        QVERIFY(exportL(imgs, p("l2.resora"), RelativeBase::Folder, p("root1")));
        QVERIFY(QDir().rename(p("root1"), p("moved1")));
        // Without the new image root the hint (old folder) is gone -> blocked.
        auto blocked = openResora(p("l2.resora"), opts());
        QCOMPARE(blocked.error, OpenError::LinksBlocked);
        QCOMPARE(blocked.problems.size(), 2);
        auto o = opts();
        o.imageRoot = p("moved1");
        auto r = openResora(p("l2.resora"), o);
        QVERIFY2(r.ok(), qPrintable(r.message));
        QVERIFY(r.links[0].viaRelative);
        QCOMPARE(slurp(r.document.images[0].path), pattern(3000, 1));
    }

    void linkedDocumentRelativeBase()
    {
        const QStringList imgs = makeImages(1, "docdir/pics");
        QVERIFY(exportL(imgs, p("docdir/d.resora"), RelativeBase::Document));
        QVERIFY(QDir().rename(p("docdir"), p("docdir2")));
        auto r = openResora(p("docdir2/d.resora"), opts());
        QVERIFY2(r.ok(), qPrintable(r.message));
        QVERIFY(r.links[0].viaRelative);
    }

    void relativeNeverAssumedWithoutBase()  // .resora folder is not the image folder
    {
        const QStringList imgs = makeImages(1, "nb");
        QVERIFY(exportL(imgs, p("nb/n.resora")));
        QVERIFY(QDir().rename(p("nb"), p("nb2")));
        QCOMPARE(openResora(p("nb2/n.resora"), opts()).error, OpenError::LinksBlocked);
    }

    // ---- linked failures ----
    void linkedMissingAndMultipleReportedTogether()
    {
        const QStringList imgs = makeImages(4, "m1");
        QVERIFY(exportL(imgs, p("m1.resora")));
        QVERIFY(QFile::remove(imgs[0]));                         // missing
        QVERIFY(QFile::remove(imgs[2]));
        QVERIFY(QDir().mkpath(imgs[2]));                         // directory in its place -> unreadable
        QVERIFY(put(imgs[3], pattern(3000 + 3 * 111, 99)));      // same name+size, other content
        auto r = openResora(p("m1.resora"), opts());
        QCOMPARE(r.error, OpenError::LinksBlocked);
        QCOMPARE(r.problems.size(), 3);
        QCOMPARE(r.problems[0].state, LinkState::Missing);
        QCOMPARE(r.problems[1].state, LinkState::Unreadable);
        QCOMPARE(r.problems[2].state, LinkState::IdentityMismatch);
        QCOMPARE(r.problems[0].name, QString("i0.png"));
        QVERIFY(!r.problems[0].recordedPath.isEmpty());
        QVERIFY(r.document.images.isEmpty());  // no partial document
        QCOMPARE(r.links.size(), 4);
        QVERIFY(r.links[1].ok());
    }

    void sameNameDifferentSizeAndSameSizeRestoredMtime()
    {
        const QStringList imgs = makeImages(2, "s1");
        QVERIFY(exportL(imgs, p("s1.resora")));
        const QDateTime mt = QFileInfo(imgs[1]).lastModified();
        QVERIFY(put(imgs[0], pattern(10, 5)));
        QByteArray d = slurp(imgs[1]);
        d[100] = char(d[100] ^ 0x01);
        QVERIFY(put(imgs[1], d));
        { QFile f(imgs[1]); QVERIFY(f.open(QIODevice::ReadWrite)); QVERIFY(f.setFileTime(mt, QFileDevice::FileModificationTime)); }
        QCOMPARE(QFileInfo(imgs[1]).lastModified(), mt);
        auto r = openResora(p("s1.resora"), opts());
        QCOMPARE(r.error, OpenError::LinksBlocked);
        QCOMPARE(r.problems.size(), 2);
        QCOMPARE(r.problems[0].state, LinkState::IdentityMismatch);
        QCOMPARE(r.problems[1].state, LinkState::IdentityMismatch);
    }

    void relinkRevalidatesEverything()
    {
        const QStringList imgs = makeImages(3, "r1");
        QVERIFY(exportL(imgs, p("r1.resora")));
        const QByteArray good0 = slurp(imgs[0]);
        QVERIFY(QDir().rename(p("r1"), p("r1gone")));
        QVERIFY(put(p("elsewhere/x.png"), good0));
        QVERIFY(put(p("elsewhere/wrong.png"), pattern(good0.size(), 77)));
        auto r = openResora(p("r1.resora"), opts());
        QCOMPARE(r.error, OpenError::LinksBlocked);
        QCOMPARE(r.problems.size(), 3);
        const QString id0 = r.problems[0].imageId, id1 = r.problems[1].imageId, id2 = r.problems[2].imageId;
        // wrong file at the right size is rejected, never substituted
        auto w = relinkAndOpen(p("r1.resora"), opts(), {{id0, p("elsewhere/wrong.png")}});
        QCOMPARE(w.error, OpenError::LinksBlocked);
        QCOMPARE(w.problems.size(), 3);
        QCOMPARE(w.problems[0].state, LinkState::IdentityMismatch);
        QVERIFY(w.problems[0].viaRelink);
        // relinking only one leaves the others blocking
        auto one = relinkAndOpen(p("r1.resora"), opts(), {{id0, p("elsewhere/x.png")}});
        QCOMPARE(one.error, OpenError::LinksBlocked);
        QCOMPARE(one.problems.size(), 2);
        QVERIFY(one.links[0].ok() && one.links[0].viaRelink);
        // restore the other two and relink the first: all revalidated -> opens
        QVERIFY(QDir().rename(p("r1gone"), p("r1")));
        QVERIFY(put(imgs[0], pattern(5, 1)));  // original location of #0 now holds other content
        auto all = relinkAndOpen(p("r1.resora"), opts(), {{id0, p("elsewhere/x.png")}});
        QVERIFY2(all.ok(), qPrintable(all.message));
        QCOMPARE(all.document.images[0].path, QDir::cleanPath(p("elsewhere/x.png")));
        // a previously valid link that changed after the relink is caught on the next open
        QVERIFY(put(imgs[2], pattern(3000 + 222, 42)));
        auto again = relinkAndOpen(p("r1.resora"), opts(), {{id0, p("elsewhere/x.png")}});
        QCOMPARE(again.error, OpenError::LinksBlocked);
        QCOMPARE(again.problems.size(), 1);
        QCOMPARE(again.problems[0].imageId, id2);
        Q_UNUSED(id1)
    }
private:

    // ---- crafted / hostile containers ----
    QString craft(const QString &name, const QString &mode, const QJsonArray &recs, const QJsonObject &extraManifest = {},
                  const QList<QPair<QString, QByteArray>> &images = {})
    {
        Writer w;
        const auto js = [](const QJsonObject &o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); };
        QJsonObject m{{"schema_version", 1}, {"mode", mode}, {"document_id", QString(32, 'a')}, {"image_count", recs.size()}};
        for (auto it = extraManifest.begin(); it != extraManifest.end(); ++it)
            m[it.key()] = it.value();
        w.addBytes("manifest", TypeJson, js(m));
        w.addBytes("layout", TypeJson, js({{"schema_version", 1}}));
        w.addBytes("charts", TypeJson, js({{"schema_version", 1}, {"charts", QJsonArray()}}));
        w.addBytes("analytics", TypeJson, js({{"schema_version", 1}}));
        w.addBytes("config", TypeJson, js({{"schema_version", 1}}));
        w.addBytes("images-index", TypeJson, js({{"schema_version", 1}, {"images", recs}}));
        for (const auto &im : images)
            w.addBytes(im.first, TypeImage, im.second);
        const QString out = p(name);
        return w.write(out).ok() ? out : QString();
    }
    QJsonObject linkedRec(const QString &id, const QString &path, const QString &rel, const QByteArray &data)
    {
        return {{"image_id", id}, {"name", "x.png"}, {"metadata", QJsonObject()},
                {"linked", QJsonObject{{"path", path}, {"relative_path", rel.isEmpty() ? QJsonValue() : QJsonValue(rel)},
                                       {"size", data.size()}, {"sha256", QString::fromLatin1(hexOf(data))}}}};
    }
    QJsonObject bundledRec(const QString &id, const QString &res, const QByteArray &data)
    {
        return {{"image_id", id}, {"name", "x.png"}, {"metadata", QJsonObject()},
                {"bundled", QJsonObject{{"resource", res}, {"size", data.size()}, {"sha256", QString::fromLatin1(hexOf(data))}}}};
    }

private slots:
    void unsafeRelativePathsRejected()
    {
        const QByteArray d = pattern(500, 9);
        QVERIFY(put(p("outside/secret.png"), d));
        QDir().mkpath(p("inside"));
        const QStringList bad = {"../outside/secret.png", "a/../../outside/secret.png", "/abs/x.png", "C:/x.png",
                                 "..\\outside\\secret.png", "a//b.png", "./x.png"};
        for (const QString &rel : bad) {
            // the absolute path is missing, so only the relative path could (wrongly) succeed
            const QString f = craft("u.resora", "linked", {linkedRec("img-1", p("nowhere/x.png"), rel, d)},
                                    {{"path_resolution", QJsonObject{{"policy", "absolute-then-relative"},
                                                                     {"base", QJsonObject{{"kind", "folder"}, {"hint_path", p("inside")}}}}}});
            QVERIFY(!f.isEmpty());
            auto o = opts();
            o.imageRoot = p("inside");
            QCOMPARE(openResora(f, o).error, OpenError::LinksBlocked);
        }
    }

    // ---- Phase 7: local-only path policy (decided from the string, before any filesystem access) ----
    QJsonObject folderBase(const QString &hint) const
    {
        return {{"path_resolution", QJsonObject{{"policy", "absolute-then-relative"},
                                                {"base", QJsonObject{{"kind", "folder"}, {"hint_path", hint}}}}}};
    }

    void networkPathsAreNotLocalAbsolute()
    {
        using resora::detail::isLocalAbsolutePath;
        for (const QString &bad : {QStringLiteral("\\\\host\\share\\x.png"), QStringLiteral("//host/share/x.png"),
                                   QStringLiteral("\\\\?\\C:\\x.png"), QStringLiteral("\\\\.\\pipe\\x"),
                                   QStringLiteral("\\/host/share"), QStringLiteral("//?/UNC/host/share/x.png"), QString(),
                                   QStringLiteral("rel/x.png"), QStringLiteral("C:x.png")})
            QVERIFY2(!isLocalAbsolutePath(bad), qPrintable(bad));
        QVERIFY(isLocalAbsolutePath(p("a/b.png")));
        QVERIFY(isLocalAbsolutePath(QStringLiteral("C:/images/a.png")));
    }

#ifdef Q_OS_WIN
    // Mapped-drive policy: judged from the DOS-device table (QueryDosDeviceW), never from the remote file system.
    // SUBST-style mappings made with DefineDosDevice need no privilege and never touch the network.
    static wchar_t freeLetter()
    {
        for (wchar_t c = L'Y'; c >= L'L'; --c) {
            wchar_t dev[3] = {c, L':', 0}, buf[8];
            if (QueryDosDeviceW(dev, buf, 8) == 0 && GetLastError() == ERROR_FILE_NOT_FOUND)
                return c;
        }
        return 0;
    }

    void driveLettersLocalRemoteAndSubst()
    {
        const QChar sys = QDir::rootPath().at(0);  // C: on this machine
        QVERIFY(!resora::detail::isRemoteDriveLetter(sys));
        QVERIFY(resora::detail::isLocalAbsolutePath(QDir::rootPath() + QStringLiteral("images/a.png")));
        wchar_t l1 = freeLetter();
        if (!l1)
            QSKIP("no free drive letter for a SUBST probe");
        const wchar_t dev1[3] = {l1, L':', 0};
        QVERIFY(!resora::detail::isRemoteDriveLetter(QChar(l1)));  // unassigned: nothing to contact, reported later as "not found"
        const std::wstring local = QDir::toNativeSeparators(p("inside")).toStdWString();
        QDir().mkpath(p("inside"));
        QVERIFY(DefineDosDeviceW(0, dev1, local.c_str()));
        const bool localSubst = !resora::detail::isRemoteDriveLetter(QChar(l1));  // SUBST of a local folder stays local
        // A drive substituted to a UNC target (no connection is made by defining it) is remote.
        wchar_t l2 = 0;
        for (wchar_t c = l1 - 1; c >= L'L'; --c) {
            wchar_t d[3] = {c, L':', 0}, b[8];
            if (QueryDosDeviceW(d, b, 8) == 0 && GetLastError() == ERROR_FILE_NOT_FOUND) { l2 = c; break; }
        }
        bool uncSubst = true, chained = true, pathRejected = true;
        if (l2) {
            const wchar_t dev2[3] = {l2, L':', 0};
            if (DefineDosDeviceW(DDD_RAW_TARGET_PATH, dev2, L"\\??\\UNC\\resora-no-such-host\\share")) {
                uncSubst = resora::detail::isRemoteDriveLetter(QChar(l2));
                pathRejected = !resora::detail::isLocalAbsolutePath(QString(QChar(l2)) + QStringLiteral(":/x.png"));
                // SUBST of a drive that is itself remote: judged transitively
                wchar_t l3 = 0;
                for (wchar_t c = l2 - 1; c >= L'L'; --c) {
                    wchar_t d[3] = {c, L':', 0}, b[8];
                    if (QueryDosDeviceW(d, b, 8) == 0 && GetLastError() == ERROR_FILE_NOT_FOUND) { l3 = c; break; }
                }
                if (l3) {
                    const wchar_t dev3[3] = {l3, L':', 0};
                    const std::wstring tgt = std::wstring(1, l2) + L":\\";
                    if (DefineDosDeviceW(0, dev3, tgt.c_str())) {
                        chained = resora::detail::isRemoteDriveLetter(QChar(l3));
                        DefineDosDeviceW(DDD_REMOVE_DEFINITION, dev3, nullptr);
                    }
                }
                DefineDosDeviceW(DDD_REMOVE_DEFINITION, dev2, nullptr);
            }
        }
        DefineDosDeviceW(DDD_REMOVE_DEFINITION, dev1, nullptr);
        QVERIFY2(localSubst, "SUBST of a local folder must stay local");
        QVERIFY2(uncSubst, "SUBST to a UNC target must be remote");
        QVERIFY2(pathRejected, "a path on a remote drive letter must not be 'local absolute'");
        QVERIFY2(chained, "SUBST of a remote drive must be remote");
    }
#endif

    void networkRecordedPathRejectedWithoutAccess()
    {
        const QByteArray d = pattern(400, 2);
        for (const QString &unc : {QStringLiteral("\\\\resora-no-such-host\\share\\x.png"), QStringLiteral("//resora-no-such-host/share/x.png"),
                                   QStringLiteral("\\\\?\\UNC\\resora-no-such-host\\share\\x.png")}) {
            const QString f = craft("unc1.resora", "linked", {linkedRec("img-1", unc, QString(), d)});
            QVERIFY(!f.isEmpty());
            QElapsedTimer t;
            t.start();
            const auto r = openResora(f, opts());
            QCOMPARE(r.error, OpenError::LinksBlocked);
            QCOMPARE(r.links[0].state, LinkState::UnsafePath);  // an access attempt would have produced Missing/Unreadable
            QVERIFY(r.links[0].message.contains("not accessed"));
            QVERIFY2(t.elapsed() < 3000, "a network lookup seems to have happened");
        }
    }

    void networkRelinkRejectedSamePolicyNoFallback()
    {
        const QByteArray d = pattern(450, 6);
        QVERIFY(put(p("nr/ok.png"), d));  // the recorded path is valid, so a silent fallback would succeed
        const QString f = craft("unc2.resora", "linked", {linkedRec("img-1", p("nr/ok.png"), QString(), d)});
        QVERIFY(!f.isEmpty());
        QVERIFY(openResora(f, opts()).ok());
        for (const QString &unc : {QStringLiteral("\\\\resora-no-such-host\\share\\x.png"), QStringLiteral("//resora-no-such-host/share/x.png"),
                                   QStringLiteral("relative/x.png")}) {
            const auto r = relinkAndOpen(f, opts(), {{"img-1", unc}});
            QCOMPARE(r.error, OpenError::LinksBlocked);
            QCOMPARE(r.links[0].state, LinkState::UnsafePath);
            QVERIFY(r.links[0].viaRelink);
            QCOMPARE(r.links[0].resolvedPath, unc);  // the user's replacement, never the old valid path
        }
    }

    void networkBaseFoldersRejected()
    {
        const QByteArray d = pattern(350, 8);
        QVERIFY(put(p("nb/inside/x.png"), d));
        const QString unc = QStringLiteral("//resora-no-such-host/share/inside");
        const QString f = craft("unc3.resora", "linked", {linkedRec("img-1", p("nb/nowhere/x.png"), "x.png", d)}, folderBase(unc));
        QVERIFY(!f.isEmpty());
        auto r = openResora(f, opts());  // hint_path is network, no imageRoot
        QCOMPARE(r.error, OpenError::LinksBlocked);
        QCOMPARE(r.links[0].state, LinkState::UnsafePath);
        QVERIFY(r.links[0].message.contains("network base folder rejected"));
        auto o = opts();  // a network imageRoot is rejected too
        o.imageRoot = unc;
        r = openResora(f, o);
        QCOMPARE(r.error, OpenError::LinksBlocked);
        QCOMPARE(r.links[0].state, LinkState::UnsafePath);
        o.imageRoot = p("nb/inside");  // control: a local base still resolves
        r = openResora(f, o);
        QVERIFY2(r.ok(), qPrintable(r.message));
        QVERIFY(r.links[0].viaRelative);
    }

    void traversalAndCollisionsInIndexRejected()
    {
        const QByteArray d = pattern(300, 4);
        QCOMPARE(openResora(craft("t1.resora", "bundled", {bundledRec("a", "../evil", d)}), opts()).error, OpenError::BadManifest);
        QCOMPARE(openResora(craft("t2.resora", "bundled", {bundledRec("a", "C:\\evil", d)}), opts()).error, OpenError::BadManifest);
        QCOMPARE(openResora(craft("t3.resora", "bundled", {bundledRec("a", "image-0001", d), bundledRec("b", "image-0001", d)}, {},
                                  {{"image-0001", d}}), opts()).error, OpenError::BadManifest);
        QCOMPARE(openResora(craft("t4.resora", "bundled", {bundledRec("a", "image-0001", d), bundledRec("a", "image-0002", d)}, {},
                                  {{"image-0001", d}, {"image-0002", d}}), opts()).error, OpenError::BadManifest);
        QCOMPARE(openResora(craft("t5.resora", "bundled", {bundledRec("a", "image-0001", d)}), opts()).error, OpenError::BadManifest);
        QCOMPARE(openResora(craft("t6.resora", "bundled", {bundledRec("a", "image-0001", pattern(300, 5))}, {}, {{"image-0001", d}}),
                            opts()).error, OpenError::BadManifest);
        QCOMPARE(openResora(craft("t7.resora", "bundled", {bundledRec("a", "image-0001", d)}, {{"image_count", 5}}, {{"image-0001", d}}),
                            opts()).error, OpenError::BadManifest);
        QCOMPARE(openResora(craft("t8.resora", "weird", {}), opts()).error, OpenError::BadManifest);
        QCOMPARE(openResora(craft("t9.resora", "linked", {bundledRec("a", "image-0001", d)}, {}, {{"image-0001", d}}), opts()).error,
                 OpenError::BadManifest);
        QVERIFY(!QFileInfo::exists(p("cache")) || QDir(p("cache")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
        auto ok = openResora(craft("tok.resora", "bundled", {bundledRec("a", "image-0001", d)}, {}, {{"image-0001", d}}), opts("ctok"));
        QVERIFY2(ok.ok(), qPrintable(ok.message));  // proves the helper itself builds a valid file
        QVERIFY(closeDocument(&ok.document));
    }

    void missingRequiredResource()
    {
        Writer w;
        w.addBytes("manifest", TypeJson, "{}");
        QVERIFY(w.write(p("mr.resora")).ok());
        QCOMPARE(openResora(p("mr.resora"), opts()).error, OpenError::BadManifest);
    }

    void corruptAndTruncatedInput()
    {
        const QStringList imgs = makeImages(2, "k1");
        QVERIFY(exportB(imgs, p("k1.resora")));
        const QByteArray whole = slurp(p("k1.resora"));
        QVERIFY(put(p("trunc.resora"), whole.left(whole.size() / 2)));
        QCOMPARE(openResora(p("trunc.resora"), opts("ck")).error, OpenError::Container);
        QByteArray flip = whole;
        flip[200] = char(flip[200] ^ 0x40);  // inside a bundled image payload
        QVERIFY(put(p("flip.resora"), flip));
        const auto f = openResora(p("flip.resora"), opts("ck"));
        QCOMPARE(f.error, OpenError::Container);
        QCOMPARE(f.containerError, Error::ChecksumMismatch);
        QVERIFY(put(p("junk.resora"), pattern(1000, 3)));
        QCOMPARE(openResora(p("junk.resora"), opts("ck")).error, OpenError::Container);
        QVERIFY(put(p("empty.resora"), QByteArray()));
        QCOMPARE(openResora(p("empty.resora"), opts("ck")).error, OpenError::Container);
        QCOMPARE(openResora(p("does-not-exist.resora"), opts("ck")).error, OpenError::InvalidInput);
        QVERIFY(!QFileInfo::exists(p("ck")) || QDir(p("ck")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    }

    void cancellationCleansUpAndKeepsUserFiles()
    {
        const QStringList imgs = makeImages(3, "x1");
        QVERIFY(exportB(imgs, p("x1.resora")));
        QVERIFY(put(p("cx/user-file.txt"), "mine"));
        QVERIFY(put(p("cx/doc-lookalike/keep.txt"), "mine too"));  // folder shaped like ours, no marker
        for (int cancelAt : {0, 1, 3}) {
            int calls = 0;
            const auto r = openResora(p("x1.resora"), opts("cx"), [&](const ImportProgress &pr) {
                return !(pr.stage == ImportProgress::Extracting && calls++ == cancelAt);
            });
            QCOMPARE(r.error, OpenError::Cancelled);
            QCOMPARE(QDir(p("cx")).entryList(QDir::Dirs | QDir::NoDotAndDotDot), QStringList{"doc-lookalike"});
        }
        QCOMPARE(slurp(p("cx/user-file.txt")), QByteArray("mine"));
        QCOMPARE(slurp(p("cx/doc-lookalike/keep.txt")), QByteArray("mine too"));
        OpenedDocument fake;  // closeDocument refuses an unmarked folder
        fake.cacheDir = p("cx/doc-lookalike");
        QVERIFY(!closeDocument(&fake));
        QCOMPARE(slurp(p("cx/doc-lookalike/keep.txt")), QByteArray("mine too"));
        QVERIFY(exportL(makeImages(2, "x2"), p("x2.resora")));
        const auto lr = openResora(p("x2.resora"), opts(), [](const ImportProgress &pr) { return pr.stage != ImportProgress::Validating; });
        QCOMPARE(lr.error, OpenError::Cancelled);
        QCOMPARE(slurp(imgs[0]), pattern(3000, 1));  // originals untouched
    }

    void cancelDuringSingleLargeImageStopsBetweenChunks()
    {
        const QString big = p("big/huge.png");
        QVERIFY(put(big, pattern(3 * 1024 * 1024 + 123, 7)));  // > 3 chunks of 1 MiB
        QVERIFY(exportB(QStringList{big}, p("big.resora")));
        QVERIFY(put(p("cb/user-file.txt"), "mine"));
        QVERIFY(put(p("cb/doc-lookalike/keep.txt"), "mine too"));
        int calls = 0, partSeenAtCancel = 0, maxPermille = 0;
        bool monotonic = true;
        int last = -1;
        const auto r = openResora(p("big.resora"), opts("cb"), [&](const ImportProgress &pr) {
            monotonic = monotonic && pr.permille >= last;
            last = pr.permille;
            maxPermille = qMax(maxPermille, pr.permille);
            if (pr.stage != ImportProgress::Extracting)
                return true;
            if (calls++ < 2)  // 0: start; 1: first chunk
                return true;
            // Second chunk: the image is mid-extraction, so its .part must exist right now.
            const auto dirs = QDir(p("cb")).entryList({"doc-*"}, QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &d : dirs)
                partSeenAtCancel += QDir(p("cb/" + d)).entryList({"*.part"}, QDir::Files).size();
            return false;
        });
        QCOMPARE(r.error, OpenError::Cancelled);
        QCOMPARE(partSeenAtCancel, 1);
        QCOMPARE(calls, 3);  // stopped at the 2nd chunk, not after the whole image
        QVERIFY(monotonic);
        QVERIFY(maxPermille < 1000);
        QCOMPARE(QDir(p("cb")).entryList(QDir::Dirs | QDir::NoDotAndDotDot), QStringList{"doc-lookalike"});
        QCOMPARE(slurp(p("cb/user-file.txt")), QByteArray("mine"));
        QCOMPARE(slurp(p("cb/doc-lookalike/keep.txt")), QByteArray("mine too"));
        QCOMPARE(slurp(big), pattern(3 * 1024 * 1024 + 123, 7));
    }

    void progressMonotonicAndFinishesAtDone()
    {
        QVERIFY(exportB(makeImages(2, "pg"), p("pg.resora")));
        int last = -1;
        bool doneSeen = false, monotonic = true, early = false;
        auto r = openResora(p("pg.resora"), opts("cpg"), [&](const ImportProgress &pr) {
            monotonic = monotonic && pr.permille >= last;
            last = pr.permille;
            if (pr.stage == ImportProgress::Done)
                doneSeen = pr.permille == 1000;
            else if (pr.permille >= 1000)
                early = true;
            return true;
        });
        QVERIFY(r.ok());
        QVERIFY(monotonic && doneSeen && !early);
        closeDocument(&r.document);
    }
};

QTEST_APPLESS_MAIN(TstImport)
#include "tst_resoraimport.moc"
