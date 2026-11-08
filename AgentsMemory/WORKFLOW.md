# Workflow

## Memory-first task sequence [U, 2026-11-06]
1. Read `AgentsMemory/INDEX.md`.
2. Read only the relevant memory files (table in INDEX).
3. Check `CURRENT_STATE.md`.
4. Note existing decisions (`DECISIONS.md`) and preferences.
5. Inspect only the source files the task needs. Don't re-read the whole repo or completed reports.
6. Implement.
7. Update memory if something important changed (see triggers below).
8. Report what changed (and what was/wasn't verified).

Memory vs code conflict → check the code, fix the memory (note the date).

## MANDATORY automatic memory updates [U, 2026-11-06]
Do not wait for "remember this". Update memory automatically after: a feature implemented; a bug investigated or fixed; an architectural decision; a design decision approved or rejected; a user correction; a milestone completed or changing status; a new limitation or technical risk; a change to workflow or user preferences; important future plans established.

### End-of-task procedure (before reporting a task complete)
1. Identify what actually changed.
2. Compare with existing memory.
3. Update only the relevant files (decisions + reasoning in `DECISIONS.md`, corrections in `USER_PREFERENCES.md`/`DECISIONS.md`, risks in `KNOWN_ISSUES.md`, status in `MILESTONES.md`).
4. Update `CURRENT_STATE.md` (always, when anything meaningful changed).
5. Check `INDEX.md` still points to the right files.
6. Report which memory files were updated.
A finished memory update never makes an implementation "complete"; completion needs the task's own verification.

### Session continuity (record in CURRENT_STATE.md)
What the user requested; what was implemented; what they approved/rejected; what remains unfinished; corrections; next logical action. Never invent manual testing: record the user's manual verification and Claude's automated testing separately, and only what was actually stated/run.

### Avoid noise
No memory write for trivial edits; group related changes into one entry; don't duplicate, don't rewrite unrelated files, don't retell the whole history, no terminal output dumps, no secrets/credentials. Preserve history: mark superseded entries instead of deleting.

### Failure handling
If a memory write fails: don't claim success, name the file that failed, report the implementation result separately, never ignore it silently.

## Build / test (from CLAUDE.md) [V]
- `run.bat` = launcher (kills only `ImageResolutionAnalyzer.exe`, venv, selfcheck, unittest, build, ctest, launch). `run.bat /nolaunch` builds and checks only. Log: `logs/run.log`. Never invent other launch commands.
- Engine tests: `.venv\Scripts\python -m unittest discover -s tests`. GUI tests: ctest via run.bat. Prefer focused tests for what changed.
- Qt 6.12.0 MinGW at `D:\Apps\Qt6\6.12.0\mingw_64`; use Qt's MinGW, not MSYS2 g++. Use `python` not `py -3`.
- `gui/tests/grab_screens.cpp` renders real windows to PNGs for visual inspection (sample data) [R].  `grab_screens <outdir> dup` renders only the Duplicates states.

## Git [V]
Check `git status` first. No commit without explicit permission. `.venv/ build/ logs/` ignored.

## Reporting
Separate automated vs manual verification; state plainly what was not seen; keep reports practical.
