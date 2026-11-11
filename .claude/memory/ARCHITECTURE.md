# Architecture summary (details: docs/ARCHITECTURE.md, BACKEND_CONTRACT.md, GUI.md) — verified 2026-11-06

## Split [V]
- **GUI** `gui/`: C++17, Qt 6 Widgets, CMake+Ninja. Presentation only, no image-analysis logic. `imgan_core` static lib holds everything except `main()` so tests link the same code.
- **Engine** `scripts/engine/`: pure Python, `python -m scripts.engine <cmd>`. Never imports Qt; independent of GUI cwd.
- **IPC**: GUI starts engine via `QProcess`; NDJSON envelopes on stdout (`progress`*, then one `result`/`error`), stderr = diagnostics, exit codes 0/1/2/3/130, cancel by writing `cancel\n` to stdin (grace 3 s, then kill). Protocol v1; new commands are additive. Image selections pass via `--params-file` temp JSON.
- Discovery: env `IMGAN_PYTHON` + `IMGAN_BACKEND_DIR` (set by run.bat) → search upward from exe → `python` on PATH.

## Engine layout [V]
`main/cli/params/protocol/command/registry` + `commands/` (analytics, properties, scan, duplicates, selfcheck, contract_test) + `imaging/` (discovery, metadata via Pillow header-only, aspect, classification, analyzer, records, selection, properties, exif_fields, analytics, collection) + `duplicates/` (sources, hashing SHA-256, perceptual dHash, exact, similar, finder, cache SQLite, savings) + `reports/` (CSV/TXT/summary, atomic write).
Rules: dependencies point downward; feature modules never import each other; no `utils` grab-bag; no global mutable state; only `protocol.py` writes stdout; module size guidelines ≤150 typical, >250 review, >350 justify. Enforced by `tests/test_engine_modules.py`. Adding a command = `commands/<name>.py` + one line in `registry.py`.

## GUI layout [V]
`backend/` (BackendProcess, ProtocolParser, backendlocator, ImportSelection, models: properties/analytics/dashboard/duplicates, comparison, insightmetrics, previewloader), `locations/` (Saved Locations, JSON in AppConfigLocation), `ui/` (theme, brand, icons, uiformat), `widgets/` (everything visual). Qt-Core-only logic in `backend/` is unit-testable.
Window: frameless, left phase rail 01 Source / 02 Scan / 03 Readout (+ 04 Analytics), Ctrl+1/2/3; each page scrolls on its own; Compare workspace fills window. Shortcuts: Ctrl+O import, Ctrl+I properties, Ctrl+Shift+C compare, Ctrl+Shift+D duplicates, Ctrl+, settings, F1 about.
Assets: SVGs in `Assets/`, compiled via `Assets/resora.qrc`, GUI links `Qt6::Svg`; icons use only two colour tokens (paper `#EFE9DD`, ember `#FF5B2E`) swapped at render time.

## Dependencies & packaging [V]
Python deps only in `requirements.txt` (Pillow etc., lower bounds). Qt modules explicit in `gui/CMakeLists.txt`. Python stays `.py` in development; engine kept packaging-friendly (single CLI entry, no dynamic imports) so Nuitka can be adopted later in a separate task [V]. Exe name stays `ImageResolutionAnalyzer.exe` on purpose [V].

## Tests [V]
Engine: `unittest` in `tests/`. GUI: ctest executables `gui/tests/tst_*.cpp` (protocolparser, backendprocess, widgets, locations, icons, dialogs, properties, analytics, comparison, duplicates, dashboard, insights).
