# `.resora` container format (version 1.0)

Binary container for RESORA dashboard exports. Implemented in Phase 1 (see [implement.md](../implement.md)) as the `resora_format` static library: C++17, **Qt Core only** (`QFile`, `QSaveFile`, `QCryptographicHash`), headers in `gui/include/resora/`, sources in `gui/src/resora/`. It knows nothing about dashboards, images or widgets: resources are opaque bytes.

All integers are **little-endian**, fixed width. Hash = **SHA-256** (32 bytes). Not a ZIP; no internal folders.

## Layout

```
[ Header 64 ][ payload 0 ][ payload 1 ] ... [ Directory ][ Trailer 96 ]
```

**Header (64 bytes at offset 0)**

| Off | Size | Field |
|---|---|---|
| 0 | 8 | magic `89 52 45 53 4F 52 41 1A` (`\x89RESORA\x1A`) |
| 8 | 2 | format major (1) |
| 10 | 2 | format minor (0) |
| 12 | 4 | flags (opaque to Phase 1; later phases: mode bits) |
| 16 | 4 | header size (64) |
| 20 | 12 | reserved, zero |
| 32 | 32 | SHA-256 of bytes 0..31 |

**Payloads**: resource bytes back to back from offset 64, in directory order, no padding, stored uncompressed (compression code 0).

**Directory** (starts at `dirOffset`, length `8 + 128 * count`)

`count u32 | reserved u32`, then `count` entries of 128 bytes:

| Off | Size | Field |
|---|---|---|
| 0 | 64 | resource id, ASCII, NUL-padded (max 63 chars) |
| 64 | 4 | type (0 opaque, 1 json, 2 image, 3 thumbnail; unknown values preserved) |
| 68 | 4 | compression (0 none; 1 reserved for zlib; other = unsupported on read) |
| 72 | 8 | payload offset |
| 80 | 8 | stored length |
| 88 | 8 | original length (equals stored length when uncompressed) |
| 96 | 32 | SHA-256 of the stored bytes |

**Trailer (last 96 bytes)**

| Off | Size | Field |
|---|---|---|
| 0 | 8 | dirOffset |
| 8 | 8 | dirLength |
| 16 | 4 | resource count |
| 20 | 4 | reserved |
| 24 | 32 | SHA-256 of the directory |
| 56 | 32 | SHA-256 of the whole file from byte 0 up to trailer byte 55 |
| 88 | 8 | end magic `1A 41 52 4F 53 45 52 89` |

### Whole-file SHA-256 (precise definition)

Let `N` be the file size. The whole-file hash is `SHA-256(file[0 .. N-40))`, i.e. every byte from offset 0 up to (not including) the 32-byte hash field, which starts at `N - 40` (trailer offset 56). That range includes the header, all payloads, the directory, and trailer bytes 0..55 (dirOffset, dirLength, count, reserved, directory hash). It **excludes** the hash field itself and the 8-byte end magic. The definition is therefore non-circular: no covered byte depends on the digest, so no zero-filling is needed (the result is identical to hashing with the field treated as absent). The writer feeds the same bytes, in the same order, to a streaming hasher and appends the digest last; the reader re-hashes `[0, N-40)` in 1 MiB chunks before trusting the directory. The end magic is not hashed but is validated as a constant, and a wrong/missing one reports `Truncated`. Tests: `wholeFileHashDefinition` (independent recomputation), `modifiedWholeFileHashField` (every digest byte flipped, zero-filled field, bad end magic), `flippedBytesAnywhere`.

The trailer sits at a fixed distance from EOF, so truncation is detected by a missing end marker. `dirOffset + dirLength` must equal the trailer start exactly (trailing garbage is rejected).

## Resource ids and types

Id regex `[a-z0-9][a-z0-9_-]{0,62}`: a flat name, never a path (no `.`, `/`, `\`, `:`, spaces, upper case). Examples: `manifest`, `layout`, `thumb-0001`, `image-0001`. Ids are unique. Types are advisory; readers must tolerate unknown types.

## Validation (Reader::open)

In order: size and magic (`BadMagic`/`Truncated`) → header size, checksum and major version (`BadHeader`/`UnsupportedVersion`; a different minor is accepted) → end marker (`Truncated`) → count ≤ 65536 (`TooManyResources`) → `dirLength` matches count and directory location (`BadTrailer`) → streaming whole-file hash (`ChecksumMismatch`) → directory hash → each entry: id valid/NUL-padded, lengths ≤ 16 GiB, offset+length within `[64, dirOffset]` using overflow-safe arithmetic (`ResourceOutOfRange`/`ResourceTooLarge`), lengths consistent (`BadDirectory`) → no overlapping payloads, no duplicate ids. Only then are payloads reachable. Every size is checked **before** allocating. `readResource` additionally caps in-memory size (default 256 MiB) and re-verifies the resource hash; `copyResource` streams in 1 MiB chunks (on `ChecksumMismatch` the caller must discard the output).

## Writing (Writer::write)

Resources are added with `addBytes` or `addFile` (streamed from a read-only source at write time). `write()` uses `QSaveFile`: output goes to a temporary file in the target folder and replaces the target only on `commit()`; any failure leaves an existing target untouched and no temporary file behind. Hashes are computed while streaming. Output is deterministic for identical input (no timestamps in the container). **Integrity guarantees before `commit()`** (public `QSaveFile` API only): every `write()` is checked for a full write; the directory and trailer lengths are checked against the entry count; the device is flushed; and `size()`/`pos()` must equal exactly `dirOffset + dirLength + 96`, otherwise the write is cancelled (`WriteFailed`) and the target is untouched. Payload hashes are computed from the same buffers passed to `write()`. **Limitations:** the temporary file is not re-opened and re-read through the `Reader` before the rename (that would depend on QSaveFile's undocumented temp-file location), so on-disk bit errors after a successful `write()` are not caught at write time; they are caught by the whole-file hash when the file is opened. Round-trip tests cover the reader/writer agreement.

## Extending

- **New resource types / ids**: just add them; no format change.
- **Compression**: set the compression code and original length; old readers fail only when reading that resource (`UnsupportedCompression`), not on open.
- **Minor version bump**: backwards-compatible additions (use the reserved fields). **Major bump**: layout change; old readers reject with a clear message.
- JSON resource schemas (documentation only; the library never parses them) are versioned JSON Schema 2020-12 files in `docs/schemas/resora/` (`schema_version` 1): `dashboard.schema.json` (`manifest` + `layout`), `charts.schema.json`, `settings.schema.json` (`config`), `analytics.schema.json`, `metadata.schema.json`, `image-references.schema.json` (`images-index`). Fields tagged `x-phase` have final semantics decided in that phase (2: bundled export and options, 3: linked identity, 5: layout and chart kinds). Unknown fields are ignored (`additionalProperties: true`); unknown optional values are `null`, never `0`. Resource overview: implement.md section 6.

Tests: `gui/tests/tst_resorafile.cpp` (target `tst_resorafile`, links only `resora_format` + QtTest).

## Bundled Export (Phase 2)

API: `gui/include/resora/bundledexport.h` (`exportBundled(BundledExportInput, targetPath, ExportProgressFn) -> ExportResult`), in the `resora_format` library (Qt Core only, no widgets). Blocking; the UI (Phase 6) runs it on a worker thread and chooses the destination. Input: app version, optional `source`, `layout`, `charts`, `analytics` (+ engine protocol), `config`, and a list of `ExportImage` (source path, display name, metadata JSON, optional thumbnail bytes). Result: error class, message, per-image `issues` (Missing / Unreadable / Changed), document id, image count, byte totals. Progress: stages Validating / Writing / Verifying / Finalizing / Done with byte counts and an overall `permille` (monotonic; 1000 only at Done, after the swap); the callback returns false to cancel.

**Resource mapping (flat ids; image N is the 0-based list position + 1, 4 digits):** `manifest` (mode `bundled`, document id, inventory), `layout`, `charts` (`{schema_version, charts[]}`), `analytics` (`{schema_version, engine_protocol, data}`, data stored unmodified), `config`, `images-index` (one record per image: `image_id` `img-NNNN`, `name`, `metadata`, `thumbnail` id or null, `bundled{resource, sha256, size}`), `thumb-NNNN` (type thumbnail, only when supplied), `image-NNNN` (type image, original bytes). File names live only in `images-index.name`, never in ids, so duplicate names are safe. Empty optional JSON becomes `{"schema_version":1}`; a zero-byte image is packaged as an empty resource.

**Flow:** (1) validate every source (exists, regular file, readable) while streaming a SHA-256 and recording size+mtime; all problems are collected, nothing is written if any exists; (2) build the description; (3) Phase 1 `Writer` streams images (1 MiB chunks) into a temporary sibling `<target>.<id>.tmp`; (4) reopen the temp file with `Reader`, check the resource set and that each image resource's hash and length equal the validation-time identity (a source changed during export is reported as `Changed`); (5) recheck source size+mtime, then swap into place (an existing target is parked under a backup name and restored on failure). Failure or cancel removes the temp file and leaves any existing target intact. The destination may not be one of the source images. Originals are only opened read-only.

**Compression:** not implemented; all resources use code 0 (uncompressed), as in Phase 1. **Additive Phase 1 change:** `Writer::write` takes an optional progress/cancel callback and `Error::Cancelled` was appended; behaviour without a callback is unchanged. **Limitations:** images are read twice (hash pass + write pass); source modification is detected by hash/length/mtime, not prevented (no file locking); the swap over an existing target is rename-based, not a single atomic replace; a size estimate / free-disk-space check is not implemented yet.

Tests: `gui/tests/tst_bundledexport.cpp` (target `tst_bundledexport`).

## Linked Data Export (Phase 3)

API: `gui/include/resora/linkedexport.h` (`exportLinked(LinkedExportInput, targetPath, ExportProgressFn) -> LinkedExportResult`), in `resora_format` (Qt Core only). Blocking; run on a worker thread. It reuses the Bundled types (`ExportError`, `ImageIssue`, `ExportProgress`, id helpers) and the shared internals in `exportcommon.*` (hashing, common JSON resources, via `addCommonResources` so `layout`/`charts`/`analytics`/`config` are byte-identical to Bundled for the same input) and the same temp -> verify -> `finalizeDestination` pipeline. Bundled behaviour is unchanged (internal refactor only; its tests pass).

**Resources (flat):** `manifest` (mode `linked`, adds `path_resolution`), `layout`, `charts`, `analytics`, `config`, `images-index`, `thumb-NNNN`. There is **no** `image-NNNN`; the verifier fails the export if any image-type resource exists. Original image bytes are never read into the container (they are only hashed, read-only).

**`images-index` record (`linked` object, replaces `bundled`):** `path` (absolute, `/` separators, cleaned), `relative_path` (string or null), `file_name`, `size`, `modified_utc` (ISO, ms), `sha256` (hex), `width`, `height`, `format` (copied from the supplied metadata, null if unknown), `name_collision` (bool). The record also has `image_id` (`img-NNNN`, the unique identifier), `name`, `metadata`, `thumbnail`. File names are never identifiers.

**Validation before completion:** each source must exist, be a regular file and be readable (streamed SHA-256 + size + mtime). Optional `expectedSize` / `expectedSha256` (analysis-time identity) make a differing file `Changed`. Before finalizing, every source is re-read and rehashed (cancellable, progress by bytes inside the Verifying window, never backwards, 1000 only after the swap) and its size + SHA-256 are compared with the values recorded during validation; any difference (or a source that can no longer be read) is `Changed` and aborts the export with the destination untouched. A cheap size+mtime pre-check runs first. All problems are collected in `result.issues` (Missing / Unreadable / Changed); any issue aborts with `SourceProblem`, nothing is written, an existing destination survives. Source paths must be absolute (`InvalidInput` otherwise); the destination may not be a source.

**Duplicate names:** images sharing a file name (case-insensitive on Windows) are reported in `result.duplicateNames` (`samePath` true when it is the same file listed twice) and flagged `name_collision`; export proceeds because identity is path + SHA-256, not the name.

**Path-resolution policy (for Phase 4):** (1) try `path` and require size + SHA-256 to match; (2) otherwise, if `relative_path` is non-null, resolve it against the base described in `manifest.path_resolution.base`: `{"kind":"folder","hint_path":...}` = an image root (the recorded path is only a hint; the user/relink root may replace it), `{"kind":"document"}` = the folder containing the `.resora` file at open time, `null` = no relative paths; (3) otherwise the user relinks. The `.resora` folder is **never** assumed to be the image folder; document-relative is used only when the exporter was told to. `relative_path` is recorded only when the image lies inside the base (never `..`, never absolute), uses `/`, and must be re-validated as untrusted on open (no `..`, no drive letters). Identity always decides acceptance. Mode is carried in `manifest.mode`; the header flags stay 0.

**Limitations:** a change made after the final rehash but before the swap (milliseconds) cannot be detected; the extra read doubles the I/O of a linked export; width/height are taken from the caller's metadata (the C++ side does not decode images); no disk-space estimate; no import, relink or UI (Phases 4-6).

Tests: `gui/tests/tst_linkedexport.cpp` (target `tst_linkedexport`).

### Destination finalization (Windows failure handling)
Implemented in `gui/{include,src}/resora/destinationfinalizer.*` (`finalizeDestination`; algorithm `detail::finalizeWith`).

**Strategy.** New destination: `MoveFileExW(tmp, dst, 0)` (no overwrite). Existing destination: `ReplaceFileW(dst, tmp, backup, REPLACEFILE_IGNORE_MERGE_ERRORS)` with a sibling backup name, which is deleted only after success. Non-Windows: `rename(2)`.

**API-documented guarantees** (Microsoft Learn, ReplaceFileW, read 2026-11-08):
| GetLastError | Documented state |
|---|---|
| 1175 `ERROR_UNABLE_TO_REMOVE_REPLACED` | replaced file could not be deleted; replaced and replacement keep their original names |
| 1176 `ERROR_UNABLE_TO_MOVE_REPLACEMENT` | with a backup name: replaced and replacement keep their original names (without one the replaced file is gone - we always pass one) |
| 1177 `ERROR_UNABLE_TO_MOVE_REPLACEMENT_2` | replacement still has its original name but inherited the old file's streams/attributes; the **replaced file exists under a different name (the backup name)**, so the destination path can be missing |
| any other error (e.g. invalid parameter) | both files keep their original names; no backup exists |
| `REPLACEFILE_WRITE_THROUGH` | documented as not supported (not used) |
All three files must be on one volume (tmp and backup are siblings of the destination).

**Handling.** The code never trusts the code alone; after a failed call it observes {target, backup, tmp}:
1. target present, backup absent, tmp present -> clean failure (1175, 1176, other errors): previous file intact, `DestinationLocked`/`Failed`; caller deletes tmp.
2. target absent, backup present, tmp present (the 1177 shape) -> the backup is moved back with a non-overwriting move. Success: clean failure, tmp (verified new export) still exists. Restore failure: `NeedsAttention`.
3. any other combination -> `NeedsAttention`: **nothing is deleted or moved, no retry**, `FinalizeResult::keepTemp` is set so `exportBundled` does not remove the temp file, and the message lists the surviving paths (destination / previous version / new export).
4. Transient sharing/lock/access-denied errors (32, 33, 5) retry only while the state is clean (6 attempts, 50-250 ms backoff).
5. Backup deletion happens only after `ReplaceFileW` returned success.

**Assumptions, not documented guarantees:** (a) the OS persists the swap across a power loss (no directory-entry flush; only the temp file is flushed); (b) filesystems other than local NTFS (network shares, FAT/exFAT) give the same behaviour; (c) a process crash inside `ReplaceFileW` leaves a state covered by the documented shapes above, but the next run does not auto-recover a stray backup or temp (none is deleted automatically); (d) in step 1 the file at target is the old one (inferred from the documented "keep original names"). Real 1175/1176/1177 cannot be provoked from user mode here, so those paths are tested through the `detail::FinalizeOps` seam with simulated file states; the real `ReplaceFileW` is exercised for success, locked and read-only destinations.
