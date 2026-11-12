# implement.md — `.resora` dashboard export: implementation guide

Permanent implementation guide for RESORA's proprietary dashboard export format (`.resora`). Created 2026-11-08.
Labels used throughout: **[CONFIRMED]** = verified in the repository on 2026-11-08; **[PROPOSED]** = future architecture, not yet existing; **[DECISION]** = product decision from the user, preserved exactly.

## Rules for future work (read first)

- A separate prompt will be provided for each phase.
- Implement **only** the explicitly requested phase. Do **not** automatically begin subsequent phases.
- After each completed phase, update this file (section 14 and the phase's status), the relevant `AgentsMemory/` files (`CURRENT_STATE.md`, `MILESTONES.md`, `DECISIONS.md` as needed) and `docs/MILESTONE.md`.
- Preserve completed phases unless a necessary correction is approved by the user.
- Do **not** commit or push unless explicitly instructed.
- Follow `CLAUDE.md`: modules small and single-purpose, tests with every change, never claim manual/visual verification that did not happen, never modify user images.
- Do not invent existing classes or APIs: anything under [PROPOSED] must be created in its phase.

---

## Phase tracker (permanent; update after every phase)

Statuses: NOT STARTED, IN PROGRESS, COMPLETED, BLOCKED. A phase is COMPLETED only when its completion criteria and required tests have passed.

| Total phases | Completed | In progress | Not started | Blocked | Overall completion |
|---|---|---|---|---|---|
| 7 | 5 | 1 | 1 | 0 | 71.43% (5/7 completed) |

Phase 1 status: COMPLETED. Formally approved by the user (2026-11-08); evidence in section 14.
Phase 2 status: COMPLETED (approved by the user 2026-11-08).
Phase 3 status: COMPLETED (approved by the user; confirmed in the Phase 5 final-review request, 2026-11-08).
Phase 4 status: COMPLETED (approved by the user 2026-11-08, stated at the start of the Phase 5 request).
Phase 5 status: COMPLETED (approved by the user; stated at the start of the Phase 6 request, 2026-11-09).
Phase 6 status: COMPLETED (approved by the user; stated at the start of the Phase 7 request, 2026-11-10).
Phase 7 status: IN PROGRESS (started 2026-11-10).

---

## 1. Project objective

Let the user export an Analytics dashboard (the result of an analysis, with its layout, charts, configuration, metadata and thumbnails) to a single `.resora` file, and later reopen it in RESORA to reconstruct the same dashboard. Two export modes: **Bundled** (self-contained, includes original images) and **Linked Data** (small, keeps references to the original images). Opening must be safe (no path traversal, integrity-checked) and honest (never silently accept missing, replaced or modified images).

## 2. `.resora` file format overview

- [DECISION] Extension `.resora`. RESORA-owned proprietary **binary container**; **not** a ZIP renamed to `.resora`. Everything lives in one file. Flat resource structure; no internal subfolders.
- [DECISION] Format versioning, checksums, safe extraction and atomic export are mandatory.
- [DECISION] Both modes reconstruct the **same** dashboard when their dependencies are available.
- [DECISION] The file-format engine is independent of dashboard UI code.
- [DECISION] Original user images are never modified (opened read-only only).

Content of every file: container header, a resource directory (index), the resources themselves, and a trailer/integrity record. Mode-specific difference is only whether image resources are present (Bundled) or replaced by image references plus identity records (Linked Data).

## 3. Bundled Export specification

- [DECISION] Includes: dashboard layout, charts, configurations, metadata, thumbnails and **original images**.
- [DECISION] Must work independently of the original image locations.
- [DECISION] On opening, bundled resources are extracted into application-managed temporary/cache storage (never next to the user's originals, never into a user-chosen folder without consent).
- [PROPOSED] Bundled images are stored byte-for-byte with their checksum; extraction verifies each checksum before the file is exposed.
- [PROPOSED] The cache location comes from Qt's per-user cache location (`QStandardPaths::CacheLocation`), one subfolder per opened document id, cleaned on close and by a startup sweep of stale entries. (The "no internal subfolders" rule applies to the container, not to the on-disk cache.)
- [PROPOSED] Because bundles can be large, writing and extraction stream in chunks; whole images are never held in memory.

## 4. Linked Data Export specification

- [DECISION] Includes: dashboard layout, charts, configurations, metadata, thumbnails and **image references**. Excludes original image files. Preserves the external image references. Smaller, but dependent on the original images.
- [PROPOSED] Each reference stores: original absolute path, file name, file size, modified time, and a content identity (SHA-256 of the file bytes; optionally also pixel dimensions from the header) used for identity validation at open time.
- [PROPOSED] Thumbnails are still embedded so the dashboard can present itself, but opening is blocked until references validate (section 8).

## 5. Internal binary container architecture [PROPOSED]

- Fixed header: magic bytes (RESORA-specific, not `PK`), format major/minor version, flags (mode: bundled/linked), header length, header checksum.
- Resource directory: entry count, then fixed-layout entries — resource id (flat name, e.g. `layout`, `charts`, `thumb-0001`, `image-0001`), type, offset, stored length, original length, per-resource checksum (SHA-256). Names are validated identifiers, never paths.
- Resource payloads: concatenated, length-prefixed. JSON resources may be compressed (zlib); image payloads are stored as-is (already compressed).
- Trailer: directory offset, whole-file checksum, end magic — lets a reader detect truncation immediately.
- Byte order: little-endian, fixed-width integers (explicit sizes, 64-bit offsets).
- Writer: writes to `<target>.tmp` in the same folder, flushes, verifies by re-reading the directory and checksums, then atomically renames over the target (same pattern as `QSaveFile` in `LocationStore` and `scripts/engine/reports/atomic.py`). A failed or cancelled export never leaves a partial `.resora` under the final name.
- Reader: random access via the directory; reads only requested resources; validates every offset/length against file size before use.
- Forward compatibility: unknown minor version fields ignored; unknown major version rejected with a clear message; unknown resource types preserved/skipped, never fatal.

## 6. Resource and JSON schema design [PROPOSED]

Flat resource ids. Core JSON resources (UTF-8, each carries `schema_version`):

| Resource id | Content |
|---|---|
| `manifest` | format version, app version, mode, created timestamp, document id, resource inventory summary, source description (folder / file list), image count |
| `layout` | dashboard view state: active view (Dashboard / Detailed), widget arrangement and visible sections |
| `charts` | chart definitions and the chart-ready data needed to draw them |
| `analytics` | the engine's `analytics` object as returned (see `docs/ANALYTICS.md`), stored unmodified |
| `config` | analysis options used (recursive, report options, etc.) |
| `images-index` | one record per image: stable image id, name, metadata (format, width, height, file size, status), thumbnail resource id, and either bundled image resource id (Bundled) or reference + identity (Linked) |
| `thumb-NNNN` | thumbnail bytes (PNG/JPEG, size-capped) |
| `image-NNNN` | original image bytes (Bundled mode only) |

Schema rules: explicit `schema_version` per JSON resource; unknown fields ignored; missing optional values are `null`, never `0` (consistent with the project-wide rule in `docs/ANALYTICS.md`); the analytics payload is the engine's own object, so no statistic is recomputed by the format layer.

---

## 7. Import and opening workflow

1. User chooses a `.resora` file (open dialog, drag-and-drop, or recent).
2. Container opened read-only; header, trailer and directory validated (magic, versions, bounds, directory checksum). Failure → clear error, nothing else touched.
3. `manifest` read; mode determined; unsupported major version rejected.
4. JSON resources parsed and schema-validated.
5. **Bundled:** images extracted to app-managed cache with per-file checksum verification; paths in the in-memory document point to the cache.
   **Linked:** every reference validated (section 8). Opening is blocked until all pass.
6. The dashboard model is rebuilt from `analytics` + `layout` + `charts` and handed to the existing dashboard presentation. The file layer never touches widgets.
7. On close: cache entry for that document removed; originals never written.

## 8. Missing-image validation and relinking behavior

- [DECISION] Block dashboard opening if any required linked image is **missing, unreadable, or fails identity validation**.
- [DECISION] Show the affected **filenames and paths**.
- [DECISION] Allow users to locate/relink images.
- [DECISION] **Revalidate all references before opening** (not only the relinked ones).
- [DECISION] Do not silently accept replaced or modified images.
- [PROPOSED] Validation states per reference: `ok`, `missing`, `unreadable`, `identity_mismatch` (size and/or SHA-256 differ). The list is presented in a dialog with Locate (single file), Locate folder (match remaining by file name, then identity-check each), and Cancel.
- [PROPOSED] A relinked file is accepted only if its identity matches the stored one; a same-named but different file is reported as a mismatch, never accepted. Relinks apply to the open session; saving updated references is a separate, explicit user action (never overwrite the source `.resora` silently).
- [PROPOSED] Validation reads files read-only and streams hashing off the GUI thread with progress and cancel.

## 9. Security and integrity requirements

- All paths/ids from the file are untrusted: resource ids restricted to a safe character set; no path separators, no `..`, no drive letters; extraction target built only from a generated name inside the cache folder.
- Every offset/length bounds-checked against file size; integer overflow guarded; maximum counts and sizes enforced (resource count, JSON size, thumbnail size) to prevent decompression/allocation bombs.
- Per-resource and whole-file SHA-256 verified before use; any mismatch fails the open with a specific reason.
- Linked image paths are references only: they are validated and opened read-only, never executed or used to write anywhere.
- Export is atomic (temp + verified + rename); extraction is atomic per file and cleaned on failure.
- Original images are never modified, moved or deleted by export, open or relink.
- No secrets in the file; absolute paths in Linked mode are disclosed to the user in the export dialog.

---

## 10. Seven implementation phases

Each phase lists: Objective, Scope, Expected files/modules, Dependencies, Implementation requirements, Testing requirements, Completion criteria. File names are **[PROPOSED]**; C++ vs Python placement is decided in Phase 1 (section 13).

### Phase 1: File Format Foundation
- **Objective:** a standalone, UI-independent library that reads and writes the `.resora` container.
- **Scope:** header/directory/trailer layout, versioning, checksums, bounds-checked reader, atomic writer, resource id validation. No dashboard knowledge; resources are opaque bytes.
- **Expected files:** a new self-contained module, e.g. `gui/include/resora/*.h` + `gui/src/resora/*.cpp` (Qt Core only, no widgets), added to `gui/CMakeLists.txt` [IMPLEMENTED: `resora_format` library, `resorafmt`/`resorareader`/`resorawriter`]. No changes to existing widgets.
- **Dependencies:** none.
- **Requirements:** sections 5 and 9 fully; streaming I/O; typed error codes with messages; deterministic output for identical input (aside from timestamps).
- **Testing:** new `tst_resorafile` via `imgan_add_test`: round-trip, truncated file, flipped bytes, bad magic, future major version, oversized counts, out-of-range offsets, duplicate/invalid ids, atomic write leaves no partial file on failure.
- **Completion:** tests pass; module has no `QtWidgets` dependency; a document can be written and read back byte-identically.

### Phase 2: Bundled Export
- **Objective:** produce a Bundled `.resora` from a completed analysis.
- **Scope:** collect manifest, analytics, layout, charts, config, images-index, thumbnails and original image bytes; write via Phase 1.
- **Expected files:** exporter module beside the Phase 1 library (e.g. `bundledexporter`); thumbnails generated with the read-only `PreviewLoader` approach.
- **Dependencies:** Phase 1.
- **Requirements:** originals opened read-only; progress and cancel; streaming; size estimate and disk-space check before writing; schema per section 6; thumbnails size-capped.
- **Testing:** export a fixture folder, reopen with the Phase 1 reader, compare checksums with sources; cancel mid-export leaves no file; unreadable source reported, not skipped silently; sources unchanged (hash before/after).
- **Completion:** a bundled file exports atomically and verifies; originals byte-identical afterwards.

### Phase 3: Linked Data Export
- **Objective:** produce a Linked Data `.resora`.
- **Scope:** same resources as Phase 2 minus image bytes; add references and identity records (path, name, size, mtime, SHA-256).
- **Expected files:** linked exporter next to the bundled one; a shared document builder for the common parts so both modes build the same dashboard resources.
- **Dependencies:** Phases 1-2 (shared builder).
- **Requirements:** identical `layout`/`charts`/`analytics` content in both modes for the same input; hashing streamed; progress/cancel.
- **Testing:** linked file much smaller than bundled for the same input; common resources equal in both modes; identity records match the files; sources unchanged.
- **Completion:** both modes produce equivalent dashboard resources; the Linked file contains no image bytes.

### Phase 4: Import and Opening Engine
- **Objective:** open either mode into an in-memory document, UI-independent.
- **Scope:** validate container, parse/schema-check JSON, Bundled extraction to app cache, Linked validation and relink logic (section 8), cache cleanup.
- **Expected files:** document opener, link validator and cache manager modules in the same library.
- **Dependencies:** Phases 1-3.
- **Requirements:** sections 7, 8, 9; any non-`ok` reference blocks opening; full revalidation immediately before opening; background threads with progress/cancel.
- **Testing:** round-trip both modes; missing / unreadable / replaced / modified image each blocks; relink with correct and wrong file; traversal-style ids rejected; cache removed on close; corrupted bundle resource fails extraction.
- **Completion:** both modes reconstruct equal document models; every failure path yields a specific, non-silent result.

### Phase 5: Dashboard Integration
- **Objective:** connect the format library to the existing Analytics dashboard.
- **Scope:** an adapter that builds export input from the current `AnalyticsData`/dashboard state and rebuilds the dashboard from an opened document.
- **Expected files:** a thin Qt-Core adapter in `gui/src/backend/` and minimal hooks in `AnalyticsPanel`/`MainWindow`.
- **Dependencies:** Phase 4.
- **Requirements:** the format layer includes no dashboard widgets; no statistic recomputed (engine numbers stored and restored); reopened dashboard equals the exported one.
- **Testing:** export, open, compare `AnalyticsData` field by field; both modes equal; `DashboardModel` `ChartItem` lists equal.
- **Completion:** reopened dashboard is data-identical to the original in both modes.

### Phase 6: Export/Import UI
- **Objective:** user-facing flows.
- **Scope:** Export dialog (mode choice, size estimate, linked-paths notice, destination, progress/cancel), Open flow, missing-image dialog with filenames, paths and Locate/Relink, error messages. Uses the project's frameless dark dialogs and `ChamferButton`.
- **Expected files:** dialogs under `gui/include/widgets/` and `gui/src/widgets/`; wiring in `MainWindow`.
- **Dependencies:** Phases 4-5.
- **Requirements:** dark-only RESORA identity (`Theme`); no blocking calls on the GUI thread; confirm before overwriting an existing file.
- **Testing:** offscreen widget tests (pattern of `tst_dialogs`); `grab_screens` renders inspected by Claude; real use by the user recorded separately.
- **Completion:** the user can export both modes, open both, and resolve missing images entirely through the UI.

### Phase 7: Compatibility and Reliability
- **Objective:** harden the feature.
- **Scope:** version migration policy, corruption tests, large-dataset and low-disk behavior, long-path and Unicode path handling on Windows, cache sweep, crash safety, a `docs/` entry; file association decision.
- **Expected files:** tests and docs mainly; fixes in earlier modules only where defects are found.
- **Dependencies:** Phases 1-6.
- **Requirements:** older/newer file handling per section 5; no leaked cache files; bounded memory on large bundles.
- **Testing:** corruption matrix, large-image-count fixture, cancel at each stage, forward/backward version fixtures.
- **Completion:** reliability suite green; limits documented; user reviews the whole feature.

## 11. Phase completion criteria (all phases)

A phase is complete only when: (1) its scope is implemented and nothing from later phases; (2) its tests exist and passed (command and result recorded); (3) existing tests still pass; (4) `run.bat /nolaunch` succeeds when GUI code changed; (5) `implement.md`, `AgentsMemory/` and `docs/MILESTONE.md` are updated; (6) automated and manual verification are recorded separately; (7) nothing is committed or pushed without instruction. In `docs/MILESTONE.md` a phase stays NOT FINISHED until the user verifies it.

## 12. Testing strategy

- C++ format library: Qt Test executables via `imgan_add_test` in `gui/tests/CMakeLists.txt` (**[CONFIRMED]** pattern: `tst_*.cpp`, offscreen, linked to `imgan_core`).
- Engine (only if Python is involved): `unittest` in `tests/`, run by `run.bat` (**[CONFIRMED]**).
- Fixture images generated in temp folders; hash sources before and after every export/open to prove no modification.
- Negative-first: corruption, truncation, traversal, version skew, replaced images.
- Report automated and manual verification separately; never claim visual verification without inspecting renders.

## 13. Technical decisions and constraints

- Decided by the user: everything marked [DECISION] above.
- Repo constraints (**[CONFIRMED]**): C++17 / Qt 6 Widgets, CMake + Ninja, Qt MinGW; no image-analysis logic in C++; the engine is pure Python with no Qt and talks via NDJSON/QProcess; Qt modules are added explicitly in `gui/CMakeLists.txt` (currently Widgets, Svg, Test); AUTOMOC needs headers under `include/`; modules should stay small.
- **[DECISION 2026-11-08, user] Container is C++ with Qt Core** (resolves the earlier open C++ vs Python question). Reasoning: dashboard state, thumbnails and `QSaveFile` atomicity already live GUI-side, so no QProcess/NDJSON round trip is needed to save or open a dashboard; `QFile`/`QSaveFile`/`QCryptographicHash` cover streaming, atomic replace and SHA-256 without new dependencies; hashing bytes is not image analysis, so it does not violate "no image logic in C++"; it is built as a separate Qt-Core-only library (`resora_format`) so it stays independent of dashboard UI code and testable without widgets. The Python engine stays free of dashboard persistence.
- **[DECISION 2026-11-08] Compression:** Phase 1 stores everything uncompressed; compression code 1 is reserved for an explicit zlib stream in a later phase (never `qCompress` framing). Binary layout and rules: [docs/RESORA_FORMAT.md](docs/RESORA_FORMAT.md).
- Not decided: file association/icon, recent-files list. Relative paths in Linked mode: decided in Phase 3 (docs/RESORA_FORMAT.md, "Path-resolution policy").

## 14. Current implementation status

| Phase | Status |
|---|---|
| Phase 1: File Format Foundation | COMPLETED (formally approved by the user 2026-11-08) |
| Phase 2: Bundled Export | COMPLETED (approved by the user 2026-11-08) |
| Phase 3: Linked Data Export | COMPLETED (approved by the user 2026-11-08) |
| Phase 4: Import and Opening Engine | COMPLETED (approved by the user 2026-11-08) |
| Phase 5: Dashboard Integration | COMPLETED (approved by the user 2026-11-09) |
| Phase 6: Export/Import UI | COMPLETED (approved by the user 2026-11-10) |
| Phase 7: Compatibility and Reliability | IN PROGRESS (started 2026-11-10) |

### Confirmed repository facts used (inspected 2026-11-08)
- `AnalyticsData` / `AnalyticsModel::fromJson` (`gui/include/backend/analyticsmodel.h`): Qt-Core struct parsed from the engine's `analytics` object. `DashboardModel` (`dashboardmodel.h`) turns it into `ChartItem` lists; `AnalyticsPanel` / `AnalyticsDashboard` render it. Analytics results are not persisted anywhere today.
- Engine `ImageRecord` (`scripts/engine/imaging/records.py`): path, status, format, width, height, file_size, resolution; header-only reads.
- `PreviewLoader` (`previewloader.h`): read-only, size-capped Qt decoding, usable for thumbnails.
- `LocationStore` uses `QSaveFile`, versioned JSON and read-only mode for newer versions (precedent); `scripts/engine/reports/atomic.py` is the engine-side atomic-write precedent.
- `scripts/engine/duplicates/` uses hashing and a SQLite cache in the engine; no GUI-side hashing exists.
- No `.resora` code, no `QCryptographicHash` use in the GUI, and no dashboard export/import exists. The feature is entirely [PROPOSED].

### Phase 1 evidence (2026-11-08)
- Implemented: `gui/include/resora/{resorafmt,resorareader,resorawriter}.h`, `gui/src/resora/*.cpp`, `resora_format` target (Qt Core only), `gui/tests/tst_resorafile.cpp`; spec in docs/RESORA_FORMAT.md.
- Automated: `ctest -R tst_resorafile` passed (see docs/MILESTONE.md for the exact record). Covers round-trip (incl. 3 MiB streamed file, empty and unknown-type resources), determinism, atomic write, bad magic, truncation, version, header/dir/file/resource checksums, flipped bytes, bad offsets/sizes/overflow/overlap, duplicate/invalid ids, oversized counts, unsupported compression.
- Review round 1 (2026-11-08): whole-file hash defined precisely (SHA-256 over bytes `[0, N-40)`, excluding the hash field and end magic; non-circular) in docs/RESORA_FORMAT.md with new tests; writer validates layout, flush and exact output size before `commit()`; formal versioned JSON Schemas (dashboard/manifest+layout, charts, settings, analytics, metadata, image-references) added under `docs/schemas/resora/` (documentation only; `x-phase` fields depend on later phases). Section 6 table is the resource overview; the schema files are authoritative for fields.
- Automated: `run.bat /nolaunch` OK, ctest 13/13 passed (incl. `tst_resorafile`). Not done: no manual verification by the user; user approval pending, so Phase 1 stays IN PROGRESS.

### Phase 2 evidence (2026-11-08)
- Implemented: `gui/include/resora/bundledexport.h`, `gui/src/resora/bundledexport.cpp` (in `resora_format`), additive `Writer::write` progress/cancel callback and `Error::Cancelled`, `gui/tests/tst_bundledexport.cpp`; API and resource mapping in docs/RESORA_FORMAT.md ("Bundled Export").
- Automated: `ctest -R "resorafile|bundledexport|duplicates|analytics"` 4/4 passed; `tst_bundledexport` 11/11. Not run: full ctest / `run.bat` (auto-commit risk). Manual: none. Status stays IN PROGRESS until the user approves.
- Final verification pass (2026-11-08, after review): **full ctest 14/14 suites passed, 0 failed, 0 skipped; 184 test functions passed** (per-suite QtTest totals). `tst_bundledexport` now 15/15. New tests: source changed mid-export (same-size content change with mtime restored, and size change) -> SourceProblem/Changed, previous destination intact; destination held open (Windows) -> FinalizeFailed, previous kept, no leftovers; cancel at Validating/Writing/Verifying keeps an existing destination; progress monotonic, 100% only at `Stage::Done` after the swap. Additive change: `ExportProgress::permille` + `Stage::Done`.
- Windows replacement (superseded 2026-11-08, crash-safety fix): the earlier two-rename swap is gone. Now isolated in `gui/{include,src}/resora/destinationfinalizer.*` (`finalizeDestination`): flush temp, then `MoveFileExW(tmp, dst, 0)` for a new destination (no overwrite flag, same volume); if it reports the destination exists, `ReplaceFileW(dst, tmp, backup, IGNORE_MERGE_ERRORS)` which swaps in one OS call and keeps the old content under a `.tmp` backup until success, then the backup is removed (best effort). Sharing/lock/access-denied errors retried 6x (50-250 ms backoff), then `FinalizeFailed` with the previous file intact; temp removed by the caller. Non-Windows: `rename(2)`. Limits: not durable-fsync of the directory entry (`REPLACEFILE_WRITE_THROUGH` unsupported by Windows); a crash can leave a stray `*.tmp`, never a missing/partial destination; ReplaceFileW is not atomic on every filesystem (network shares). Source-change detection: Writer hashes the bytes it streams, verify compares size+SHA-256 to the validation pass, plus an mtime/size recheck.
- Test-runner note: QtTest prints nothing to a piped stdout in this MinGW/Git-Bash setup, so `... | grep Totals` yields empty output and exit 1 (grep's status; the test exe itself exits 0). Use `-o file,txt` for counts or `ctest`. Cause identified from observation, not from the earlier run itself.
- Replacement fix verification (2026-11-08): `tst_bundledexport` 21 passed (6 new finalizer tests: new destination, replace without leftovers, locked destination keeps previous + temp, transient lock released mid-retry succeeds, read-only destination keeps previous, missing temp / folder destination); full ctest 14/14 suites passed, QtTest totals 190 passed / 0 failed / 0 skipped. No manual verification; `run.bat` not run; nothing committed.
- ReplaceFileW failure-handling review (2026-11-08, user-requested): Microsoft's page shows my earlier claim "every failure keeps the original name" was WRONG for 1177 (replaced file lives under the backup name, destination path can be missing) and the caller still deleted the temp file. Fixed: `finalizeWith` observes {target, backup, tmp} after a failed `ReplaceFileW`; clean state -> clean failure; 1177 shape -> non-overwriting restore of the backup; anything uncertain -> `FinalizeStatus::NeedsAttention` + `keepTemp` (nothing deleted/moved, no retry, message lists paths); `exportBundled` skips temp cleanup when `keepTemp`. Documented-vs-assumed split: docs/RESORA_FORMAT.md ("Destination finalization"). New tests via the `detail::FinalizeOps` seam: 1175, 1176, 1177 restorable, 1177 restore failed (both preserved, one call only), unexpected states a/b/c, success removes only the backup, transient error retried. `tst_bundledexport` 27 passed; full ctest 14/14, QtTest totals 196 passed / 0 failed / 0 skipped. Real 1175-1177 cannot be provoked from user mode (simulated). No manual verification; `run.bat` not run; nothing committed.
- Approved by the user 2026-11-08 (COMPLETED).

### Phase 3 evidence (2026-11-08)
- Implemented: `gui/{include,src}/resora/linkedexport.*` (`exportLinked`), `exportcommon.*` (shared hashing + common JSON resources; Bundled refactored to use it, behaviour unchanged), `gui/tests/tst_linkedexport.cpp`; spec in docs/RESORA_FORMAT.md ("Linked Data Export"), `image-references.schema.json` linked record finalised.
- Automated: `tst_linkedexport` 10/10 passed; full ctest 15/15 suites passed (incl. `tst_bundledexport`, `tst_resorafile`). Covers multi-image export, original bytes absent, references + thumbnails, duplicate names, missing/unreadable/modified sources, absolute/relative/document-relative paths, 1500-image collection, cancel at 3 stages, existing destination preserved, common resources equal to Bundled, reader round-trip/integrity. Not run: `run.bat`. Manual: none. Not committed. Stays IN PROGRESS until the user approves.
- Phase 3 review fix (2026-11-08, user-requested): final source-identity verification. Before finalizing, `exportLinked` re-reads and rehashes every source and compares size + SHA-256 with the validation-time values (mismatch/unreadable -> `Changed` issue, `SourceProblem`, destination preserved, temp removed); cancellable per chunk; progress by bytes in the Verifying window (monotonic, 1000 only after finalization). Bundled unchanged. New tests: same-size edit + restored mtime after the initial hash is detected with the previous destination intact; cancel during the rehash. `tst_linkedexport` 12/12, full ctest 15/15. No `run.bat`, no manual verification, nothing committed. Phase 3 stays IN PROGRESS; Phase 4 not started.

### Phase 4 evidence (2026-11-08)
- Implemented: `gui/{include,src}/resora/{resoraimport.h,importinternal.h,importparse.cpp,linkresolve.cpp,bundledextract.cpp,importdocument.cpp}` (in `resora_format`, Qt Core only), `gui/tests/tst_resoraimport.cpp`; spec in docs/RESORA_FORMAT.md ("Import and Opening (Phase 4)"). Phases 1-3 code unchanged.
- Automated: `tst_resoraimport` 16 passed / 0 failed (14 test functions); full `ctest` 16/16 suites passed. Built directly with ninja/ctest; `run.bat` not run. Manual/GUI: none. Nothing committed.
- Limits: no stale-cache sweep (Phase 7); single-image copy cancellable only between images; linked open rehashes every file each time.

### Phase 5 evidence (2026-11-08)
- Implemented: `gui/{include/backend/dashboardstate.h,src/backend/dashboard{serialize,restore,session}.cpp}` (Qt Core only, in `imgan_core`, which now links `resora_format`), read-only `AnalyticsPanel::result()`, `gui/tests/tst_dashboardstate.cpp`; mapping in docs/RESORA_FORMAT.md ("Dashboard Integration (Phase 5)"). Note: the dashboard holds no full image list; its image associations are the four extreme images. No schema/format change, no UI.
- Automated: `tst_dashboardstate` 12 passed; full ctest 17/17. `run.bat` not run. Manual: none. Nothing committed. Stays IN PROGRESS for review.
- Tracker note (superseded 2026-11-08): this line previously said Phase 3 was left IN PROGRESS; the user has since confirmed Phase 3 as approved, so the tracker now shows COMPLETED.
- Final-review correction A (2026-11-08): `AnalyticsPanel::showRestored(result, pathByRole, &err)` + `dashboard::runtimePaths(Restored)`. Persisted analytics JSON (`result()`) is unchanged; the four extreme-image rows (largest/smallest/highest/lowest) use the Phase 4 validated paths (Bundled: extracted cache; Linked: validated/relinked originals). `restore()` now fails if an image has no readable resolved file; a missing role path makes `showRestored` fail with the panel untouched (no fallback to stale JSON paths). Tests added: bundled after source dir deleted, linked via relink, unresolved/missing. `tst_dashboardstate` 15 passed (incl. init/cleanup), full ctest 17/17 (direct cmake/ninja). Phase 5 stays IN PROGRESS; no manual/GUI verification; nothing committed.

### Phase 6 evidence (2026-11-09)
- Implemented: `gui/{include,src}/backend/{resoraworkflow,resorajobs}` (destination check, estimate, ExportJob/ImportJob on QThreadPool; destructor cancels and waits), `widgets/{exportdialog,openresoradialog,relinkpanel,confirmdialog}`, MainWindow wiring (Ctrl+Shift+E / Ctrl+Shift+O, Analytics page actions), `gui/tests/tst_resoraui.cpp`. No format/engine change.
- Fix found while testing: relinks that already passed are kept across retries (`accepted_`), so only still-failing images are re-asked.
- Automated: `tst_resoraui` 14 cases passed; full ctest 18/18 (direct cmake/ninja; `run.bat` not run). Manual/visual: none by Claude; not inspected on screen. Nothing committed. Stays IN PROGRESS pending user review.

### Phase 6 correction: relinking table and Remove from Dashboard (2026-11-10)
- UI: `RelinkPanel` is now a table (Image, Used as, Status, Actions) listing ALL failing images at once; statuses Missing / Wrong Image / Resolved / Removed from Dashboard, plus an interim "Replacement chosen" (selected, not yet verified by the engine). Actions: Choose/Change Replacement, Clear, Remove from Dashboard (confirmation dialog). Table state is the Qt-Core `resoraui::RelinkModel` (`backend/relinkmodel.*`).
- API changes (additive; default behavior unchanged): `OpenOptions::omitImages` (Linked only, per call, unknown id or Bundled => InvalidInput, omitted images are not checked and are absent from `document.images`; every other image still blocks), `LinkState::Omitted`, `LinkStatus::roles`, `OpenedDocument::omittedImageIds`; `dashboard::State::removedRoles`, `Restored::removedRoles`, `AnalyticsPanel::showRestored(..., removedRoles)` / `removedRoles()`. Export writes optional `layout.omitted_roles` (no widget, no image for a removed role); restore accepts a missing widget only for roles listed there or omitted in this open. Analytics JSON is never edited. Format `schema_version` unchanged.
- Automated: `tst_dashboardstate` + `tst_resoraui` extended (omission scoped/never silent, one/all removed, remaining associations, no stale paths, source `.resora`/analytics/original images unchanged, re-export keeps removal for Linked and Bundled, wrong replacement rejected, accepted replacements kept across retries, Clear restores unresolved). `tst_resoraui` 18 passed; full ctest 19/19 (direct cmake/ninja; `run.bat` not run). Manual/visual: none; table not inspected on screen.
- Phase 6 stays IN PROGRESS. Nothing committed.

### Phase 6 final review: re-export source and `omitted_roles` compatibility (2026-11-10)
- **Finding (A):** before this change export read image paths from the persisted analytics JSON (`im.path`), so a Bundled dashboard opened after its source directory was deleted could not be re-exported (Plan would point at stale paths). **Fix (small, additive):** `dashboard::State` gained `restored` + `runtimePaths` (role -> validated file); `Restore` fills them, `AnalyticsPanel` exposes `isRestored()/runtimePaths()` and `MainWindow::exportDashboard` passes them. When `restored` is true `plan()` takes image files ONLY from `runtimePaths` and fails for a remaining role without one (no stale-path fallback). `State::operator==` still compares only the logical state. Persisted analytics JSON and the opened `.resora` stay unchanged. Bundled re-export reads the extracted cache copies (valid while the session is active); Linked re-export uses the validated/relinked paths; removed roles stay removed.
- **Tests added (`tst_dashboardstate`, now 23 passed):** `bundledReExportWorksAfterSourceDirectoryDeleted` (export Bundled, delete source dir, open, re-export Bundled, new file opens, bytes of all images intact, original document unchanged), `restoredExportNeverFallsBackToStalePaths`, `linkedReExportAfterRelinkKeepsRemovals` (relink one, remove one, re-export Linked, reopen, removals kept, paths = relinked), `omittedRolesFieldIsValidated`. Full ctest 19/19 (direct cmake/ninja; `run.bat` not run).
- **Limit:** the re-export of a Bundled dashboard works only while its session (cache folder) is active; the cache is removed when the session is replaced/closed (by design).
- **`layout.omitted_roles` (B):** schema entry added (`dashboard.schema.json`: array, unique items, enum of the 4 roles, optional, null invalid). Reader: must be an array when present (null/other types rejected), each item must be a string and an allowed role, duplicates rejected, a role listed both as widget and omitted rejected. *(Superseded 2026-11-12: the layout `schema_version` is now 2 when `omitted_roles` is present; see "Phase 7 evidence: omitted_roles compatibility and schema policy". The statements above about "stays 1" and "not verified" describe the state on 2026-11-10 only.)*
- Phase 6 stays IN PROGRESS. Nothing committed. Phase 7 not started.

### Phase 7 evidence: compatibility and reliability (2026-11-10)
- **Audit findings:** (1) `schema_version` was checked with `toInt(-1) != 1` (accepted `1.0`, no newer/older/invalid distinction) and the charts schema was not checked - now one policy function with specific messages. (2) Duplicate resource-id checks were O(n^2) in reader and writer - now hashed. (3) No cleanup of crash-orphaned `doc-*` folders - conservative startup sweep added. (4) Extraction used the untrusted display-name extension (any <=5 alnum chars, e.g. `exe`) - now a fixed image-extension list else `.img`; Windows device-name stems prefixed. (5) Cache marker held only the document id (no owner) - now JSON with pid; legacy markers still recognised. (6) The sweep's first file-name check accepted any `<id>.<ext>` (a failing test showed `notes.txt` qualified) - tightened to image extensions. No defect found in reader bounds checks, the finalizer, export rehash or worker lifecycle (reviewed; Phase 1-6 tests already cover them).
- **No format change, no migration, no breaking change.** Container 1.0 and all schemas stay at 1. Policy: `docs/RESORA_FORMAT.md` (Phase 7 section).
- **Tests added:** `tst_resorareliability` (9: sweep removes only dead-owner intact folders; keeps active/young/unmarked/foreign/garbage-marker/unexpected-content/lookalike-name folders; legacy marker age rule; missing root; junction never followed, also as cache root; open session untouched; hostile extensions; 25 open/replace/close cycles), `tst_resorafile` +4 (minor version/flags, unknown resource type, 20000-resource stress, hostile directory entries incl. overflow), `tst_dashboardstate` +1 (schema-version and unknown-field policy).
- **Automated (direct cmake/ninja, no run.bat):** full ctest 20/20; tst_resorafile 27, tst_resorareliability 9, tst_dashboardstate 24, tst_resoraimport 17, tst_bundledexport 27, tst_linkedexport 12, tst_resoraui 18, tst_mainwindow 7 passed.
- **Not done / not verified:** no manual or visual verification; no full-disk, network-share, removable-drive, antivirus or power-loss testing; non-Windows branch not run. Phase 7 stays IN PROGRESS pending review. Nothing committed.

### Phase 7 evidence: continuation (2026-11-12)
- **Added:** `unicodeAndLongPathRoundTrip` in `tst_resorareliability` (non-ASCII names, >200-char path, export/open/byte compare, cache removed). The first version failed because the test kept a `QFile` handle open, which blocked cache removal; the handle is now scoped in the test. Product code was not changed.
- **Automated (direct cmake/ninja, Qt DLLs on PATH; `run.bat` not run):** `tst_resorareliability` 10/10; full ctest 20/20.
- **Cleanup pass (2026-11-12):** fixture `schema_version` values asserted explicitly (layout 1 / 2, analytics, config, manifest 1); re-export test compares role -> image filename for every remaining role; long-path comment corrected to measured lengths (folder ~266, target ~283 UTF-16 units with a typical temp base; no product code change). Validation layers documented in docs/RESORA_FORMAT.md.
- **Still open (need user decision or real fixtures):** file association decision; real low-disk test. The `omitted_roles` older-reader item is resolved (see above). Details: docs/RESORA_FORMAT.md "Open Phase 7 items".
- **Status:** Phase 7 stays IN PROGRESS pending review. Nothing committed.

### Phase 7 evidence: omitted_roles compatibility and schema policy (2026-11-12)
- **Real Phase 5/6 reader:** `git archive a467857^` (the commit before `omitted_roles` existed; pre-Phase 7 `schema_version` checks) was built in the scratchpad with a small probe (`openResora` + `dashboard::restore`), not a simulated reader. Results: v1 fixture restores (0 warnings); v2 fixture (`omitted_roles`, layout schema 2) is rejected with "layout has an unsupported schema_version". The v1-shaped-but-with-removals case was not produced as a file, so its message is by code reading only.
- **Decision:** layout `schema_version` 2 is written only when `omitted_roles` is non-empty; the reader accepts 1 and 2; `omitted_roles` requires 2. No migration (additive; no rewrite on read). Charts/analytics/config stay at 1.
- **Tests (direct build, Qt on PATH):** `tst_dashboardstate` 27 passed, 1 skipped (fixture regeneration, env-gated); new: `compatibilityFixturesRestore`, `omittedRolesNeedVersionTwo`, `reexportPreservesVersionAndAssociations`; `versionAndUnknownFieldPolicy` now uses 3 as the newer value for layout. Full ctest 20/20.
- **Fixtures:** `gui/tests/fixtures/dashboard/v1_no_omitted_roles.resora`, `v2_omitted_largest.resora` (generated by the current writer, synthetic pattern images, ~12 KB each).
- **Not done:** file association (untouched, by instruction); no run.bat, no commit; Phase 7 stays IN PROGRESS. Old-reader probe is a scratchpad tool, not part of the repo.

### Phase 7 evidence: low-disk reliability, Bundled and Linked export (2026-11-12)
- **Audit findings (before the change):** export had **no up-front free-space check** (only extraction did); the Writer had **no failure seam**, so storage exhaustion could not be tested deterministically; no test covered exhaustion, silent truncation or an uncertain replacement during export. Preservation of an existing destination, temp cleanup and progress ordering were already implemented.
- **Changes:** Writer: `containerBytes()`, `setByteLimit()` (simulated capacity; over-limit writes fail as `Error::OutOfSpace`, appended to the enum), `budgetOk` on every write. Export: pre-check `containerBytes + 16 MiB` against the destination volume before writing (`ExportError::InsufficientStorage`, appended), `OutOfSpace` mapped to it. Seams in `gui/include/resora/exportseams.h`; `exportBundledWith`/`exportLinkedWith` take them, public functions use defaults. Behaviour for normal use is unchanged.
- **Tests:** new `tst_exportlowdisk` (16 cases, registered in `gui/tests/CMakeLists.txt`), deterministic injection only. Bundled: insufficient space before writing; exhaustion mid-payload; exhaustion in the trailer; silent truncation caught by verification; clean replacement refusal; uncertain replacement (1177 + failed restore) keeps every surviving file; a later success leaves only the destination. Linked: needs only container space (same tight volume that refuses Bundled succeeds); insufficient space; exhaustion during write; verification; replacement refusal; uncertain replacement; distinct codes for cancel/space/verify.
- **Not done / not verified:** no real full disk, no removable drive or network share, no antivirus lock during replacement, no power-loss test, and no classification of a real ENOSPC (it surfaces as `WriteFailed`). Phase 7 stays IN PROGRESS pending review. Nothing committed.
- **Full CTest (direct cmake/ninja, Qt on PATH):** 21/21 suites passed, including `tst_exportlowdisk` (16 cases), `tst_bundledexport`, `tst_linkedexport`, `tst_resorareliability`, `tst_dashboardstate`. `run.bat` not run.

### Phase 7 evidence: low-disk audit corrections (2026-11-12)
- **Finalizer flush (Windows):** `FlushFileBuffers` and its open are now checked before any move; failure returns `FinalizeStatus::Failed` ("previous file was kept"), nothing moved, backup logic untouched. Not unit-tested (no seam for the OS flush); power-loss safety is not claimed.
- **Seams internal:** `Writer::setByteLimit` removed from the public API (private, set via `detail::WriterLimit` in `exportseams.h`); `exportBundledWith`/`exportLinkedWith` moved to the internal `exportinternal.h`. Public headers no longer expose them.
- **Automated:** build OK; focused ctest 7/7 (`tst_resorafile`, `tst_bundledexport`, `tst_linkedexport`, `tst_resoraimport`, `tst_resorareliability`, `tst_dashboardstate`, `tst_exportlowdisk`). Full suite not re-run; `run.bat` not run.
- **Still open (not changed):** source growth after the free-space check can exceed the estimate (bounded by the 16 MiB reserve); real ENOSPC reports `WriteFailed`; test-isolation/missing-case gaps listed in the audit. Phase 7 IN PROGRESS; nothing committed.
- **Update 2026-11-12 (supersedes the three "not unit-tested / still open" statements above):**
  - **Source growth:** the free-space check is a snapshot, not a guarantee; the 16 MiB reserve is only a margin (it also covers old+new destination coexisting during replacement). Before this change the Writer streamed each source to EOF, so a source that grew after the estimate was caught only by post-write verification. Now Bundled caps the Writer byte budget at the preflight estimate (a test capacity can only lower it): an unchanged container is exactly that many bytes, so growth aborts the write early and, because a changed source is detected via size/mtime, is reported as `SourceProblem`/`Changed` (not as a space error); temp removed, destination untouched. Linked never streams source bytes, and its final rehash already rejects growth. Concurrent growth is still not prevented (only detected); real exhaustion remains a clean failure (`WriteFailed`).
  - **Flush seam:** `detail::FinalizeOps::flush` (internal, no public API, no global state) wraps open + `FlushFileBuffers`; `finalizeWith` runs it before any move. New tests prove open failure and flush failure move nothing (old destination byte-identical, temp preserved, no backup) and a successful flush continues into the replacement.
  - **Tests:** `tst_exportlowdisk` now one fresh `QTemporaryDir` per test (`init`/`cleanup`); added growth-after-preflight (Bundled, Linked), growth-during-write (Bundled), uncertain replacement keeping both backup and temp (Bundled, Linked), 3 flush-seam tests: 22 cases, 24 passed incl. `initTestCase`/`cleanupTestCase`. Focused ctest 6/6 then full ctest 21/21 (direct cmake/ninja, Qt on PATH); `run.bat` not run; nothing committed; no manual verification.
  - **Still open:** real ENOSPC reports `WriteFailed`; power-loss atomicity, real full disk, network/removable drives not tested. Phase 7 IN PROGRESS.

### Phase 7 evidence: final low-disk audit corrections (2026-11-12)
- **Git state (supersedes every "nothing committed" line in the Phase 7 evidence sections above; those stay as history):** Phase 7 implementation = commit `954c4fb`; low-disk audit follow-up = commit `de8bedb`. This round's changes (tests only, `gui/tests/tst_exportlowdisk.cpp`) are uncommitted.
- **Budget cap proven:** `bundledSourceGrowingAfterPreflightIsRejectedBeforeOverrun` and `bundledSourceGrowingDuringWritingIsRejected` set an `afterWrite` hook flag and assert it is never reached, no `Verifying`/`Finalizing`/`Done` progress, permille < 1000, `SourceProblem` with one `Changed` issue, old destination byte-identical. Mutation check: with the budget cap temporarily removed both tests fail on the hook assertion (source restored afterwards). Post-write verification is untouched.
- **Progress:** recorders (no `Done`, permille < 1000) added to Bundled trailer exhaustion, both uncertain-replacement-keeps-temp cases, Linked growth, and the new flush test. Finalizer-level tests were left alone.
- **Export-level flush test:** `bundledExportFlushFailureKeepsDestinationAndRemovesTemp` injects a failing `FinalizeOps::flush` through the existing `seams.finalize` seam (no new API/global state): `FinalizeFailed`, nothing moved, old destination byte-identical, temp removed by export cleanup (finalizer returns `keepTemp=false`), no `Done`. Finalizer-level flush tests are retained (they show the temp is kept by the finalizer itself).
- **Automated:** `tst_exportlowdisk` 25 passed (23 cases + `initTestCase`/`cleanupTestCase`); full ctest 21/21 (direct cmake/ninja, Qt on PATH). `run.bat` not run; no manual verification. Phase 7 IN PROGRESS.


## Phase 7 - Linked path security and cache safety (2026-11-12) [IN PROGRESS]
- **Findings:** (1) `QDir::isAbsolutePath` accepted UNC/`\?\` paths, so recorded paths, relinks, `hint_path`/`imageRoot` bases reached `QFileInfo::exists`/hash -> automatic SMB access. Also `C:x` counted as absolute. (2) `closeDocument` used `removeRecursively` with only a name prefix + marker-exists check (no junction check, no marker validation, deleted foreign files); `abortWith` same; `Session::close()` ignored the result.
- **Changes:** `isLocalAbsolutePath` applied in `linkresolve.cpp` (recorded, relink, hint/imageRoot; docDir exempt); `removeOwnedCacheDir` (`cachesweep.cpp`) used by sweep, closeDocument, failed-open cleanup; marker deleted last; `Session::close()` -> bool, `pendingCleanup()`, `retryCleanup()`. No format change. Policy: docs/RESORA_FORMAT.md "Linked-path security policy and cache ownership".
- **Behaviour change:** Linked documents whose recorded path / hint_path is a UNC or device path now open as `UnsafePath` (user can relink to a local file). Bundled close now keeps a folder containing foreign entries instead of deleting it.
- **Tests:** `tst_resoraimport` 21/21 (+4: local-path predicate, UNC recorded path, UNC/relative relink without fallback, UNC hint/imageRoot), `tst_resorareliability` 16/16 (+6 closeDocument cases incl. real junctions, foreign file/dir, marker states, failed cleanup + retry via read-only file), `tst_dashboardstate` 28 passed/1 pre-existing skip (+ Session pending/retry). Full ctest 21/21 (direct cmake/ninja, Qt on PATH). Junction tests ran (mklink /J needs no privilege). Not tested: real SMB share, symlinks needing privilege, race conditions. run.bat not run; no manual verification.

## Phase 7 audit follow-up - mapped drives, cache race, pending cleanup (2026-11-12) [IN PROGRESS]
- `isRemoteDriveLetter` (linkresolve.cpp) feeds `isLocalAbsolutePath`; `DirLock` (cachesweep.cpp) held in `removeOwnedCacheDir`. Pending-list loss on Session destruction is an accepted, documented gap (startup sweep fallback). Details: docs/RESORA_FORMAT.md "Linked-path security policy".

## Phase 7 - Application-level cache cleanup ownership (2026-11-12) [IN PROGRESS]
- **Lifecycle finding (where ownership was lost):** `Session` kept failed folders in its own `pending_` list. Every path that drops a Session (MainWindow `session_` replacement in `openDashboardFile`, `session_.reset()` in `openInAnalytics` and the analytics refresh, and the Open dialog's worker session when the open or restore fails) destroyed that list with the object. The startup sweep was the only remaining path.
- **Change:** `dashboard::CacheCleanupManager` (`backend/cachecleanupmanager.{h,cpp}`, Qt Core, mutex-protected, path strings only) owns pending folders. `Session(std::shared_ptr<CacheCleanupManager>)` registers failures there; `ImportJob::start` and `OpenResoraDialog` pass the same pointer, so worker-thread sessions register too. MainWindow owns the manager (member, not global) and retries at: replacement, reset points, Open dialog close, and `~MainWindow`. `Session::pendingCleanup()`/`retryCleanup()` now read/retry the shared manager; a session without an argument gets a private one.
- **Ownership rules:** every retry goes through `resora::closeDocument` (the same `removeOwnedCacheDir` close check: `doc-<32 hex>` name, non-link, valid marker, pid must be this process or legacy, every entry a RESORA-named regular file, marker removed last, DirLock). Refused or still-failing folders stay pending and are untouched. `add()` de-duplicates by cleaned path.
- **Tests (`tst_dashboardstate`, +4 cases, 28 passed + 1 skipped):** `sharedManagerKeepsPendingAcrossSessionDestruction` (failed close, session destroyed, pending survives, duplicate add ignored, blocked retry keeps entry, successful retry removes entry and folder, further retry is a no-op); `successfulCloseLeavesNoPendingEntry`; `otherProcessCacheIsNeverRemoved` (marker re-stamped with another pid: refused on every retry, folder and files kept); `invalidMarkerAndUnsafeFoldersStayUntouched` (garbage marker and non-cache folder registered directly: kept, files kept).
- **Automated:** `tst_dashboardstate` / `tst_mainwindow` / `tst_resoraui` passed; full build clean; full ctest 21/21 (direct cmake/ninja, Qt on PATH). `run.bat` not run. No manual GUI verification.
- **Limits (not fixed):** the manager lives only for one application run (no persistence; the startup sweep covers crash/exit leftovers). A folder refused for another process's pid stays in the in-memory list until exit. Retries are synchronous single passes on the GUI thread (a few small folders; DirLock and file removal are the only waits). No timer retry. Race: between a successful retry and a concurrent external writer there is still the pre-existing Windows DirLock/rmdir window. Phase 7 stays IN PROGRESS; file association not started; nothing committed.
- **Superseded:** the "Pending cleanup lifecycle" gap in docs/RESORA_FORMAT.md and the "pending list dies with the Session" limitation in KNOWN_ISSUES.md are resolved by this change.
