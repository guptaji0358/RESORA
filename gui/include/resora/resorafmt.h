#pragma once
// .resora container: shared constants, error type and layout helpers. Qt Core only (no widgets, no
// dashboard knowledge). Layout is documented in docs/RESORA_FORMAT.md.
#include <QByteArray>
#include <QString>
#include <QtEndian>

namespace resora {

// ---- layout constants (little-endian, fixed-width) ----------------------------------------------
constexpr quint16 kVersionMajor = 1;
constexpr quint16 kVersionMinor = 0;
constexpr int kHeaderSize = 64;      // magic 8 | major 2 | minor 2 | flags 4 | headerSize 4 | rsv 12 | sha256(first 32) 32
constexpr int kEntrySize = 128;      // id[64] | type 4 | compression 4 | offset 8 | stored 8 | original 8 | sha256 32
constexpr int kDirHeaderSize = 8;    // count 4 | reserved 4
constexpr int kTrailerSize = 96;     // dirOffset 8 | dirLength 8 | count 4 | rsv 4 | dirSha 32 | fileSha 32 | endMagic 8
constexpr int kIdFieldSize = 64;     // NUL-padded; id is at most 63 bytes
constexpr int kMaxIdLength = 63;
constexpr int kShaSize = 32;

// ---- limits (reject before allocating) ----------------------------------------------------------
constexpr quint32 kMaxResources = 65536;
constexpr quint64 kMaxResourceSize = Q_UINT64_C(16) << 30;       // 16 GiB per resource
constexpr quint64 kDefaultMaxInMemory = Q_UINT64_C(256) << 20;   // readResource() default cap

inline QByteArray magic() { return QByteArray("\x89RESORA\x1A", 8); }
inline QByteArray endMagic() { return QByteArray("\x1A" "ARO" "SER\x89", 8); }

// Compression codes. Phase 1 stores everything uncompressed; code 1 is reserved for zlib.
enum Compression : quint32 { CompressionNone = 0, CompressionZlib = 1 };

// Resource type codes. Unknown values are preserved and never fatal.
enum ResourceType : quint32 { TypeOpaque = 0, TypeJson = 1, TypeImage = 2, TypeThumbnail = 3 };

enum class Error {
    None, InvalidArgument, IoError, WriteFailed, BadMagic, UnsupportedVersion, BadHeader, Truncated,
    BadTrailer, BadDirectory, TooManyResources, InvalidResourceId, DuplicateResourceId,
    ResourceOutOfRange, ResourceTooLarge, ChecksumMismatch, ResourceNotFound, UnsupportedCompression,
    Cancelled,  // added in Phase 2 (additive, appended last): progress callback asked to stop
};

struct Result
{
    Error error = Error::None;
    QString message;
    bool ok() const { return error == Error::None; }
    static Result fail(Error e, const QString &m) { return {e, m}; }
};

// Resource id: [a-z0-9][a-z0-9_-]{0,62}. A flat name, never a path (no '.', '/', '\\', ':').
bool isValidResourceId(const QString &id);

// ---- little-endian helpers ----------------------------------------------------------------------
inline void putU16(QByteArray &b, quint16 v) { char t[2]; qToLittleEndian(v, t); b.append(t, 2); }
inline void putU32(QByteArray &b, quint32 v) { char t[4]; qToLittleEndian(v, t); b.append(t, 4); }
inline void putU64(QByteArray &b, quint64 v) { char t[8]; qToLittleEndian(v, t); b.append(t, 8); }
inline quint16 getU16(const QByteArray &b, int o) { return qFromLittleEndian<quint16>(b.constData() + o); }
inline quint32 getU32(const QByteArray &b, int o) { return qFromLittleEndian<quint32>(b.constData() + o); }
inline quint64 getU64(const QByteArray &b, int o) { return qFromLittleEndian<quint64>(b.constData() + o); }

// True when [offset, offset+length) lies inside [lo, hi] without integer overflow.
inline bool rangeWithin(quint64 offset, quint64 length, quint64 lo, quint64 hi)
{
    return offset >= lo && offset <= hi && length <= hi - offset;
}

}  // namespace resora
