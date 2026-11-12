#include "resora/resorawriter.h"

#include "resora/resorareader.h"  // ResourceInfo

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace resora {

namespace {
constexpr qint64 kChunk = 1 << 20;

// Writes to the file and feeds both hashers; false on a short write.
bool putAndHash(QSaveFile &f, QCryptographicHash &whole, const char *p, qint64 n)
{
    whole.addData(QByteArrayView(p, n));
    return f.write(p, n) == n;
}
}  // namespace

Result Writer::checkId(const QString &id) const
{
    if (!isValidResourceId(id))
        return Result::fail(Error::InvalidResourceId, QStringLiteral("Invalid resource id: '%1'").arg(id));
    if (ids_.contains(id))
        return Result::fail(Error::DuplicateResourceId, QStringLiteral("Duplicate resource id: '%1'").arg(id));
    if (items_.size() >= int(kMaxResources))
        return Result::fail(Error::TooManyResources, QStringLiteral("Too many resources"));
    return {};
}

Result Writer::addBytes(const QString &id, quint32 type, const QByteArray &data)
{
    const Result r = checkId(id);
    if (!r.ok())
        return r;
    ids_.insert(id);
    items_.append({id, type, data, QString()});
    return {};
}

Result Writer::addFile(const QString &id, quint32 type, const QString &sourcePath)
{
    if (sourcePath.isEmpty())
        return Result::fail(Error::InvalidArgument, QStringLiteral("Empty source path"));
    const Result r = checkId(id);
    if (!r.ok())
        return r;
    ids_.insert(id);
    items_.append({id, type, QByteArray(), sourcePath});
    return {};
}

bool Writer::budgetOk(quint64 n, quint64 &written) const
{
    if (limit_ != 0 && written + n > limit_)
        return false;
    written += n;
    return true;
}

quint64 Writer::containerBytes() const
{
    quint64 payload = 0;
    for (const Item &it : items_)
        payload += it.sourcePath.isEmpty() ? quint64(it.bytes.size())
                                           : quint64(qMax<qint64>(0, QFileInfo(it.sourcePath).size()));
    return quint64(kHeaderSize) + payload + quint64(kDirHeaderSize) + quint64(items_.size()) * kEntrySize
           + quint64(kTrailerSize);
}

Result Writer::write(const QString &targetPath, const ProgressFn &progress) const
{
    if (targetPath.isEmpty())
        return Result::fail(Error::InvalidArgument, QStringLiteral("Empty target path"));
    quint64 total = 0, done = 0;
    for (const Item &it : items_)
        total += it.sourcePath.isEmpty() ? quint64(it.bytes.size()) : quint64(qMax<qint64>(0, QFileInfo(it.sourcePath).size()));
    const auto cancelled = [&]() { return progress && !progress(done, total); };
    QSaveFile out(targetPath);  // atomic: temp file in the same folder, renamed on commit()
    if (!out.open(QIODevice::WriteOnly))
        return Result::fail(Error::IoError, QStringLiteral("Cannot create %1: %2").arg(targetPath, out.errorString()));

    QCryptographicHash whole(QCryptographicHash::Sha256);
    quint64 written = 0;  // bytes handed to the device so far (checked against limit_)
    const auto ioFail = [&](const QString &why) {
        out.cancelWriting();
        return Result::fail(Error::WriteFailed, why);
    };
    const auto full = [&]() {
        out.cancelWriting();
        return Result::fail(Error::OutOfSpace, QStringLiteral("Not enough space on the destination drive"));
    };

    // Header.
    QByteArray head = magic();
    putU16(head, kVersionMajor);
    putU16(head, kVersionMinor);
    putU32(head, flags_);
    putU32(head, kHeaderSize);
    head.append(QByteArray(12, '\0'));
    head.append(QCryptographicHash::hash(head, QCryptographicHash::Sha256));
    if (!budgetOk(quint64(head.size()), written))
        return full();
    if (!putAndHash(out, whole, head.constData(), head.size()))
        return ioFail(QStringLiteral("Write failed: ") + out.errorString());

    // Payloads, streamed; each hashed as it is written.
    QList<ResourceInfo> entries;
    quint64 pos = kHeaderSize;
    for (const Item &it : items_) {
        ResourceInfo e;
        e.id = it.id;
        e.type = it.type;
        e.offset = pos;
        QCryptographicHash h(QCryptographicHash::Sha256);
        quint64 len = 0;
        if (it.sourcePath.isEmpty()) {
            h.addData(it.bytes);
            len = quint64(it.bytes.size());
            if (!budgetOk(len, written))
                return full();
            if (!putAndHash(out, whole, it.bytes.constData(), it.bytes.size()))
                return ioFail(QStringLiteral("Write failed: ") + out.errorString());
        } else {
            QFile src(it.sourcePath);
            if (!src.open(QIODevice::ReadOnly)) {
                out.cancelWriting();
                return Result::fail(Error::IoError, QStringLiteral("Cannot read %1: %2").arg(it.sourcePath, src.errorString()));
            }
            QByteArray buf(int(kChunk), '\0');
            for (;;) {
                const qint64 n = src.read(buf.data(), kChunk);
                if (n < 0) {
                    out.cancelWriting();
                    return Result::fail(Error::IoError, QStringLiteral("Read failed: ") + it.sourcePath);
                }
                if (n == 0)
                    break;
                h.addData(QByteArrayView(buf.constData(), n));
                len += quint64(n);
                done += quint64(n);
                if (cancelled()) {
                    out.cancelWriting();
                    return Result::fail(Error::Cancelled, QStringLiteral("Cancelled"));
                }
                if (len > kMaxResourceSize) {
                    out.cancelWriting();
                    return Result::fail(Error::ResourceTooLarge, QStringLiteral("Resource too large: ") + it.id);
                }
                if (!budgetOk(quint64(n), written))
                    return full();
                if (!putAndHash(out, whole, buf.constData(), n))
                    return ioFail(QStringLiteral("Write failed: ") + out.errorString());
            }
        }
        if (it.sourcePath.isEmpty()) {
            done += len;
            if (cancelled()) {
                out.cancelWriting();
                return Result::fail(Error::Cancelled, QStringLiteral("Cancelled"));
            }
        }
        e.storedLength = e.originalLength = len;
        e.sha256 = h.result();
        pos += len;
        entries.append(e);
    }

    // Directory.
    QByteArray dir;
    putU32(dir, quint32(entries.size()));
    putU32(dir, 0);
    for (const ResourceInfo &e : entries) {
        QByteArray id = e.id.toLatin1();  // ids are validated ASCII
        id.append(QByteArray(kIdFieldSize - id.size(), '\0'));
        dir.append(id);
        putU32(dir, e.type);
        putU32(dir, CompressionNone);
        putU64(dir, e.offset);
        putU64(dir, e.storedLength);
        putU64(dir, e.originalLength);
        dir.append(e.sha256);
    }
    const quint64 dirOffset = pos;
    if (!budgetOk(quint64(dir.size()), written))
        return full();
    if (!putAndHash(out, whole, dir.constData(), dir.size()))
        return ioFail(QStringLiteral("Write failed: ") + out.errorString());

    // Trailer: the file hash covers every byte before the file-hash field itself (bytes [0, size-40)),
    // so there is no circular dependency; the field and the end magic are excluded by construction.
    QByteArray tr;
    putU64(tr, dirOffset);
    putU64(tr, quint64(dir.size()));
    putU32(tr, quint32(entries.size()));
    putU32(tr, 0);
    tr.append(QCryptographicHash::hash(dir, QCryptographicHash::Sha256));
    whole.addData(tr);
    tr.append(whole.result());
    tr.append(endMagic());
    if (!budgetOk(quint64(tr.size()), written))
        return full();
    if (out.write(tr) != tr.size())
        return ioFail(QStringLiteral("Write failed: ") + out.errorString());

    // Pre-commit integrity check, using only public QSaveFile API: the structure we serialised must
    // be self-consistent and the device must hold exactly the bytes we produced. (Payload bytes are
    // not re-read; their hashes were computed from the very buffers handed to write().)
    const quint64 expectedSize = dirOffset + quint64(dir.size()) + quint64(tr.size());
    if (tr.size() != kTrailerSize || quint64(dir.size()) != quint64(kDirHeaderSize) + quint64(entries.size()) * kEntrySize)
        return ioFail(QStringLiteral("Internal error: inconsistent container layout"));
    if (!out.flush())
        return ioFail(QStringLiteral("Flush failed: ") + out.errorString());
    if (quint64(out.size()) != expectedSize || quint64(out.pos()) != expectedSize)
        return ioFail(QStringLiteral("Output size mismatch: expected %1 bytes, device holds %2").arg(expectedSize).arg(out.size()));

    if (!out.commit())
        return Result::fail(Error::WriteFailed, QStringLiteral("Cannot finalise %1: %2").arg(targetPath, out.errorString()));
    return {};
}

}  // namespace resora
