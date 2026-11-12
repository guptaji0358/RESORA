# Decisions (accepted / rejected / deferred)

Format: decision — reason — source. Dates are those of the evidence; original discussion dates are mostly [?].

## Accepted
- GUI (C++/Qt) + separate Python engine over QProcess/NDJSON — keeps image logic out of C++, enables later packaging swap. [V docs/ARCHITECTURE.md]
- Header-only metadata reads (no pixel decode) for analysis. [V]
- Per-image problems are data (`status != "ok"`), not job failures; absent metadata = null, never 0. [V ROADMAP]
- Protocol stays v1; new commands additive. [V]
- Roadmap order: Properties → Analytics → Comparison → Duplicates, one milestone per change, each waits for review. [V][U]
- Comparison has **no `compare` backend command** (deviation from roadmap sketch): reuse `properties`, compute deltas in Qt-Core. Previews decoded by Qt only, capped 4096 px, read-only. [V MILESTONE]
- Dashboard charts as custom QPainter widgets. [V MILESTONE]
- Duplicates: detect and compare only; SQLite fingerprint cache keyed by path/size/mtime. [V]
- Brand name RESORA; repo, packages and exe file name keep the old name (`ImageResolutionAnalyzer.exe`) on purpose. [V CLAUDE.md]
- `main.py` is a legacy prototype: do not extend/modify; `image_analyzer.py` must not return. [U][V]
- Milestone records in `docs/MILESTONE.md` only after real evidence; unverified work stays NOT FINISHED. [V]

## Rejected
- Splash screen; Qt Charts; Matplotlib; Windows/Fluent stock look; light theme. (reasons in DESIGN_MEMORY.md)
- MSYS2 `g++` for the GUI build (ABI mismatch with Qt MinGW). [V]

## Deferred
- Nuitka/compiled engine packaging — "separate task later". [V]
- Delete/move in Duplicate Finder — would require explicit confirmation design; not approved. [V]
- Trademark/domain clearance for RESORA — not claimed. [V GUI.md]

## Unknown [?]
Reasons behind some earlier choices (e.g. why phase rail structure) were presumably discussed in conversations not available here.

## 2026-11-07 Independent feature organization [U approved]
- [U] Option 2 chosen: Analyzer/Properties, Analytics, Compare, Duplicates are independent first-class tools, equal in navigation (ToolRail, Ctrl+1..4), each with its own workspace and source; no feature is a tab/step/promo of another. Cross-feature hand-offs allowed via `ToolRequest`/`MainWindow::route`.
- [U] "Approve" answered my open questions; I applied my recommendations: independent sources with a "Use Analyzer import" convenience; Properties inline as an Analyzer step. Supersedes the old 4-phase rail (Source/Scan/Readout/Analytics) and the Compare Back button.

## 2026-11-07 C++ directory reorganization [U approved]
- [U] `gui/` split into `include/` (headers), `src/` (matching .cpp), `main/` (entry point). Subfolders kept. User answered "approve" without choosing the open questions; I applied my recommendations: qualified includes (`"widgets/x.h"`), headers-less split parts (`duplicatefinder_results.cpp`, `comparisonworkspace_ui.cpp`) stay in `src/widgets/`, `grab_screens.cpp` stays in `tests/`.
- [V] AUTOMOC needs headers listed as target sources (`RESORA_HEADERS` glob in `gui/CMakeLists.txt`); without it Q_OBJECT classes fail to link.

## 2026-11-08 `.resora` container in C++/Qt Core [U]
- [U] Phase 1 of the `.resora` dashboard export (see root `implement.md`): the open C++ vs Python question is resolved in favour of C++ with Qt Core. Reasons: dashboard state, thumbnails and QSaveFile atomicity are GUI-side (no IPC); Qt Core covers streaming, atomic replace and SHA-256; byte hashing is not image analysis; separate Qt-Core-only library `resora_format` keeps it independent of dashboard UI.
- [V] Format choices (mine, within the brief): little-endian, SHA-256 everywhere, 64-byte header, 128-byte fixed directory entries, 96-byte trailer, flat ids `[a-z0-9][a-z0-9_-]{0,62}`, uncompressed in v1 (zlib code reserved). Spec: docs/RESORA_FORMAT.md.
- [V] Whole-file hash = SHA-256 over `[0, N-40)` (excludes its own field and end magic; non-circular, no zero-fill). JSON schemas are documentation-only JSON Schema files in `docs/schemas/resora/` (user review 2026-11-08).

## 2026-11-08 Bundled export design [V]
- Exporter lives in `resora_format`, UI-independent. Source identity (SHA-256+size+mtime) taken in a validation pass; write goes to a temp sibling, is re-verified with the Reader, then swapped in. Compression stays unimplemented (code 0). Writer got an additive progress/cancel callback (justified by the cancel requirement; no format change). Destination equal to a source image is rejected. 2026-11-08 [U review]: the two-rename swap was rejected as not crash-safe; finalization is now `destinationfinalizer.*` (`MoveFileExW` for new, `ReplaceFileW` with backup for existing, retry on sharing violations, `rename(2)` elsewhere); the previous destination is never deleted before success. Same day, after a second review: failure handling must not assume ReplaceFileW keeps original names (1177 moves the replaced file to the backup name); the finalizer observes file state, restores only via non-overwriting move, and on any uncertainty preserves every file (`NeedsAttention`, `keepTemp`) instead of deleting.

## 2026-11-08 Linked export final source rehash [U][V]
- User review: hashing sources once misses a same-size edit with restored mtime. Decision: `exportLinked` rehashes every source right before finalizing and compares size + SHA-256 with validation values; mismatch -> `Changed`, destination preserved; cancellable; progress stays monotonic, 1000 only after finalize. Bundled unchanged.

## 2026-11-08 Linked export path policy [V]
- Linked references: absolute path first, then `relative_path` against an explicit base (`folder` image root or `document` folder, only when chosen); never assume the .resora folder is the image folder; relative path only when inside the base (no `..`); identity (size + SHA-256) always decides; duplicate file names flagged, never used as ids. Header flags stay 0 (mode lives in the manifest). Details: docs/RESORA_FORMAT.md.


## 2026-11-08 `.resora` Phase 4 decisions [V]
- Relink is the sole candidate for that image (no fallback), identity always required; every open revalidates all links. Bundled cache = `<cacheRoot>/doc-<uuid>` with `.resora-cache` marker; `closeDocument` deletes only marked folders. Opening blocked (no document returned) if any link fails. Stale-cache sweep deferred to Phase 7.

## 2026-11-08 `.resora` Phase 5 decisions [V]
- Adapter lives in `imgan_core` (Qt Core only); `resora_format` stays dashboard-free. Dashboard image associations = the 4 extreme images (the dashboard keeps no other image list). Charts derived from `DashboardModel`, stored, and must equal the rebuilt ones on open. Non-analytics result keys preserved verbatim in `config.result_context`. Unknown fields -> warnings, unsupported values -> failure. State replaced transactionally via `dashboard::Session`.
- [V] 2026-11-08 Phase 5 review: runtime image paths are a separate representation (`runtimePaths`, `AnalyticsPanel::showRestored`) from the persisted analytics JSON; unresolved images fail restore, no silent fallback to stale paths.

## 2026-11-09 Phase 6 decisions [V]
- Dialogs use test seams (overwrite confirmer); closing while a job runs cancels and closes only after the worker finishes; passed relinks are retained across retries; old session is released only after the new one is shown.

## 2026-11-10 Remove from Dashboard [U request, V implemented]
- Accepted: explicit per-open omission of linked images (not a global bypass); removal = dashboard association only, never deletes the image; removal persisted on re-export via optional `layout.omitted_roles`. Interim status "Replacement chosen" added beside the four requested statuses. No undo-removal action (not requested).

## 2026-11-10 Re-export uses runtime paths [V]
- Restored dashboards export from validated runtime paths (`State::runtimePaths`), never persisted analytics paths; missing runtime path = error. Equality of `State` stays logical. `layout.omitted_roles` stays optional, `schema_version` 1; older-reader compatibility unverified -> Phase 7.

## 2026-11-10 Phase 7 decisions [V, implemented; awaiting user approval]
- Compatibility: container major must be 1, any minor of 1 reads, header flags carried not interpreted (a "required" feature needs a major bump), `schema_version` must equal 1 exactly with specific newer/older/invalid errors, unknown layout/config keys warn. No migrations (no older format exists) - inventing one rejected. Supersedes "older-reader compatibility unverified" for omitted_roles: documented as intended rejection.
- Stale-cache sweep implemented (supersedes "deferred to Phase 7"): only `doc-<32 hex>` real dirs with a valid JSON marker, only generated-shape image files, dead owner pid and >=1 h old (7 days for legacy markers); never recursive, never follows links. Extraction extensions limited to a fixed image list, else `.img`.

## 2026-11-12 Layout schema version 2 for omitted_roles [V]
- Decision: layout `schema_version` is 2 only when `layout.omitted_roles` is present; version 1 files are unchanged. Reader accepts 1 and 2; `omitted_roles` in version 1 is refused. No migration (additive, no rewrite). Evidence: pre-change Phase 5/6 reader (a467857^) restores v1 and rejects v2 with "unsupported schema_version" (real probe). Charts/analytics/config stay at 1.

## 2026-11-12 Linked-path security + cache deletion policy [V][U-requested policy]
- Decision: Linked paths must be local absolute; UNC/device/drive-relative rejected from the string before any filesystem access (recorded path, relink, hint_path, imageRoot). Document's own folder is the only permitted non-local base. Relink = same rules, no fallback to old path. SHA-256 identity unchanged. No format change. Network support, if wanted, must be a separate explicit user option.
- Decision: cache folders removed only via `removeOwnedCacheDir` (name + non-link + valid marker + only RESORA-named regular files, non-recursive, marker deleted last); otherwise preserved. Failed cleanup is reported (`Session::pendingCleanup`) and retryable.
- Why: audit found automatic SMB access and a weakly-checked recursive delete. How to apply: new path-taking code must call `isLocalAbsolutePath`; never `removeRecursively` on cache folders.

## 2026-11-12 Mapped drives and cache race (audit) [V][U-requested policy]
- Decision: reject remote drive letters via local DOS-device table (no remote I/O); document that local symlinks/junctions to network locations are not detected (OneDrive reparse points rule out blanket rejection). Cache deletion holds a no-share-delete, reparse-aware handle (`DirLock`) from validation to rmdir; guarantee and residual local-attacker window are documented in docs/RESORA_FORMAT.md. Rejected: ACL/ownership hardening and handle-based NtDeleteFile (fragile, no benefit vs same-user attacker). Pending cleanup UI deferred; startup sweep accepted as fallback.

## LIAS local automation/knowledge system (2026-11-12)
- [U] User approved building LIAS in `.claude/lias/` (+ `.claude/settings.json` hook, `.claude/skills/lias`). Stage 3 core implemented and unit-tested (29 tests); separate from `.claude/memory/`. Not yet verified in a live Claude session; no credit-savings claims. See `.claude/lias/docs/README.md`.
- [V] LIAS Stage 4 headless live tests (2026-11-12): skill auto-discovery, hook retrieval, store, unknown-fact all worked; cost benefit unproven (n=1). Desktop-app behaviour not tested.
