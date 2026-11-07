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
- **Expected files:** a new self-contained module, e.g. `gui/include/resora/*.h` + `gui/src/resora/*.cpp` (Qt Core only, no widgets), added to `gui/CMakeLists.txt`. No changes to existing widgets.
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
- **Open decision for Phase 1 (not yet decided; needs user confirmation):** container in C++ (Qt Core: `QFile`, `QSaveFile`, `QCryptographicHash`) versus the Python engine. Recommendation: C++/Qt Core, because dashboard state, thumbnails and `QSaveFile` atomicity already live GUI-side and no IPC is needed. Hashing bytes is not image analysis.
- Compression: choose in Phase 1 (explicit zlib stream, or uncompressed JSON; avoid `qCompress`'s Qt-specific framing for a long-lived format).
- Not decided: file association/icon, recent-files list, relative paths in Linked mode.

## 14. Current implementation status

| Phase | Status |
|---|---|
| Phase 1: File Format Foundation | NOT STARTED |
| Phase 2: Bundled Export | NOT STARTED |
| Phase 3: Linked Data Export | NOT STARTED |
| Phase 4: Import and Opening Engine | NOT STARTED |
| Phase 5: Dashboard Integration | NOT STARTED |
| Phase 6: Export/Import UI | NOT STARTED |
| Phase 7: Compatibility and Reliability | NOT STARTED |

### Confirmed repository facts used (inspected 2026-11-08)
- `AnalyticsData` / `AnalyticsModel::fromJson` (`gui/include/backend/analyticsmodel.h`): Qt-Core struct parsed from the engine's `analytics` object. `DashboardModel` (`dashboardmodel.h`) turns it into `ChartItem` lists; `AnalyticsPanel` / `AnalyticsDashboard` render it. Analytics results are not persisted anywhere today.
- Engine `ImageRecord` (`scripts/engine/imaging/records.py`): path, status, format, width, height, file_size, resolution; header-only reads.
- `PreviewLoader` (`previewloader.h`): read-only, size-capped Qt decoding, usable for thumbnails.
- `LocationStore` uses `QSaveFile`, versioned JSON and read-only mode for newer versions (precedent); `scripts/engine/reports/atomic.py` is the engine-side atomic-write precedent.
- `scripts/engine/duplicates/` uses hashing and a SQLite cache in the engine; no GUI-side hashing exists.
- No `.resora` code, no `QCryptographicHash` use in the GUI, and no dashboard export/import exists. The feature is entirely [PROPOSED].
