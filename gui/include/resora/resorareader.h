#pragma once
// Reads a .resora container. open() validates everything structural (magic, version, header and
// directory checksums, trailer, whole-file SHA-256, every offset/size, ids, duplicates, overlaps)
// before any payload is touched. Payload reads re-verify the per-resource SHA-256.
#include <QFile>
#include <QList>
#include <QString>

#include "resora/resorafmt.h"

namespace resora {

struct ResourceInfo
{
    QString id;
    quint32 type = TypeOpaque;
    quint32 compression = CompressionNone;
    quint64 offset = 0;
    quint64 storedLength = 0;
    quint64 originalLength = 0;
    QByteArray sha256;  // 32 bytes, of the stored bytes
};

class Reader
{
public:
    Result open(const QString &path);
    void close();

    quint16 versionMajor() const { return major_; }
    quint16 versionMinor() const { return minor_; }
    quint32 flags() const { return flags_; }
    const QList<ResourceInfo> &resources() const { return entries_; }
    bool contains(const QString &id) const;
    ResourceInfo info(const QString &id) const;  // empty id when unknown

    // Whole resource into memory; refuses (before allocating) anything larger than maxBytes.
    Result readResource(const QString &id, QByteArray *out, quint64 maxBytes = kDefaultMaxInMemory);
    // Streams a resource to `out` in chunks. On ChecksumMismatch the caller must discard what it wrote.
    Result copyResource(const QString &id, QIODevice *out);

private:
    Result parseHeader(quint64 size);
    Result parseTrailer(quint64 size, quint64 *dirOffset, quint64 *dirLength, quint32 *count);
    Result parseDirectory(quint64 dirOffset, quint64 dirLength, quint32 count);
    Result verifyFileHash(quint64 hashedLength, const QByteArray &expected);
    Result streamResource(const ResourceInfo &e, QIODevice *out, QByteArray *mem);

    QFile file_;
    quint16 major_ = 0, minor_ = 0;
    quint32 flags_ = 0;
    QList<ResourceInfo> entries_;
};

}  // namespace resora
