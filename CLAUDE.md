# RESORA — Image Resolution Analyzer

Product name RESORA, tagline "See every pixel clearly." Identity (mark, wordmark, icon, About) in `gui/src/ui/brand.*`, [docs/GUI.md](docs/GUI.md). Repo, packages and exe file name (`ImageResolutionAnalyzer.exe`) intentionally keep the old name.

Windows desktop tool: scan a folder, read each image's pixel dimensions from its header (no pixel decoding), classify the resolution (SD … 8K+), export reports. Original images are opened read-only and never modified.

**Status:** modular engine + CLI contract + image-analysis backend (`scan` command: discovery, Pillow header metadata, resolution classification, CSV/TXT reports). The Qt GUI is implemented (folder/report selection, scan, progress, cancel, results; see docs/GUI.md). `main.py` (renamed from `image_analyzer.py`; do not restore that name) + `README.md` are the legacy Tkinter prototype (reference for classification rules; do not extend). Do not implement features without an explicit instruction.

## Architecture
- **GUI** (`gui/`): C++17 + Qt 6 Widgets, CMake + Ninja. Presentation only; no image logic.
- **Engine** (`scripts/engine/`): pure Python package, run as `python -m scripts.engine ...`. All scanning/classification/export logic. Must never import Qt or any GUI toolkit, and must not depend on the GUI's working directory.
- **IPC:** the GUI launches the backend as a separate process (`QProcess`): NDJSON envelopes on stdout (`progress`*, then one `result`/`error`), diagnostics on stderr, exit codes 0/1/2/3/130. Contract: [docs/BACKEND_CONTRACT.md](docs/BACKEND_CONTRACT.md). New commands go in `scripts/engine/commands/<name>.py` and are registered in `scripts/engine/registry.py`; command modules never import each other, never print to stdout, never import Qt. The GUI finds it via env `IMGAN_PYTHON` + `IMGAN_BACKEND_DIR` (set by `run.bat`). Details: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

### Module rules (engine)
One module = one responsibility, independently testable, plain inputs/outputs. Dependencies point downward only (`commands/*` -> `command`, `protocol`, logic modules); feature modules never import each other; no giant `utils`/manager files, no global mutable state. Size: most modules <=150 lines, >250 needs review, >350 needs justification, avoid 500+ (guidelines, never split needlessly). Map and how-tos: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Environment (this machine)
- Qt 6.12.0 MinGW 64 at `D:\Apps\Qt6\6.12.0\mingw_64`; MinGW 13.1, CMake, Ninja under `D:\Apps\Qt6\Tools` (override with `QT_DIR` / `QT_TOOLS`). Use Qt's MinGW, **not** the MSYS2 `g++` on PATH (ABI mismatch).
- Python 3.11+ from PATH (3.13 installed). `py -3` is broken here (stale 3.15t registration), so `python` is preferred.

## Build and run
- `run.bat` is the development launcher: kills only `ImageResolutionAnalyzer.exe`, reuses/creates `.venv`, reinstalls deps only if `requirements.txt` changed, runs engine selfcheck + unit tests, configures (once) and builds the GUI into `build/`, launches it.
- `run.bat` also runs GUI tests via ctest (exit 53). `run.bat /nolaunch` builds and checks without launching. `run.bat /commit` = normal build + launch, then `git add -A` + commit with the last block of `commit.txt` (explicit user action only; never pushes; exit 67-69). `run.bat /push` is a separate Git-only push (exit 60-66, see header of run.bat). Log: `logs/run.log`. Exit codes: 10 stop app, 20-22 python/venv, 30 pip, 40-41 engine checks, 50-52 Qt/build.

## Development workflow (verification discipline)
- Don't run `run.bat /nolaunch` repeatedly to prove completion; don't repeat a successful verification without a reason (changed code, a failure to re-check, shared infrastructure touched).
- Need the real app (launch or manual GUI testing)? Use the existing `run.bat`; never invent separate launch commands, the launcher owns the environment.
- Never claim a GUI was visually verified unless it was actually inspected (screenshot/interaction). The user can inspect the running app; say plainly what was not seen.
- Prefer focused tests for the code you changed; run the full suite when shared infrastructure or several components are affected.
- Report automated verification (unit/ctest/subprocess) and manual verification separately. Never weaken tests or hide failures; this rule only avoids redundant repeats.
- `run.bat /push` pushes the current branch (explicit user action only; it never stages, commits or force-pushes). Don't run it unless the user asks.

## Testing
- Engine: `unittest` in `tests/` (`.venv\Scripts\python -m unittest discover -s tests`); run automatically by `run.bat`. Add tests with every engine change.
- GUI: must at least build cleanly; verify launch via `run.bat`. Never claim a test passed without running it.

## Conventions
- Python: type hints, `from __future__ import annotations`, stdlib-first, no global state, errors reported (never swallowed).
- C++: C++17, Qt idioms, `QProcess` for the engine, no blocking calls on the GUI thread.
- Dependencies: Python deps only in `requirements.txt` (lower bounds, minimal). Qt modules added explicitly in `gui/CMakeLists.txt`.
- Python stays as `.py` during development (no compiled executables). Keep the engine packaging-friendly (single CLI entry, no dynamic imports/path hacks) so Nuitka can be adopted later in a separate task.

## Git workflow
- Check `git status` before changes; never discard user modifications; never force-push; **never commit without explicit permission**.
- After each meaningful task, write a concise conventional-commit message into `commit.txt` describing only work actually done (no invention, don't touch unrelated changes). Keep it in sync with the real diff.
- `.venv/`, `build/`, `logs/` are git-ignored.

## Restrictions
- No Qt in the engine; no image-analysis logic in C++.
- Never modify or delete analyzed user images.
- Don't kill unrelated Python/Qt processes.
- New docs only when genuinely needed; link them here.

## Docs
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): GUI/engine split.
- [docs/BACKEND_CONTRACT.md](docs/BACKEND_CONTRACT.md): CLI/IPC contract (protocol v1), QProcess, cancellation, how to add commands.
- [docs/GUI.md](docs/GUI.md): GUI classes, backend discovery, QProcess, progress, cancellation, reports.
- [Assets/README.md](Assets/README.md): visual assets (SVG icons, branding, ICO), colour tokens, how to add an icon.
- [docs/MILESTONE.md](docs/MILESTONE.md): permanent per-milestone completion record (append only; evidence-based).
- [docs/ROADMAP.md](docs/ROADMAP.md): approved four-feature roadmap (properties, analytics, comparison, duplicates) and per-milestone plan; Milestones 1-4 are implemented (awaiting review).
- [docs/ANALYTICS.md](docs/ANALYTICS.md): Resolution analytics definitions (averages, percentages, categories), `analytics` command, TXT option.
- [docs/DUPLICATES.md](docs/DUPLICATES.md): Duplicate Finder (`duplicates` command): exact/similar algorithms, thresholds, storage savings.
- [docs/IMAGE_ANALYSIS.md](docs/IMAGE_ANALYSIS.md): `scan` command, classification strategy, EXIF/multi-frame behaviour, reports.
