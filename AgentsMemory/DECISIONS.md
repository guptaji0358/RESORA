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
