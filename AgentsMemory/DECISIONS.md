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
