# Qt GUI (`gui/`)

Presentation only: it collects options, runs the `scan` command as a child process and renders the NDJSON it emits. No image logic lives here. Contract: [BACKEND_CONTRACT.md](BACKEND_CONTRACT.md).

## Classes (`gui/src/`)
| Class | Responsibility |
|---|---|
| `MainWindow` | Layout, input validation, overwrite confirmation, owns one `BackendProcess` per scan, progress/status/result display |
| `backend/ProtocolParser` | Buffers stdout, splits lines, validates `v`, emits `progress` / `result` / `errorEnvelope` / `violation` |
| `backend/BackendProcess` | One run: `QProcess`, parser, cancel -> grace -> kill, stall timeout, maps outcome to exactly one of `succeeded` / `failed` / `cancelled` |
| `backend/locateBackend` | Finds Python and the engine directory |
| `backend/ScanRequest` | Options -> `scan` argument list |
| `widgets/SourcePicker`, `ImportDialog`, `ReportSelector`, `ResultsPanel` | Imported-source summary, Ctrl+O import chooser, per-report destination (CSV / TXT independent), results + Open buttons |
| `backend/ImportSelection` | Folder / single image / multiple images model: classifies missing, unsupported, duplicate paths (no image logic) |

`imgan_core` (static library) holds everything except `main()`, so tests link the same code as the app.

## UI structure and styling (Milestone 6)
Window: fixed header (title + "images are never modified") above a scrollable column of cards: Folder, Scan settings, Report export, Start/Cancel row, Progress, Results. Minimum size 640x560; content scrolls when short, and long paths elide instead of widening the window.

| Piece | Responsibility |
|---|---|
| `ui/theme` | Dark-only colour tokens, app-wide dark `QPalette` (`applyPalette`) and the one application stylesheet; `Theme::setProperty` sets a dynamic property and re-polishes. Widgets are styled by property (`role`, `variant`, `state`, `tone`, `invalid`), never by per-widget stylesheets |
| `ui/uiformat` | Pure text helpers (`count`, `percent`, `progressSummary`), unit-tested |
| `widgets/SectionCard` | Titled card with a body layout |
| `widgets/ProgressPanel` | State badge (Ready / Starting / Scanning / Cancelling / Completed / Failed / Cancelled), message, bar, "N of M files (P%)", current item, stderr details |
| `widgets/MetricCard` | One headline number (discovered / analyzed / failed) |
| `widgets/ElidedLabel` | Middle-elided single-line text with the full text as tooltip (paths) |

Start Scan is the primary (filled) button, Cancel Scan the red-outline secondary; disabled buttons are grey. Controls share 2 px borders and a 32 px height; keyboard focus turns the border blue.

**Theme:** the application is dark-mode only by design (no toggle, no light palette). Deep charcoal window (`#1e2024`), lighter cards (`#272a30`), darker input surfaces (`#181a1e`), subtle `#3b4048` borders, soft-white text, muted-grey secondary text, restrained blue primary (`#2f6fd0`, focus `#5b9bff`), readable green/red for success/error. `Theme::applyPalette` sets Fusion plus a dark palette so menus, tooltips and Qt-drawn dialogs match; native file dialogs keep the Windows system look.
 State is always also written as a word in the badge, never colour alone. The bar is indeterminate until `total` is known; percentages come only from backend `done`/`total`. `MainWindow` applies the stylesheet; `main()` only calls `Theme::applyPalette`.

## RESORA brand identity
Product: **RESORA** (RES from resolution, ORA for a distinctive ending), subtitle *Image Resolution Analyzer*, tagline *See every pixel clearly.* The name is the selected internal name; no trademark or domain clearance is claimed.

**Mark ("the resolving grid"):** a square quadtree. A coarse, dim cell at top-left subdivides toward bottom-right into ever finer, brighter cells until one ember pixel remains: resolution increasing until every pixel is clear. It reuses the Darkroom ember/paper palette, so there is no stock-software look. Wordmark: Bahnschrift Bold, wide tracking, RES in paper and ORA in ember.

| Piece | Where |
|---|---|
| Names, tagline, version, painted mark, `QIcon` | `ui/brand` (`Brand::paintMark`, `Brand::icon`) |
| Header lockup (mark + wordmark + descriptor) | `widgets/BrandLockup`, used by `AppHeader` |
| About box (F1) | `widgets/AboutDialog` |
| Window title, app name/icon | `MainWindow`, `main()` |
| Exe icon + Windows file description | `gui/assets/resora.rc` + `resora.ico` (regenerate with `python gui/assets/generate_icon.py`; geometry mirrors `brand.cpp`) |

The executable file name stays `ImageResolutionAnalyzer.exe` (run.bat and tooling); the Windows resource shows "RESORA" as product name. No splash screen: the app starts instantly, so one would only add delay. The tagline appears on the Source page and in About.

## "Darkroom" identity (Milestone 6.6)
Dark-only, original look: warm black (`#0D0C0B`), paper-white text, one ember safelight accent (`#FF5B2E`), lime for success, rose for failure; Bahnschrift for display text and Cascadia Mono for data. Tokens, fonts and the small stylesheet (line edits, scroll bars, tooltips) live in `ui/theme`.

Structure: a frameless window (custom title strip with painted min/max/close; edge resizing via `WM_NCHITTEST` in `MainWindow::nativeEvent`) with a left **phase rail** (01 Source, 02 Scan, 03 Readout; Ctrl+1/2/3) and one stage that moves through the phases: Start Scan switches to Scan, a completed scan switches to Readout, New Scan returns to Source. Rail entries are enabled only once reachable. Validation problems appear on the Source page.

| Widget | Role |
|---|---|
| `PhaseRail` | Painted numbered phase tabs |
| `AppHeader` | Drag area, mark, wordmark, state readout, caption buttons |
| `Viewfinder` | 16:9 frame that "exposes" from the bottom by the real done/total; indeterminate = sweeping scan line |
| `DistributionBar` | "Resolution ladder": rows sorted by pixel size, each with a frame glyph at true proportion, share bar and count (custom/panorama classes get a dashed frame) |
| `ChamferButton`, `ToggleSwitch` | Painted button (clipped corner) and switch, still `QPushButton` / `QCheckBox` underneath |
| `SourcePicker` | Shows kind (FOLDER / SINGLE IMAGE / MULTIPLE IMAGES), name, location, count and any skipped files; "Import..." / "Change selection..." and Ctrl+O open `ImportDialog`; accepts dropped folders or files |
| `ImportDialog` | Three tiles (keys 1/2/3): Import Folder, Import Single Image, Import Multiple Images; each opens the native Qt file dialog (image filter plus "All files") |

Backend, QProcess, NDJSON, reports and cancellation are unchanged.

## Python / backend discovery
1. `IMGAN_BACKEND_DIR` / `IMGAN_PYTHON` (set by `run.bat`); a set-but-wrong value is an error, not a silent fallback.
2. Otherwise search upwards from the exe for `scripts/engine/__main__.py`, then use `<dir>/.venv/Scripts/python.exe`.
3. Otherwise `python` on `PATH`. Nothing is downloaded or installed at launch.

## Process and protocol
Program = interpreter, arguments = list `-m scripts.engine --cancel-on-stdin scan <folder> [--recursive] [--csv P] [--txt P]` (image selections: `--params-file <temp json> scan [--csv P] [--txt P]`, the temp file is removed when the job ends) (never a shell string), working directory = backend dir, `PYTHONIOENCODING=utf-8`. stdout (protocol) and stderr (diagnostics, last 64 KB kept, shown on failure) are read separately and asynchronously; there are no `waitFor*` calls.

Outcome mapping: `spawn_failed` (cannot start), engine `error` codes (e.g. `invalid_input`, `report_failed`), `crashed` (crash, or exit without a final envelope), `protocol_error` (malformed JSON, wrong `v`, envelope after the final one, oversized or truncated line; the process is killed), `timeout` (no stdout for 60 s). Unknown envelope types are ignored. An unterminated last line is accepted if it is valid JSON, else reported as a violation.

## Progress
Backend progress is throttled to ~10/s and the UI refreshes at most every 100 ms. `total` known -> determinate bar; `total` null (discovery) -> indeterminate bar. No percentage is invented.

## Cancellation
Cancel writes `cancel\n` to stdin and closes it, then `kill()` after 3 s if the process is still alive. Outcome is reported as *cancelled* (not failed) whatever the exit status. Exception: if the final `result` had already arrived with exit 0, the scan is reported as completed. Closing the window mid-scan runs the same sequence, then closes. Files from a cancelled scan are not valid.

## Reports
CSV and TXT are independent (none, either, both). Enabled with an empty path is rejected; the right extension is appended if missing; a folder as destination is rejected; CSV and TXT must differ. Missing parent folders are allowed (the backend creates them). Existing files are only replaced after a Yes in a confirmation dialog. Results show a report path only if `result.data.reports` confirms it; Open / Show folder use `QDesktopServices`. A `report_failed` error is shown as a report failure, separate from scan results (the backend reports no summary in that case).

## Running
`run.bat` (build + checks + launch) or `run.bat /nolaunch`. GUI tests (`ctest --test-dir build`, run by `run.bat`, exit 53 on failure): `tst_protocolparser`, `tst_backendprocess` (drives the real engine: success, error, spawn failure, cancel, kill escalation, stall timeout), `tst_widgets` (validation, request arguments, initial window state, UI formatting, progress panel states, results metrics).
