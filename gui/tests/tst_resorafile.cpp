// .resora container: creation, writing, reading, and invalid-file rejection. Qt Core only.
#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "resora/resorareader.h"
#include "resora/resorawriter.h"

using namespace resora;

namespace {
QByteArray sha(const QByteArray &b) { return QCryptographicHash::hash(b, QCryptographicHash::Sha256); }

QByteArray slurp(const QString &p) { QFile f(p); f.open(QIODevice::ReadOnly); return f.readAll(); }
void spit(const QString &p, const QByteArray &b) { QFile f(p); f.open(QIODevice::WriteOnly); f.write(b); }

struct RawEntry { QByteArray id; quint32 comp = 0; quint64 offset = 0, stored = 0, orig = 0; QByteArray sha; };

// Builds a container with *valid* checksums around arbitrary directory entries, so structural
// validation (not checksums) is what rejects it.
QByteArray forge(const QByteArray &payload, const QList<RawEntry> &entries, quint16 major = 1, quint32 declaredCount = 0xFFFFFFFF, quint16 minor = 0)
{
    QByteArray head = magic();
    putU16(head, major); putU16(head, minor); putU32(head, 0); putU32(head, kHeaderSize);
    head.append(QByteArray(12, '\0'));
    head.append(sha(head));
    QByteArray dir;
    putU32(dir, quint32(entries.size())); putU32(dir, 0);
    for (const RawEntry &e : entries) {
        dir.append(e.id.leftJustified(kIdFieldSize, '\0'));
        putU32(dir, 0); putU32(dir, e.comp);
        putU64(dir, e.offset); putU64(dir, e.stored); putU64(dir, e.orig);
        dir.append(e.sha.leftJustified(kShaSize, '\0'));
    }
    QByteArray body = head + payload;
    const quint64 dirOffset = body.size();
    body += dir;
    QByteArray tr;
    putU64(tr, dirOffset); putU64(tr, quint64(dir.size()));
    putU32(tr, declaredCount == 0xFFFFFFFF ? quint32(entries.size()) : declaredCount); putU32(tr, 0);
    tr.append(sha(dir));
    QByteArray whole = body + tr;
    tr.append(sha(whole));
    tr.append(endMagic());
    return body + tr;
}
}  // namespace

class TstResoraFile : public QObject
{
    Q_OBJECT
    QTemporaryDir dir_;
    QString p(const char *n) const { return dir_.filePath(QString::fromLatin1(n)); }

    Error openErr(const QByteArray &bytes)
    {
        spit(p("x.resora"), bytes);
        Reader r;
        return r.open(p("x.resora")).error;
    }
    QByteArray goodFile()
    {
        Writer w;
        w.addBytes("manifest", TypeJson, "{\"a\":1}");
        w.addBytes("thumb-0001", TypeThumbnail, QByteArray(1000, 'x'));
        w.write(p("g.resora"));
        return slurp(p("g.resora"));
    }

private slots:
    void initTestCase() { QVERIFY(dir_.isValid()); }

    void idValidation()
    {
        QVERIFY(isValidResourceId("layout"));
        QVERIFY(isValidResourceId("image-0001"));
        for (const char *bad : {"", "..", "a/b", "a\\b", "C:x", "Up", "-x", "a.b", "has space"})
            QVERIFY2(!isValidResourceId(QString::fromLatin1(bad)), bad);
        QVERIFY(!isValidResourceId(QString(64, 'a')));
        QVERIFY(isValidResourceId(QString(63, 'a')));
    }

    void roundTrip()
    {
        const QByteArray big(3 * 1024 * 1024 + 7, 'q');
        spit(p("src.bin"), big);
        Writer w(5);
        QVERIFY(w.addBytes("manifest", TypeJson, "{}").ok());
        QVERIFY(w.addBytes("empty", TypeOpaque, QByteArray()).ok());
        QVERIFY(w.addFile("image-0001", TypeImage, p("src.bin")).ok());
        QVERIFY(w.addBytes("future", 4242, "zzz").ok());
        QVERIFY(w.write(p("rt.resora")).ok());

        Reader r;
        QVERIFY(r.open(p("rt.resora")).ok());
        QCOMPARE(r.versionMajor(), kVersionMajor);
        QCOMPARE(r.flags(), 5u);
        QCOMPARE(r.resources().size(), 4);
        QCOMPARE(r.info("future").type, 4242u);  // unknown type preserved
        QByteArray out;
        QVERIFY(r.readResource("manifest", &out).ok());
        QCOMPARE(out, QByteArray("{}"));
        QVERIFY(r.readResource("empty", &out).ok());
        QVERIFY(out.isEmpty());
        QVERIFY(r.readResource("image-0001", &out).ok());
        QCOMPARE(out, big);
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QVERIFY(r.copyResource("image-0001", &buf).ok());
        QCOMPARE(buf.data(), big);
        QCOMPARE(r.readResource("nope", &out).error, Error::ResourceNotFound);
        QCOMPARE(r.readResource("image-0001", &out, 100).error, Error::ResourceTooLarge);
    }

    void deterministic()
    {
        const QByteArray a = goodFile();
        QCOMPARE(goodFile(), a);
    }

    void writerRejectsBadIds()
    {
        Writer w;
        QCOMPARE(w.addBytes("../evil", 0, "x").error, Error::InvalidResourceId);
        QVERIFY(w.addBytes("ok", 0, "x").ok());
        QCOMPARE(w.addBytes("ok", 0, "y").error, Error::DuplicateResourceId);
        QCOMPARE(w.count(), 1);
    }

    void emptyContainer()
    {
        Writer w;
        QVERIFY(w.write(p("e.resora")).ok());
        Reader r;
        QVERIFY(r.open(p("e.resora")).ok());
        QCOMPARE(r.resources().size(), 0);
    }

    void atomicWriteLeavesNoPartial()
    {
        // Existing target survives a failed write (missing source file), and no temp file remains.
        Writer ok;
        ok.addBytes("a", 0, "keep");
        QVERIFY(ok.write(p("at.resora")).ok());
        const QByteArray before = slurp(p("at.resora"));
        Writer bad;
        bad.addFile("a", 0, p("does-not-exist.bin"));
        QCOMPARE(bad.write(p("at.resora")).error, Error::IoError);
        QCOMPARE(slurp(p("at.resora")), before);
        Writer fresh;
        fresh.addFile("a", 0, p("does-not-exist.bin"));
        QVERIFY(!fresh.write(p("never.resora")).ok());
        QVERIFY(!QFile::exists(p("never.resora")));
        const QStringList left = QDir(dir_.path()).entryList({"never*", "at.resora*"}, QDir::Files);
        QCOMPARE(left, QStringList{"at.resora"});
    }

    void badMagic()
    {
        QByteArray b = goodFile();
        b[0] = 'X';
        QCOMPARE(openErr(b), Error::BadMagic);
        QCOMPARE(openErr("PK\x03\x04 not ours"), Error::BadMagic);
    }

    void tooSmallAndMissing()
    {
        QCOMPARE(openErr(QByteArray()), Error::Truncated);
        QCOMPARE(openErr(magic()), Error::Truncated);
        Reader r;
        QCOMPARE(r.open(p("missing.resora")).error, Error::IoError);
    }

    void minorVersionAndFlagsPolicy()  // Phase 7: additive minors readable, flags carried but never interpreted
    {
        QCOMPARE(openErr(forge({}, {}, 1, 0xFFFFFFFF, 7)), Error::None);  // newer minor of the same major is read
        QCOMPARE(openErr(forge({}, {}, 0)), Error::UnsupportedVersion);   // major 0 never existed
        QCOMPARE(openErr(forge({}, {}, 65535)), Error::UnsupportedVersion);
        Writer w(0x80000001u);
        w.addBytes("a", TypeJson, "{}");
        QVERIFY(w.write(p("fl.resora")).ok());
        Reader r;
        QVERIFY(r.open(p("fl.resora")).ok());
        QCOMPARE(r.flags(), 0x80000001u);
        QCOMPARE(r.versionMinor(), quint16(kVersionMinor));
    }

    void unknownResourceTypesAreCarried()
    {
        Writer w;
        w.addBytes("future", 0xABCD, "zz");
        QVERIFY(w.write(p("ut.resora")).ok());
        Reader r;
        QVERIFY(r.open(p("ut.resora")).ok());
        QByteArray b;
        QVERIFY(r.readResource("future", &b).ok());
        QCOMPARE(r.info("future").type, quint32(0xABCD));
    }

    void manyResourcesStress()  // 20000 resources: hashed duplicate checks keep this fast; every one verifies
    {
        QElapsedTimer t;
        t.start();
        Writer w;
        for (int i = 0; i < 20000; ++i)
            QVERIFY(w.addBytes(QStringLiteral("r%1").arg(i), TypeOpaque, QByteArray::number(i)).ok());
        QCOMPARE(w.addBytes("r5", TypeOpaque, "x").error, Error::DuplicateResourceId);
        QVERIFY(w.write(p("many.resora")).ok());
        Reader r;
        QVERIFY(r.open(p("many.resora")).ok());
        QCOMPARE(r.resources().size(), 20000);
        QByteArray b;
        QVERIFY(r.readResource("r19999", &b).ok());
        QCOMPARE(b, QByteArray("19999"));
        QVERIFY2(t.elapsed() < 20000, qPrintable(QString::number(t.elapsed())));
    }

    void hostileDirectoryEntries()  // offset+length overflow, huge declared sizes: rejected before any allocation
    {
        const QByteArray payload(16, 'p');
        RawEntry e;
        e.id = "a"; e.offset = quint64(-8); e.stored = 16; e.orig = 16;
        QCOMPARE(openErr(forge(payload, {e})), Error::ResourceOutOfRange);
        e.offset = kHeaderSize; e.stored = e.orig = quint64(-1);
        QCOMPARE(openErr(forge(payload, {e})), Error::ResourceTooLarge);
        e.stored = e.orig = kMaxResourceSize;  // within the cap, but far outside the file
        QCOMPARE(openErr(forge(payload, {e})), Error::ResourceOutOfRange);
        e.stored = 16; e.orig = 16; e.comp = CompressionZlib;
        QCOMPARE(openErr(forge(payload, {e})), Error::None);  // directory is fine; reading it is what fails
        Reader r;
        QVERIFY(r.open(p("x.resora")).ok());
        QByteArray out;
        QCOMPARE(r.readResource("a", &out).error, Error::UnsupportedCompression);
    }

    void unsupportedVersion()
    {
        QCOMPARE(openErr(forge({}, {}, 2)), Error::UnsupportedVersion);
        QByteArray b = goodFile();  // header checksum guards tampering with the version field
        b[8] = 9;
        QVERIFY(openErr(b) == Error::UnsupportedVersion);
    }

    void headerChecksum()
    {
        QByteArray b = goodFile();
        b[12] = char(b[12] ^ 1);  // flags changed
        QCOMPARE(openErr(b), Error::BadHeader);
    }

    void truncation()
    {
        const QByteArray b = goodFile();
        for (int cut : {1, 40, 96, 200, int(b.size()) - 70})
            QVERIFY2(openErr(b.left(b.size() - cut)) != Error::None, qPrintable(QString::number(cut)));
        QCOMPARE(openErr(b.left(b.size() - 1)), Error::Truncated);
        QCOMPARE(openErr(b + "junk"), Error::Truncated);  // trailing garbage hides the end marker
    }

    void flippedBytesAnywhere()
    {
        const QByteArray b = goodFile();
        for (int i = 8; i < b.size(); i += 7) {
            QByteArray c = b;
            c[i] = char(c[i] ^ 0x40);
            QVERIFY2(openErr(c) != Error::None, qPrintable(QString("byte %1").arg(i)));
        }
    }

    void payloadCorruptionDetectedByFileHash()
    {
        QByteArray b = goodFile();
        b[kHeaderSize + 2] = char(b[kHeaderSize + 2] ^ 1);
        QCOMPARE(openErr(b), Error::ChecksumMismatch);
    }

    void wholeFileHashDefinition()
    {
        // Independent recomputation: SHA-256 over bytes [0, size-40) equals trailer bytes 56..87,
        // i.e. everything before the hash field; the field and end magic are not covered.
        const QByteArray b = goodFile();
        const int hashPos = b.size() - kTrailerSize + 56;
        QCOMPARE(hashPos, b.size() - 40);
        QCOMPARE(sha(b.left(hashPos)), b.mid(hashPos, kShaSize));
        QCOMPARE(openErr(b), Error::None);
    }

    void modifiedWholeFileHashField()
    {
        const QByteArray b = goodFile();
        const int hashPos = b.size() - 40;
        for (int i = 0; i < kShaSize; ++i) {
            QByteArray c = b;
            c[hashPos + i] = char(c[hashPos + i] ^ 0x01);
            QCOMPARE(openErr(c), Error::ChecksumMismatch);
        }
        QByteArray zero = b;  // zero-filled field is not a valid digest
        zero.replace(hashPos, kShaSize, QByteArray(kShaSize, '\0'));
        QCOMPARE(openErr(zero), Error::ChecksumMismatch);
        QByteArray magicBad = b;  // end magic is validated separately
        magicBad[b.size() - 1] = char(magicBad[b.size() - 1] ^ 1);
        QCOMPARE(openErr(magicBad), Error::Truncated);
    }

    void writerOutputIsExactlyAccounted()
    {
        const QByteArray b = goodFile();  // 64 + 7 + 1000 + (8 + 2*128) + 96
        QCOMPARE(b.size(), kHeaderSize + 7 + 1000 + kDirHeaderSize + 2 * kEntrySize + kTrailerSize);
    }

    void perResourceChecksum()
    {
        // Valid whole-file hash, but the directory's recorded resource hash is wrong.
        const QByteArray payload("hello");
        RawEntry e{"a", 0, kHeaderSize, 5, 5, sha("other")};
        Reader r;
        spit(p("x.resora"), forge(payload, {e}));
        QVERIFY(r.open(p("x.resora")).ok());
        QByteArray out;
        QCOMPARE(r.readResource("a", &out).error, Error::ChecksumMismatch);
        QVERIFY(out.isEmpty());
    }

    void badOffsetsAndSizes()
    {
        const QByteArray pl("0123456789");
        const quint64 base = kHeaderSize;
        const QByteArray h = sha(pl);
        QCOMPARE(openErr(forge(pl, {{"a", 0, base + 5, 10, 10, h}})), Error::ResourceOutOfRange);  // runs into directory
        QCOMPARE(openErr(forge(pl, {{"a", 0, 3, 4, 4, h}})), Error::ResourceOutOfRange);            // inside header
        QCOMPARE(openErr(forge(pl, {{"a", 0, base, ~quint64(0), ~quint64(0), h}})), Error::ResourceTooLarge);
        QCOMPARE(openErr(forge(pl, {{"a", 0, ~quint64(0) - 2, 8, 8, h}})), Error::ResourceOutOfRange);  // overflow
        QCOMPARE(openErr(forge(pl, {{"a", 0, base, 4, 5, h}})), Error::BadDirectory);                   // len mismatch
        QCOMPARE(openErr(forge(pl, {{"a", 0, base, 6, 6, h}, {"b", 0, base + 4, 6, 6, h}})), Error::ResourceOutOfRange);  // overlap
        QVERIFY(openErr(forge(pl, {{"a", 0, base, 5, 5, h}, {"b", 0, base + 5, 5, 5, h}})) == Error::None);
    }

    void idProblemsInDirectory()
    {
        const QByteArray pl("abcd"), h = sha(pl);
        QCOMPARE(openErr(forge(pl, {{"a", 0, kHeaderSize, 2, 2, h}, {"a", 0, kHeaderSize + 2, 2, 2, h}})), Error::DuplicateResourceId);
        QCOMPARE(openErr(forge(pl, {{"", 0, kHeaderSize, 2, 2, h}})), Error::InvalidResourceId);
        QCOMPARE(openErr(forge(pl, {{"../x", 0, kHeaderSize, 2, 2, h}})), Error::InvalidResourceId);
        QCOMPARE(openErr(forge(pl, {{"UPPER", 0, kHeaderSize, 2, 2, h}})), Error::InvalidResourceId);
        QCOMPARE(openErr(forge(pl, {{QByteArray(64, 'a'), 0, kHeaderSize, 2, 2, h}})), Error::InvalidResourceId);  // no NUL
    }

    void oversizedCounts()
    {
        QCOMPARE(openErr(forge({}, {}, 1, 0x7FFFFFFF)), Error::TooManyResources);
        QCOMPARE(openErr(forge({}, {}, 1, 3)), Error::BadTrailer);  // count disagrees with directory length
    }

    void unsupportedCompressionOnlyFailsOnRead()
    {
        const QByteArray pl("abcd");
        Reader r;
        spit(p("x.resora"), forge(pl, {{"z", 1, kHeaderSize, 4, 9, sha(pl)}}));
        QVERIFY(r.open(p("x.resora")).ok());
        QByteArray out;
        QCOMPARE(r.readResource("z", &out).error, Error::UnsupportedCompression);
    }
};

QTEST_APPLESS_MAIN(TstResoraFile)
#include "tst_resorafile.moc"
