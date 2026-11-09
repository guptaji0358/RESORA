# Current state (update after each meaningful change)

Last updated: 2026-11-06 (memory system creation). Evidence: `git status`/`git log` snapshot at session start + docs read.

## Git [V]
- Branch `main`, last commit `04c9e02 refactor(gui): UI refinement of layouts, spacing and buttons`.
- Nothing from later work is committed. Nothing was committed or pushed by the memory task.

## Uncommitted work present in the working tree [V]
1. **Analytics Dashboard** (Dashboard + Detailed View, QPainter charts) — documented in `docs/MILESTONE.md` (NOT FINISHED, awaiting user review) and in `commit.txt` last block.
2. **Image Comparison "Insights" redesign (appears in progress)** [V files, ?scope]: new `insightmetrics`, `insightsview`, `insightcanvas`, `metadatacompare`, `dimensiondiagram`, `aspectindicator`, `filesizebars`, `resolutionchart`, `collapsiblesection`, `tst_insights.cpp`; modified `comparison.*`, `comparisontable.*`. Header comment says: Quick Verdict + visual comparisons (resolution, file size, dimensions, aspect, metadata) + collapsible written explanations, presentation only, numbers from `Comparison::metrics`.
   - **Not recorded** in `docs/MILESTONE.md` or `commit.txt`; its verification status, tests run and approval are **[?] unknown**. Do not treat as finished.
3. `AgentsMemory/` and CLAUDE.md memory section (this task).
4. Modified `docs/MILESTONE.md`, `commit.txt` (milestone paperwork).

## Milestone status snapshot (details: MILESTONES.md)
Roadmap 1–4 implemented; Milestones 2, 3, 4, UI Refinement, Analytics Dashboard all "NOT FINISHED — awaiting manual verification" per `docs/MILESTONE.md`. Only the "GUI polish / Saved Locations" milestone is FINISHED.

## Latest recorded automated evidence [R from docs/MILESTONE.md]
Dashboard milestone (2026-11-06): `run.bat /nolaunch` — engine selfcheck, Python tests (128 at Milestone 4), ctest 11/11 passed. Not re-run for the Insights work as far as docs show [?].

## Session log
- 2026-11-06: user requested AgentsMemory; created. User confirmed the Comparison Insights status is correctly recorded as unknown/unfinished (their reply "yes correct"). User then made automatic memory updates mandatory; WORKFLOW.md and CLAUDE.md updated. No app code changed; no manual or automated app testing involved.

## Next actions (suggested, awaiting user direction) [I]
- User reviews/approves the Dashboard milestone; user manually verifies unverified milestones.
- Clarify with the user the status of the Insights work (finish, document in MILESTONE.md/commit.txt).
- Do not start another milestone before approval [U].

## 2026-11-07 update
Independent-feature organization implemented (ToolRail, hosted Duplicates/Properties pages, Analytics own source, hand-offs). `run.bat /nolaunch`: ctest 12/12. NOT manually/visually verified; nothing committed. Details: docs/MILESTONE.md, docs/GUI.md. Note: git was clean at session start, so earlier "uncommitted work" notes above are stale (those commits exist).

## 2026-11-07 C++ layout [V]
- GUI now `gui/include` + `gui/src` + `gui/main` (see docs/GUI.md, DECISIONS.md). `run.bat /nolaunch` exit 0, ctest 12/12 passed (automated). Not visually/manually verified; not committed. `commit.txt` not updated for this.

- [V] 2026-11-07 [U] Left ToolRail removed; tool navigation moved into the title bar (ToolNav inside AppHeader). Built + ctest OK; NOT visually inspected.

- [V] 2026-11-07 Duplicates page layout fix (user-reported mixed-up layout): QGridLayout for the 8 metric cards, flexible setup column (min 320/max 360, stretch 1:2), member pane min width 340, group list elided. `run.bat /nolaunch` ctest 12/12; Claude inspected hosted-page renders (grab_screens, sample data) at 1180x800 and 960x640. User has not verified; nothing committed. See docs/MILESTONE.md.

- [V] 2026-11-07 Stage 1 committed as c1ef529 (previous Duplicates grid fix; not pushed; AnimeWatchURL.json left untracked, unrelated). Stage 2 [U]: Duplicates page redesigned (header; scrolling Sources/Options sections with the Find action pinned below; results grouped as Exact / Visually similar / Scan; member buttons under text). UNCOMMITTED per user. `run.bat /nolaunch` OK, ctest 12/12; Claude inspected grab_screens renders at 1180x800 and 960x640; user has not verified. At 960x640 the page still scrolls vertically (safety net).

- [V] 2026-11-08 Duplicates results refactored to group hierarchy [U request] (`duplicatefinder_results.cpp`: `fillGroups`, `groupHeader`, `memberRow`; QListWidget/splitter removed; `selectGroup` removed). `run.bat /nolaunch` OK, ctest 12/12; Claude inspected `grab_screens <dir> dup` renders (1 group, many, similar, empty). User has not verified; NOT committed/pushed; `commit.txt` not updated. Previous Stage 2 redesign still uncommitted too.

## 2026-11-08 `.resora` Phase 1 [V]
- Phase 1 (File Format Foundation) implemented, uncommitted: `resora_format` library (`gui/include|src/resora/`), `tst_resorafile`, docs/RESORA_FORMAT.md; tracker in implement.md = 0/7 completed, Phase 1 IN PROGRESS (awaiting user review), 2-7 NOT STARTED. Review round 1 done: whole-file hash documented+tested, writer pre-commit size/layout checks, JSON Schemas in docs/schemas/resora/; run.bat /nolaunch OK, ctest 13/13. Awaiting user approval. NOTE: `run.bat` reads only ONE switch, so `/nolaunch /nocommit` still auto-commits (happened once; undone with reset --mixed). Use `/nocommit` alone when commits are forbidden (it launches the app). Not integrated into the app/dashboard (by design). Phases 2-7 not started; each needs its own prompt.

## 2026-11-08 `.resora` Phase 2 [V]
- Phase 1 formally approved by the user [U] (COMPLETED). Phase 2 Bundled Export implemented, uncommitted: `resora::exportBundled` (`gui/include|src/resora/bundledexport.*`), additive Writer progress/cancel callback, `tst_bundledexport` 15/15; full ctest 14/14 suites (184 tests) passed after review fixes (progress `permille`/`Stage::Done`, source-change/locked-destination/cancel tests). Not `run.bat`; no manual verification. Tracker 1 completed / 1 in progress / 5 not started (14.29%). Do not use `run.bat` without `/nocommit` working (auto-commit incident). API and mapping: docs/RESORA_FORMAT.md.
- Later 2026-11-08: user review required crash-safe destination replacement; done via `destinationfinalizer.*` (ReplaceFileW + backup, MoveFileExW for new, retry on locks). `tst_bundledexport` now 21/21, full ctest 14/14 (190 tests). Phase 2 stays IN PROGRESS awaiting user review; Phase 3 not started; nothing committed/pushed.
- Second review pass 2026-11-08: ReplaceFileW 1175/1176/1177 handled explicitly (state-observing finalizer, `NeedsAttention` + `keepTemp`, never deletes uncertain files); `tst_bundledexport` 27/27, full ctest 14/14 (196 tests). Documented vs assumed behaviour in docs/RESORA_FORMAT.md. Phase 2 still IN PROGRESS; no Phase 3; nothing committed.

## 2026-11-08 `.resora` Phase 3 [V]
- Phase 2 approved [U] (COMPLETED). Phase 3 Linked Data Export implemented, uncommitted: `resora::exportLinked` (`linkedexport.*`), shared `exportcommon.*` (Bundled refactored onto it, behaviour unchanged). `tst_linkedexport` 12/12, full ctest 15/15; review fix [V]: final rehash of every source (size + SHA-256 vs validation) before finalizing, cancellable, tested with same-size edit + restored mtime. No `run.bat`, no manual verification. Tracker 2 completed / 1 in progress / 4 not started (28.57%). Path policy and schema: docs/RESORA_FORMAT.md. Awaiting user approval; Phase 4 not started.


## 2026-11-08 `.resora` Phase 4 [V]
- Phase 4 Import/Opening engine implemented, uncommitted: `resora::openResora/relinkAndOpen/closeDocument/readThumbnail` (`resoraimport.h`, 4 .cpp files in `resora_format`), Final correction: extraction cancel is per 1 MiB chunk (`Reader::copyResource` optional chunk callback, default no-op); new test `cancelDuringSingleLargeImageStopsBetweenChunks`; `tst_resoraimport` passed (new case added; CTest hides per-case totals), full ctest 16/16 [V, ctest direct]. Phase 4 stays IN PROGRESS pending review. Linked: abs path then relative (folder vs document base, `imageRoot` override), size+SHA-256 mandatory, all failures collected, `LinksBlocked` returns no document, relink revalidates all. Bundled: extract to marked `doc-<uuid>` cache folder, verified, cleaned on fail/cancel. Awaiting user review; Phase 5 not started. No `run.bat`, no manual verification.

## 2026-11-08 `.resora` Phase 5 [V]
- Phase 4 approved [U]. Phase 5 dashboard integration implemented, uncommitted: `dashboard::{plan,exportBundled,exportLinked,restore,Session}` (`backend/dashboardstate.h`), `AnalyticsPanel::result()`; `tst_dashboardstate` 12/12, full ctest 17/17 (direct cmake/ninja, no run.bat). No UI wiring (Phase 6). Awaiting review; Phase 6 not started. implement.md still lists Phase 3 IN PROGRESS (user did not say it was approved).

## 2026-11-08 Phase 5 final-review corrections [V]
- A: restored dashboard now uses runtime paths: `AnalyticsPanel::showRestored` + `dashboard::runtimePaths`; persisted analytics JSON unchanged; `restore()` rejects unresolved files; no fallback to stale paths. `tst_dashboardstate` 15 passed, full ctest 17/17 (direct cmake/ninja). B: commit f8d169b is unreachable (reflog: made, then `reset --mixed HEAD~1`); HEAD is 85c2626; Phase 5 files are uncommitted/untracked; nothing changed in history. C: tracker now Phase 1-4 COMPLETED (Phase 3 user-approved), Phase 5 IN PROGRESS, 6-7 NOT STARTED (4/7). No run.bat, no manual/GUI verification, nothing committed. Phase 6 not started.

## 2026-11-09 `.resora` Phase 6 [V]
- Phase 5 approved [U]. Phase 6 UI implemented, uncommitted: export/open dialogs, relink panel, jobs (worker waits on destroy), MainWindow shortcuts; `tst_resoraui` 14 passed, ctest 18/18. Relinks that passed persist across retries. Not visually verified; Phase 7 not started.
