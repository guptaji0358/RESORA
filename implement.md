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
Phase 6 status: IN PROGRESS (started 2026-11-09).
Phase 7 status: NOT STARTED.

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
| Phase 6: Export/Import UI | IN PROGRESS (started 2026-11-09) |
| Phase 7: Compatibility and Reliability | NOT STARTED |

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
