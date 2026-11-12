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

**Compression:** not implemented; all resources use code 0 (uncompressed), as in Phase 1. **Additive Phase 1 change:** `Writer::write` takes an optional progress/cancel callback and `Error::Cancelled` was appended; behaviour without a callback is unchanged. **Limitations:** images are read twice (hash pass + write pass); source modification is detected by hash/length/mtime, not prevented (no file locking); the swap over an existing target is rename-based, not a single atomic replace; the free-space pre-check added in Phase 7 is described under "Low-disk reliability (Phase 7)".

Tests: `gui/tests/tst_bundledexport.cpp` (target `tst_bundledexport`).

## Linked Data Export (Phase 3)

API: `gui/include/resora/linkedexport.h` (`exportLinked(LinkedExportInput, targetPath, ExportProgressFn) -> LinkedExportResult`), in `resora_format` (Qt Core only). Blocking; run on a worker thread. It reuses the Bundled types (`ExportError`, `ImageIssue`, `ExportProgress`, id helpers) and the shared internals in `exportcommon.*` (hashing, common JSON resources, via `addCommonResources` so `layout`/`charts`/`analytics`/`config` are byte-identical to Bundled for the same input) and the same temp -> verify -> `finalizeDestination` pipeline. Bundled behaviour is unchanged (internal refactor only; its tests pass).

**Resources (flat):** `manifest` (mode `linked`, adds `path_resolution`), `layout`, `charts`, `analytics`, `config`, `images-index`, `thumb-NNNN`. There is **no** `image-NNNN`; the verifier fails the export if any image-type resource exists. Original image bytes are never read into the container (they are only hashed, read-only).

**`images-index` record (`linked` object, replaces `bundled`):** `path` (absolute, `/` separators, cleaned), `relative_path` (string or null), `file_name`, `size`, `modified_utc` (ISO, ms), `sha256` (hex), `width`, `height`, `format` (copied from the supplied metadata, null if unknown), `name_collision` (bool). The record also has `image_id` (`img-NNNN`, the unique identifier), `name`, `metadata`, `thumbnail`. File names are never identifiers.

**Validation before completion:** each source must exist, be a regular file and be readable (streamed SHA-256 + size + mtime). Optional `expectedSize` / `expectedSha256` (analysis-time identity) make a differing file `Changed`. Before finalizing, every source is re-read and rehashed (cancellable, progress by bytes inside the Verifying window, never backwards, 1000 only after the swap) and its size + SHA-256 are compared with the values recorded during validation; any difference (or a source that can no longer be read) is `Changed` and aborts the export with the destination untouched. A cheap size+mtime pre-check runs first. All problems are collected in `result.issues` (Missing / Unreadable / Changed); any issue aborts with `SourceProblem`, nothing is written, an existing destination survives. Source paths must be absolute (`InvalidInput` otherwise); the destination may not be a source.

**Duplicate names:** images sharing a file name (case-insensitive on Windows) are reported in `result.duplicateNames` (`samePath` true when it is the same file listed twice) and flagged `name_collision`; export proceeds because identity is path + SHA-256, not the name.

**Path-resolution policy (for Phase 4):** (1) try `path` and require size + SHA-256 to match; (2) otherwise, if `relative_path` is non-null, resolve it against the base described in `manifest.path_resolution.base`: `{"kind":"folder","hint_path":...}` = an image root (the recorded path is only a hint; the user/relink root may replace it), `{"kind":"document"}` = the folder containing the `.resora` file at open time, `null` = no relative paths; (3) otherwise the user relinks. The `.resora` folder is **never** assumed to be the image folder; document-relative is used only when the exporter was told to. `relative_path` is recorded only when the image lies inside the base (never `..`, never absolute), uses `/`, and must be re-validated as untrusted on open (no `..`, no drive letters). Identity always decides acceptance. Mode is carried in `manifest.mode`; the header flags stay 0.

**Limitations:** a change made after the final rehash but before the swap (milliseconds) cannot be detected; the extra read doubles the I/O of a linked export; width/height are taken from the caller's metadata (the C++ side does not decode images); disk-space pre-check added in Phase 7 (metadata-only container); no import, relink or UI (Phases 4-6).

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

## Import and Opening (Phase 4)

API: `gui/include/resora/resoraimport.h` (`openResora`, `relinkAndOpen`, `closeDocument`, `readThumbnail`), in `resora_format` (Qt Core only, no widgets). Blocking; run on a worker thread. Internals: `importparse.cpp` (manifest/index validation), `linkresolve.cpp` (path safety + identity), `bundledextract.cpp` (cache + extraction), `importdocument.cpp` (orchestration). Bundled extraction checks cancel after every 1 MiB chunk of each image (additive `Reader::copyResource` chunk callback -> `Error::Cancelled`; bounded memory): the output is closed and the whole fresh `doc-<uuid>` folder, `.part` included, is removed, nothing else is touched. Progress (`ImportProgress`, stages Reading / Validating / Extracting / Done, monotonic permille, 1000 only at Done) and cancel (callback returns false -> `OpenError::Cancelled`) mirror the export API.

**Flow:** `Reader::open` (all Phase 1 checks incl. whole-file hash) -> require resources `manifest, layout, charts, analytics, config, images-index` -> JSON parsed (64 MiB cap each) -> manifest checks (`schema_version` 1, mode bundled|linked, 32-hex `document_id`, `image_count` equals records) -> record checks (unique `image_id`; mode-matching `bundled`/`linked` object only; thumbnail resource exists; bundled: resource id valid, unique across records, exists, type image, uncompressed, directory size/SHA-256 equal the index) -> mode step. Any failure returns a specific `OpenError` and an empty `document`; nothing is half-opened.

**Bundled:** a fresh folder `<cacheRoot>/doc-<uuid>` (default `QStandardPaths::CacheLocation/documents`) is created with `mkdir` (never reused), containing a `.resora-cache` marker. Each image is streamed (`Reader::copyResource`, which checks the resource SHA-256) to `<resource-id><ext>.part`, then re-hashed from disk (size + SHA-256 vs the recorded values) and renamed. File names come only from the validated resource id plus an alphanumeric (<=5 chars) extension of the display name; no container-supplied path is used. Free space is checked up front (needed bytes + 16 MiB) and after a write failure (`InsufficientStorage`). Failure or cancel removes only the folder this call created. **Lifetime/cleanup:** the caller owns the document; `closeDocument` deletes `cacheDir` only if it is a non-symlink folder named `doc-*` containing the marker; unmarked folders are never touched. Stale folders left by a crash are removed by the Phase 7 startup sweep (see "Compatibility, limits and reliability (Phase 7)").

**Linked:** per image, candidates in order: (1) recorded absolute path; (2) `relative_path` joined to the base from `manifest.path_resolution.base`: `folder` -> `OpenOptions::imageRoot` if given, then the recorded `hint_path`; `document` -> folder of the opened `.resora`; none -> no relative attempt. A candidate is accepted only when it is a regular readable file with the recorded size **and** SHA-256 (size is checked first as a cheap reject). File name alone is never trusted. States: `Ok / Missing / Unreadable / IdentityMismatch / UnsafePath`; the most informative state is reported. **Every image is checked (no early stop)**; if any fails, `OpenError::LinksBlocked` is returned with `problems` (image id, name, recorded/relative/resolved path, message), `links` (all) and no document. **Relink:** `OpenOptions::relinks` (image_id -> absolute file) or `relinkAndOpen`; a relink is the only candidate for that image (no fallback to another file) and must pass the same identity check; all images are revalidated from scratch on every call, so a file that changed after an earlier successful pass is caught.

**Path safety:** stored relative paths must be non-empty, `/`-separated, no leading `/`, no `\`, `:`, control characters, empty/`.`/`..` segments; the joined path must stay under the base (cleaned prefix check, plus canonical check when it exists so a link cannot escape); recorded absolute paths must be absolute. Rejected paths yield `UnsafePath`/blocked, never a different file.

**Limits:**  the `.resora` is never written; hashing a large linked set is I/O-bound (every open re-reads all linked files by design). Tests: `gui/tests/tst_resoraimport.cpp` (`tst_resoraimport`).

## Dashboard Integration (Phase 5)

API: `gui/include/backend/dashboardstate.h` (namespace `dashboard`), in `imgan_core` (Qt Core only; `imgan_core` now links `resora_format`). No schema or format change; `resora_format` is untouched.

**State:** `State {result, viewIndex}` = what `AnalyticsPanel::showAnalytics()` receives plus its view switch. The panel gained one read-only accessor, `result()` (raw object last shown, empty after `clear()`); no widget behaviour changed. `stale` is a transient hint about the current collection and is not persisted.

**Mapping:** `analytics` resource `data` = the engine object, unmodified. `layout` = `active_view` (`dashboard`|`detailed`), `visible_sections` null (not configurable today), `widgets` = `{id:"extreme.<role>", role, image_id}` for roles largest/smallest/highest/lowest (these are the only images the dashboard associates). `charts` = 4 definitions derived by `DashboardModel` (`chart.categories` bar, `chart.orientation` donut, `chart.formats` donut, `chart.storage` hbar) with `categories`, `series`, `data_source`. `config` = `recursive`, null `report_options`/`scan_filters`, and `result_context` = every non-`analytics` key of the result, verbatim. `manifest.source` from the result's folder/files info. Images = distinct extreme files (deduped by path), metadata from the engine image record, `image_id` = `img-NNNN` by first appearance. No thumbnails are generated (none exist in the dashboard state).

**Export:** `plan()` validates (analytics present, view 0/1, absolute image paths) and builds inputs; `exportBundled`/`exportLinked` call the Phase 2/3 engines (hashing, streaming, finalization, path policy, identity checks are theirs). Linked takes `RelativeBase`/`baseDir`, default none.

**Import:** `restore()` takes an `OpenedDocument` (so Phase 4 blocking, extraction and checksums already ran) and checks: analytics usable; layout schema, view, widget roles/ids, image ids exist, widget image name equals the analytics image, every present extreme has an association; charts structurally valid and equal to `chartsFor(analytics)`; config `result_context`. Unsupported values fail; unknown/unsupported fields produce `Restored::warnings` (never silent). `Session::open` = `openResora` + `restore` into a separate object; the active state (and its Bundled cache folder) is replaced only after both succeed; failure/cancel keeps the old state and removes the new attempt's cache.

**Limits:** charts must match what this build derives (a file from a build with different chart derivation is rejected; migration policy is Phase 7). Restored element images point to cache/validated paths in `ElementImage::path`; the analytics JSON keeps the original paths, and panel use of the mapping is Phase 6. No UI or `MainWindow` wiring.

Tests: `gui/tests/tst_dashboardstate.cpp` (12 passed).

## Export/Import UI (Phase 6)
Export: Analytics page action or Ctrl+Shift+E; choose Bundled (copies images, standalone, larger) or Linked (absolute paths, originals must remain, identity checked on open); destination must end in `.resora`, cannot be a source image, existing files need confirmation. Open: Ctrl+Shift+O; all unresolved linked images are listed together, replacements are checked by size and SHA-256, opening is blocked until all pass; a failed open never changes the current dashboard.

### Remove from Dashboard (Phase 6 correction, additive)
- Open: `OpenOptions::omitImages` (Linked only; this call only) skips validation of the listed image ids; unknown ids or a Bundled document fail with `InvalidInput`; all other images still block. The document then has no entry for them (`omittedImageIds`).
- Restore: widgets of omitted images are dropped and their roles reported in `Restored::removedRoles`; no path remains for them. The analytics JSON is unchanged.
- Export: roles in `State::removedRoles` get no widget and no image; they are listed in the optional `layout.omitted_roles`. Restore accepts a missing role widget only if the role is listed there (or omitted in the current open); a role both listed and present as a widget is rejected. `schema_version` stays 1; older readers reject such a file because the role association is missing.
- Original image files and the source `.resora` are never modified.

### Re-export and `omitted_roles` (Phase 6 final review)
- **Re-export source:** a restored dashboard carries `State::restored` and `State::runtimePaths` (role -> validated runtime file: extracted Bundled copy, or validated/relinked Linked original). `plan()` then uses only these paths; a remaining role without one is an error (no fallback to the persisted analytics paths). Bundled re-export therefore works with the original directory deleted, while the opened session (cache) is active. The persisted analytics JSON and the opened `.resora` are not modified.
- **`layout.omitted_roles`:** optional array of unique strings from `largest|smallest|highest|lowest` (schema: `docs/schemas/resora/dashboard.schema.json`). Reader rejects a non-array (including `null`), non-string/unknown roles, duplicates, and a role that is also a widget. It is written only when a role was removed; absent otherwise.
- **`schema_version` (layout) is now 2 when `omitted_roles` is present** (Phase 7 decision, see "Layout schema versions" below). Files without removals stay version 1.

## Compatibility, limits and reliability (Phase 7)

### Versioning and compatibility policy [documented = implemented and tested]
- **Container major** (header): only `1` is read; any other value -> `UnsupportedVersion` with both versions in the message. A major bump means a layout change.
- **Container minor**: any minor of major 1 is read (tested: minor 7). Minor bumps may only add data in reserved/optional places that a 1.0 reader can ignore without misreading the document. No minor above 0 has ever been written, so nothing is claimed for such files beyond "opens".
- **Header flags**: carried (`Reader::flags()`), never interpreted; no flag is defined, so none can be "required". A future feature that an old reader must not ignore needs a **major** bump (old readers then reject it). This is the stated answer to "required unknown features": they are not expressible in minor versions.
- **Resource types**: unknown type codes are preserved and not fatal (tested). **Compression**: only 0 (none) is readable; a resource with another code fails when read (`UnsupportedCompression`), not when the directory is parsed.
- **JSON `schema_version`** (manifest, images-index, charts, analytics, layout, config): must be a JSON integer >= 1 and one the reader supports. Supported: 1 for manifest, images-index, charts, analytics, config; 1 and 2 for layout (2 only with `omitted_roles`). Missing/non-integer/0/`"1"`/1.5 -> "has no valid schema_version"; greater -> "written by a newer RESORA"; smaller or unsupported -> "cannot read (no migration exists)". `resora::schemaVersionProblem` is the single implementation. **Validation layers (intentional):** the container importer (`importparse.cpp`) checks the manifest, images-index and charts versions and the manifest/index structure; `dashboard::restore` (`dashboardrestore.cpp`) checks the analytics, layout and config versions, because they are only meaningful with the dashboard's own rules (widgets, roles, analytics agreement). Moving them would duplicate checks without a defect to fix.
- **Unknown JSON fields**: unknown top-level keys of layout/config are accepted but reported in `Restored::warnings` (never silent). Fields the reader depends on stay strict (unknown/duplicate `omitted_roles`, null, non-array -> rejected).
- **`layout.omitted_roles`**: absent in every file written before it existed and in any file without removed images; absence = no omitted roles (tested: current files restore with no warnings). Present and valid -> honoured. A build that predates the field would reject a file that uses it (code reading; no such build was run), which is intended because the file is not representable without it.
- **Layout versions (Phase 7):** `1` = no `omitted_roles` (every file written before this change, and every file without removals); `2` = may carry `omitted_roles`. The reader accepts 1 and 2; `omitted_roles` in a version-1 file is refused ("requires layout schema_version 2"); any other value is refused with the standard newer/older/invalid messages. **Real-reader evidence:** the pre-change Phase 5/6 reader (commit a467857^, built from `git archive`) restores `v1_no_omitted_roles.resora` with no warnings and rejects `v2_omitted_largest.resora` with "layout has an unsupported schema_version". Without the bump the same old reader rejects removed-role files only indirectly ("layout has no image association for ..." — by code reading; that un-bumped file was not produced for a run). **Migration: none.** v1 to v2 only adds an optional key; the reader never rewrites a file; a re-export writes the version that matches the state (tested: `reexportPreservesVersionAndAssociations`). Fixtures: `gui/tests/fixtures/dashboard/` (regenerate with `RESORA_WRITE_FIXTURES=<folder>` and the `writeCompatibilityFixtures` test).
- **Migrations implemented: none.** Only schema/major 1 has ever existed, so there is no historical format to migrate; inventing one was rejected. Anything else is refused with the messages above.

### Resource limits (enforced before allocating)
| Limit | Value | Why |
|---|---|---|
| Resources per container | 65536 | the directory is read whole (<= 8 MiB); bounds work and memory |
| Single resource | 16 GiB | far above a camera original; payloads are streamed in 1 MiB chunks, never loaded whole |
| `readResource` in-memory cap | 256 MiB (default) | JSON/thumbnails only; images are copied, not buffered |
| JSON resource on import | 64 MiB each | parsed in memory |
| Cache folder entries (sweep) | 70000 | refuses to delete a folder with an implausible number of entries |

Duplicate-id checks (writer, reader) are hashed: a 65536-entry directory costs O(n), not O(n^2).

### Hostile-input review (code reading + tests)
Offset/length arithmetic uses overflow-safe range checks (`offset = 2^64-8` and sizes of 2^64-1 are rejected before any read); declared sizes are validated against the file before allocation; payload overlap, truncation, trailing garbage, duplicate/invalid ids and header/directory/whole-file/per-resource SHA-256 are rejected (Phase 1 tests plus `hostileDirectoryEntries`, `manyResourcesStress`). Extraction names are generated: `safeStem(resource id)` (ids cannot contain separators; Windows device names get an `r-` prefix) + an extension only from a fixed image-extension list, otherwise `.img`, so a display name such as `payload.exe` never creates an executable-looking file (tested). Nothing is extracted outside `<cacheRoot>/doc-<uuid>`.

### Cache lifetime and stale-cache sweep
- A Bundled session owns `<cacheRoot>/doc-<32 hex>` while the document is open; `closeDocument`, or replacing the session (the old one is released only after the new one is adopted), removes it. Failed/cancelled opens remove their own folder.
- **Marker** `.resora-cache` is JSON: `{app:"RESORA", kind:"resora-document-cache", version:1, document_id, pid, created_utc}`. Phase 4 markers (bare document id) are still recognised.
- **Startup sweep** (`resora::sweepStaleCache`, called once from `main()` before the window): only direct children named exactly `doc-<32 hex>` that are real directories (symlinks, junctions and shortcuts are never followed, nor is a linked cache root), carry a valid marker, contain only the marker plus files shaped like generated names (`<id>.<known image ext | img>[.part]`), whose owner pid is not alive (unknown/denied = alive) and untouched for 1 h (7 days when the marker has no pid). It removes those files and then `rmdir`s the folder - never recursively. Everything else is left and counted in `SweepReport`.
- **Limits:** a recycled pid keeps a stale folder until that pid is free (safe direction); the age rule delays cleanup by up to an hour; a locked file (antivirus) makes removal fail and it is retried at the next start.

### Export reliability (see also "Destination finalization")
Covered by automated tests: vanished/changed/same-size-restored-mtime sources (final rehash), cancel at validation/writing/verification/final rehash, unwritable/locked/read-only destinations, existing destination intact after every failure, no temp leftovers, `ReplaceFileW` failure shapes incl. 1177 and unexpected states (nothing deleted, `keepTemp`). **Honest limits:** a crash or power loss during finalization can leave the destination, a `.tmp` sibling and/or a backup sibling; the algorithm never deletes the only valid copy but the swap is not power-fail atomic. Insufficient disk space is covered by deterministic fault injection (see "Low-disk reliability (Phase 7)"); no real full disk was used.

### Filesystem and platform assumptions (not verified beyond what is stated)
Developed and tested on Windows 11 / local NTFS only. Network shares, removable drives, FAT/exFAT, cloud-synced folders (e.g. OneDrive) and antivirus filters were NOT tested: `ReplaceFileW` and sharing violations are retried briefly (3 tries) but behaviour there is unknown. The non-Windows finalizer branch (`rename`) was not run. No second platform is supported by the project and none was added.

### Verified capacity vs theory
Verified in tests: 20000 resources in one container (write + open + read, seconds); 3 MiB single image with chunk-level cancel; 25 consecutive open/replace/close cycles leave an empty cache; Linked export of a few hundred references. Theoretical only (limits above, not exercised): 65536 resources, 16 GiB resources.

### Unicode and long paths (Phase 7, tested on Windows NTFS)
`unicodeAndLongPathRoundTrip` (tst_resorareliability) exports to a target and source under non-ASCII folder/file names with a path longer than 200 characters, then opens the bundle and compares the image bytes and cache cleanup. Only this path shape was exercised; paths beyond MAX_PATH without the long-path manifest were not tested.

### Low-disk reliability (Phase 7)
Both exports run an up-front free-space check: the container size (`Writer::containerBytes`) plus a 16 MiB reserve must fit on the destination volume, otherwise `ExportError::InsufficientStorage` is returned before any byte is written. Bundled needs the image bytes; Linked needs only the container (thumbnails and JSON), because images are never stored. The check is skipped only when the volume reports no free space (negative value). Write-time exhaustion (`Error::OutOfSpace` from the Writer) is also reported as `InsufficientStorage`. **Real ENOSPC is not classified:** on a real disk, a failed write surfaces as `WriteFailed` with Qt's text, because `QSaveFile` does not expose the OS error code. Only the injected limit produces `OutOfSpace`.
**Seams (tests only, `gui/include/resora/exportseams.h`; plus `detail::FinalizeOps::flush` for the finalizer's open/FlushFileBuffers step):** available bytes, reserve, simulated write limit, a hook after the temp file is complete, and the destination swap. Production callers use the defaults.
**Guarantees covered by `tst_exportlowdisk` (deterministic, no real disk fill):** insufficient space before writing (no temp, target intact); exhaustion mid-payload and in the trailer (target intact, temp removed, progress never 1000); silent truncation caught by verification (`VerifyFailed`); clean replacement refusal (previous file kept, temp removed); uncertain replacement (every surviving file kept, nothing deleted, reported by name); cancel, space and verification remain distinct error codes; a later success leaves only the destination. Progress reaches 1000 only with the final `Done` event.
**Source growth:** the pre-check is a snapshot, not a guarantee. Bundled caps the Writer byte budget at the estimate, so a source that grows after the check aborts the write early and is reported as `SourceProblem`/`Changed` (temp removed, destination untouched); Linked never streams source bytes and its final rehash rejects growth. Growth is detected, not prevented. A flush/open failure before replacement moves nothing and leaves the destination byte-identical, but who removes the temp depends on the level: **finalizer level** (`finalizeWith` called directly): the temporary file is retained (`Failed`, the caller owns it); **export level** (`exportBundled`/`exportLinked`): the finalizer reports `keepTemp=false`, so the export's cleanup removes the temporary file and only the old destination remains (`FinalizeFailed`, no `Done`). Both are covered by `tst_exportlowdisk`. *(Supersedes the earlier wording "temp kept" without this distinction.)*

**Limits:** the free-space pre-check uses a fixed 16 MiB reserve, not a measured size of the replacement (a margin, not a guarantee); during replacement the old and new files coexist briefly, which the reserve must cover. A full disk, network share, removable drive, antivirus lock during replacement and power loss were not tested.

### Open Phase 7 items (not decided or not verified)
- **File association:** no decision recorded; not implemented. Needs an explicit user decision.
- **`omitted_roles` older-reader behaviour:** resolved (not open). Decision: layout version 2 when `omitted_roles` is present; the reader accepts 1 and 2; Phase 5 readers reject version 2 (real probe, see "Layout versions (Phase 7)" above).
- **Low disk:** covered by injected failures for export (see "Low-disk reliability (Phase 7)"); no real full-disk or removable-drive test yet.

### Linked-path security policy and cache ownership (Phase 7, 2026-11-12)
No format change: only reader behaviour is stricter. `hint_path`, `path` and `relative_path` stay as written; the reader decides what it will touch.

**Linked paths (initial open and relink use the same rules, decided from the string before any filesystem call):**
- Recorded absolute paths, relink replacements, `hint_path` and a caller `imageRoot` must be *local absolute* (`isLocalAbsolutePath`): UNC (`\host\share`, `//host/share`), device (`\?\`, `\.\`, `//?/UNC/...`), drive-relative (`C:x`), relative and control-character paths are rejected as `UnsafePath` ("not accessed"). Opening an untrusted `.resora` therefore never contacts an SMB share via these fields.
- The one base that may be non-local is the folder the user opened the `.resora` from (document-relative base); that location was chosen by the user.
- Relative paths must pass `isSafeRelativePath` and stay inside their base (lexically and, when the target exists, by canonical path, so junctions/symlinks inside the base cannot lead out).
- A relink is the only candidate for that image: a rejected or failing replacement never falls back to the recorded or relative path. SHA-256 + size identity verification is unchanged.
- Explicit network support, if ever wanted, must be a separate user-controlled option; none exists.

**Bundled cache ownership/deletion (closeDocument, failed-open cleanup and startup sweep share `removeOwnedCacheDir`):**
- Removable only if: name is `doc-<32 hex>`, not a symlink/junction/shortcut, valid RESORA marker (closeDocument additionally requires the marker pid to be this process or a legacy marker), and every direct entry is the marker or a regular, non-link file with a RESORA-generated name. Deletion is never recursive and never follows links.
- Anything else (foreign file/subfolder, link inside, bad/missing/foreign marker, wrong name) keeps the whole folder untouched (`NotOwned`).
- The marker is deleted last, so a partial failure (file in use/read-only) leaves a still-provable folder; `closeDocument` returns false and keeps `cacheDir`; `dashboard::Session::close()` returns false and records the folder in `pendingCleanup()`; `retryCleanup()` retries; otherwise the startup sweep removes it once the owner is dead.
- Race hardening (Windows): after the name/link check, `removeOwnedCacheDir` holds a `DirLock` on the folder (handle opened with `FILE_FLAG_OPEN_REPARSE_POINT`, no `FILE_SHARE_DELETE`) until the final `rmdir`. While held, the folder and its parents cannot be renamed, deleted or replaced by a junction/symlink, so content validation and file deletion act on the validated folder. A folder that is itself a reparse point is refused; one that cannot be locked reports `Failed` (retry later). Deletion APIs (`DeleteFileW`, `RemoveDirectoryW`) act on a link itself and never follow it.
- **Actual guarantee:** RESORA never deletes anything outside a `doc-<32 hex>` folder it validated, never recursively, never through a link it can see. Remaining local-attacker window: the instant between releasing the lock and the final `rmdir` (a junction swapped in then is removed as a link only, its target untouched), and a same-user process that can already write the cache root can do anything the user can; this is not a privilege boundary and no ACL hardening is applied. Local NTFS only; non-Windows has no lock (check-then-delete).
- **Pending cleanup ownership (application level, supersedes the per-Session list):** `dashboard::CacheCleanupManager` (`gui/include/backend/cachecleanupmanager.h`) owns every cache folder whose removal failed. MainWindow holds it as a `shared_ptr`; each `Session` and each import job/dialog receives the same pointer, so a failed folder is registered even when the Session is destroyed on a worker thread or replaced. The manager is mutex-protected, holds path strings only, and never deletes anything itself. `add()` de-duplicates by cleaned path. `retryAll()` is one synchronous pass: each folder goes through `resora::closeDocument` (the full close ownership check, including the pid rule), gone/removed entries leave the list, anything refused or still locked stays pending untouched. It never waits or loops. **Retry points:** after a dashboard is replaced (`openDashboardFile`, after the swap), after the active session is reset (Analytics hand-off, fresh analysis), after the Open dialog closes, and once in `~MainWindow`. There is no timer and no UI. A session constructed without a manager gets a private one (standalone use, tests). **Fallback:** a folder still pending at exit keeps its marker (removed last), so the next start's `sweepStaleCache` (owner pid dead, folder older than 1 h; 7 days for legacy markers) removes it. The manager itself is not persisted across application runs.

**Mapped network drives (Windows):** `isLocalAbsolutePath` also rejects a drive letter whose DOS device (`QueryDosDeviceW`, a local object-manager lookup, no I/O to the volume or network) is a network redirector (`\Device\LanmanRedirector`, `\Device\Mup`, WebDAV, VirtualBox/VMware shared folders, WinFsp) or a SUBST of `\??\UNC\...` / of another remote letter (followed up to 4 levels). `\Device\HarddiskVolume*`, `Harddisk*`, `CdRom*`, `Floppy*` are local (also mounted folders, VHDs, Dev Drives, local SUBST). Unknown devices fall back to `GetDriveTypeW` (`DRIVE_REMOTE`/`UNKNOWN`/`NO_ROOT_DIR` rejected). An unassigned or disconnected-and-unmapped letter is not contacted and ends as "not found". A disconnected mapped drive is still defined in the device table, so it is rejected without waiting for any timeout.
- **Not covered:** a local directory symlink/junction pointing at a network location (the redirect is only discovered when the file is opened; blanket reparse-point rejection would break OneDrive placeholders); a third-party volume driver that reports a fixed drive; a mapping created after the check. The policy blocks network drives; it does not claim every network path is blocked.
- Remaining limits: non-atomic cache check-then-delete on non-Windows; local NTFS only.
