#include "resora/resorareader.h"

#include <QCryptographicHash>
#include <QSet>
#include <algorithm>

namespace resora {

namespace {
constexpr qint64 kChunk = 1 << 20;

Result bad(Error e, const char *m) { return Result::fail(e, QString::fromLatin1(m)); }

// Reads exactly n bytes at offset; false on short read.
bool readAt(QFile &f, quint64 offset, qint64 n, QByteArray *out)
{
    if (!f.seek(qint64(offset)))
        return false;
    *out = f.read(n);
    return out->size() == n;
}
}  // namespace

void Reader::close()
{
    file_.close();
    entries_.clear();
    major_ = minor_ = 0;
    flags_ = 0;
}

Result Reader::open(const QString &path)
{
    close();
    file_.setFileName(path);
    if (!file_.open(QIODevice::ReadOnly))
        return Result::fail(Error::IoError, QStringLiteral("Cannot open %1: %2").arg(path, file_.errorString()));
    const quint64 size = quint64(file_.size());

    Result r = parseHeader(size);
    quint64 dirOffset = 0, dirLength = 0;
    quint32 count = 0;
    if (r.ok())
        r = parseTrailer(size, &dirOffset, &dirLength, &count);
    if (r.ok())
        r = parseDirectory(dirOffset, dirLength, count);
    if (!r.ok())
        close();
    return r;
}

Result Reader::parseHeader(quint64 size)
{
    QByteArray head;
    if (size < 8 || !readAt(file_, 0, 8, &head))
        return bad(Error::Truncated, "File is too small to be a .resora container");
    if (head != magic())
        return bad(Error::BadMagic, "Not a .resora file (bad magic signature)");
    if (size < quint64(kHeaderSize + kDirHeaderSize + kTrailerSize) || !readAt(file_, 0, kHeaderSize, &head))
        return bad(Error::Truncated, "File is truncated (shorter than header and trailer)");
    major_ = getU16(head, 8);
    minor_ = getU16(head, 10);
    flags_ = getU32(head, 12);
    if (major_ != kVersionMajor)
        return Result::fail(Error::UnsupportedVersion,
                            QStringLiteral("Unsupported .resora format version %1.%2 (this build reads %3.x)")
                                .arg(major_).arg(minor_).arg(kVersionMajor));
    if (getU32(head, 16) != quint32(kHeaderSize))
        return bad(Error::BadHeader, "Unexpected header size");
    if (QCryptographicHash::hash(head.left(32), QCryptographicHash::Sha256) != head.mid(32, kShaSize))
        return bad(Error::BadHeader, "Header checksum mismatch");
    return {};
}

Result Reader::parseTrailer(quint64 size, quint64 *dirOffset, quint64 *dirLength, quint32 *count)
{
    QByteArray tr;
    if (!readAt(file_, size - kTrailerSize, kTrailerSize, &tr))
        return bad(Error::IoError, "Cannot read trailer");
    if (tr.right(8) != endMagic())
        return bad(Error::Truncated, "End marker missing: file is truncated or corrupted");
    *dirOffset = getU64(tr, 0);
    *dirLength = getU64(tr, 8);
    *count = getU32(tr, 16);
    if (*count > kMaxResources)
        return bad(Error::TooManyResources, "Resource count exceeds the supported maximum");
    const quint64 trailerStart = size - kTrailerSize;
    if (*dirLength != quint64(kDirHeaderSize) + quint64(*count) * kEntrySize)
        return bad(Error::BadTrailer, "Directory length does not match resource count");
    if (!rangeWithin(*dirOffset, *dirLength, kHeaderSize, trailerStart) || *dirOffset + *dirLength != trailerStart)
        return bad(Error::BadTrailer, "Directory location is out of range");

    const Result h = verifyFileHash(size - 40, tr.mid(56, kShaSize));
    if (!h.ok())
        return h;
    QByteArray dir;
    if (!readAt(file_, *dirOffset, qint64(*dirLength), &dir))
        return bad(Error::IoError, "Cannot read directory");
    if (QCryptographicHash::hash(dir, QCryptographicHash::Sha256) != tr.mid(24, kShaSize))
        return bad(Error::ChecksumMismatch, "Directory checksum mismatch");
    return {};
}

Result Reader::verifyFileHash(quint64 hashedLength, const QByteArray &expected)
{
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!file_.seek(0))
        return bad(Error::IoError, "Seek failed");
    quint64 left = hashedLength;
    QByteArray buf(int(kChunk), '\0');
    while (left > 0) {
        const qint64 n = file_.read(buf.data(), qint64(std::min<quint64>(left, kChunk)));
        if (n <= 0)
            return bad(Error::IoError, "Read failed while verifying file");
        h.addData(QByteArrayView(buf.constData(), n));
        left -= quint64(n);
    }
    if (h.result() != expected)
        return bad(Error::ChecksumMismatch, "File checksum mismatch (file is corrupted)");
    return {};
}

Result Reader::parseDirectory(quint64 dirOffset, quint64 dirLength, quint32 count)
{
    QByteArray dir;
    if (!readAt(file_, dirOffset, qint64(dirLength), &dir))  // bounded: count <= 65536 entries
        return bad(Error::IoError, "Cannot read directory");
    if (getU32(dir, 0) != count)
        return bad(Error::BadDirectory, "Directory count disagrees with trailer");

    QList<ResourceInfo> list;
    list.reserve(int(count));
    for (quint32 i = 0; i < count; ++i) {
        const int o = kDirHeaderSize + int(i) * kEntrySize;
        const QByteArray field = dir.mid(o, kIdFieldSize);
        const int nul = field.indexOf('\0');
        const int len = nul < 0 ? kIdFieldSize : nul;
        if (nul < 0 || field.mid(len).count('\0') != kIdFieldSize - len)
            return bad(Error::InvalidResourceId, "Resource id is not NUL-terminated/padded");
        ResourceInfo e;
        e.id = QString::fromLatin1(field.left(len));
        if (!isValidResourceId(e.id))
            return Result::fail(Error::InvalidResourceId, QStringLiteral("Invalid resource id in directory: '%1'").arg(e.id));
        e.type = getU32(dir, o + 64);
        e.compression = getU32(dir, o + 68);
        e.offset = getU64(dir, o + 72);
        e.storedLength = getU64(dir, o + 80);
        e.originalLength = getU64(dir, o + 88);
        e.sha256 = dir.mid(o + 96, kShaSize);
        if (e.storedLength > kMaxResourceSize || e.originalLength > kMaxResourceSize)
            return Result::fail(Error::ResourceTooLarge, QStringLiteral("Resource '%1' exceeds the size limit").arg(e.id));
        if (!rangeWithin(e.offset, e.storedLength, kHeaderSize, dirOffset))
            return Result::fail(Error::ResourceOutOfRange, QStringLiteral("Resource '%1' lies outside the payload area").arg(e.id));
        if (e.compression == CompressionNone && e.storedLength != e.originalLength)
            return Result::fail(Error::BadDirectory, QStringLiteral("Resource '%1': lengths disagree").arg(e.id));
        list.append(e);
    }

    QList<ResourceInfo> byOffset = list;
    std::sort(byOffset.begin(), byOffset.end(), [](const ResourceInfo &a, const ResourceInfo &b) {
        return a.offset < b.offset;
    });
    for (int i = 0; i < byOffset.size(); ++i) {
        if (i > 0 && byOffset[i - 1].offset + byOffset[i - 1].storedLength > byOffset[i].offset)
            return Result::fail(Error::ResourceOutOfRange, QStringLiteral("Resource '%1' overlaps another").arg(byOffset[i].id));
    }
    QSet<QString> seen;  // hashed: a 65536-entry directory must not cost O(n^2) comparisons
    seen.reserve(list.size());
    for (const ResourceInfo &e : list) {
        if (seen.contains(e.id))
            return Result::fail(Error::DuplicateResourceId, QStringLiteral("Duplicate resource id: '%1'").arg(e.id));
        seen.insert(e.id);
    }
    entries_ = list;
    return {};
}

bool Reader::contains(const QString &id) const { return !info(id).id.isEmpty(); }

ResourceInfo Reader::info(const QString &id) const
{
    for (const ResourceInfo &e : entries_)
        if (e.id == id)
            return e;
    return {};
}

Result Reader::streamResource(const ResourceInfo &e, QIODevice *out, QByteArray *mem,
                              const std::function<bool(quint64)> &onChunk)
{
    if (e.compression != CompressionNone)
        return Result::fail(Error::UnsupportedCompression, QStringLiteral("Resource '%1' uses unsupported compression %2").arg(e.id).arg(e.compression));
    if (!file_.isOpen() || !file_.seek(qint64(e.offset)))
        return bad(Error::IoError, "Cannot seek to resource");
    QCryptographicHash h(QCryptographicHash::Sha256);
    QByteArray buf(int(kChunk), '\0');
    quint64 left = e.storedLength;
    while (left > 0) {
        const qint64 want = qint64(std::min<quint64>(left, kChunk));
        const qint64 n = file_.read(buf.data(), want);
        if (n <= 0)
            return bad(Error::Truncated, "Resource data is truncated");
        h.addData(QByteArrayView(buf.constData(), n));
        if (mem)
            mem->append(buf.constData(), n);
        else if (out->write(buf.constData(), n) != n)
            return bad(Error::WriteFailed, "Cannot write resource to destination");
        left -= quint64(n);
        if (onChunk && !onChunk(e.storedLength - left))
            return Result::fail(Error::Cancelled, QStringLiteral("Resource copy cancelled"));
    }
    if (h.result() != e.sha256)
        return Result::fail(Error::ChecksumMismatch, QStringLiteral("Resource '%1' checksum mismatch").arg(e.id));
    return {};
}

Result Reader::readResource(const QString &id, QByteArray *out, quint64 maxBytes)
{
    const ResourceInfo e = info(id);
    if (e.id.isEmpty())
        return Result::fail(Error::ResourceNotFound, QStringLiteral("No such resource: '%1'").arg(id));
    if (e.storedLength > maxBytes)
        return Result::fail(Error::ResourceTooLarge, QStringLiteral("Resource '%1' is larger than the in-memory limit").arg(id));
    out->clear();
    out->reserve(qsizetype(e.storedLength));
    const Result r = streamResource(e, nullptr, out);
    if (!r.ok())
        out->clear();
    return r;
}

Result Reader::copyResource(const QString &id, QIODevice *dest, const std::function<bool(quint64)> &onChunk)
{
    const ResourceInfo e = info(id);
    if (e.id.isEmpty())
        return Result::fail(Error::ResourceNotFound, QStringLiteral("No such resource: '%1'").arg(id));
    if (!dest)
        return bad(Error::InvalidArgument, "No destination device");
    return streamResource(e, dest, nullptr, onChunk);
}

}  // namespace resora
