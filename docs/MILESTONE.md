# RESORA — Milestone Records

Permanent completion record. **Append a new section per milestone; never overwrite earlier ones.**
Copy the template at the bottom for each new milestone. Evidence must come from real runs or
real manual checks; do not record anything that was not actually done.

Status values: **FINISHED** only when all required work is complete and verified, otherwise **NOT FINISHED**.

---

## Milestone: GUI polish — Saved Locations, asset system, dialogs, Enter-key fix

Scope (from `commit.txt`): Saved Locations in Settings with path
suggestions; SVG icon/asset system (`Assets/`); frameless dialogs with on-screen placement;
stray-Enter fix.

### 1. Automated verification
Source: `logs/run.log`, run of 2026-11-05 11:09 (`run.bat`). Not re-run for this document.

| Check | Result |
|---|---|
| Engine selfcheck | OK (backend 0.0.1, Python 3.13.15, Pillow 13.0.0) |
| Engine unit tests (`unittest`) | 69 tests, OK |
| GUI build (`ninja`) | Clean ("no work to do"; up to date) |
| GUI tests (ctest) | 6/6 passed: tst_protocolparser, tst_backendprocess, tst_widgets, tst_locations, tst_icons, tst_dialogs |

Source-state limitation: the log does not record the exact source state of that run; edits made after
it are not covered by this automated evidence.

### 2. Manual verification
Performed by the user (2026-11-06), who reported checking the running application and confirming that
the requested functionality works. The specific scenarios were not itemised by the user, so none are
listed here. The assistant did not visually inspect the GUI.

### 3. Known defects
None confirmed.

### 4. Unverified items
- Whether the 11:09 automated run matches the final source state (see limitation above).
- No visual or interactive inspection by the assistant.

### 5. Final status
**FINISHED** — automated checks passed (with the source-state limitation noted) and the user manually
verified the functionality.

### 6. Administrative note (does not affect completion)
Git changes for this milestone are still uncommitted (including staged renames of `gui/assets/*` to
`Assets/app/*`). Commit and push are handled separately by the user; no committed revision has been
verified, and this does not block the FINISHED status.

---

## Milestone: Resolution Analytics (Milestone 2)

Scope: collection/resolution statistics, category/orientation/format distributions, Analytics phase in the GUI,
`analytics` command, optional TXT analytics section. Definitions: [ANALYTICS.md](ANALYTICS.md).

### 1. Automated verification
Source: `logs/run.log`, `run.bat /nolaunch` of 2026-11-06 20:3x (run once after the final code change).

| Check | Result |
|---|---|
| Engine unit tests (`unittest`) | 100 tests, OK (21 new in tests/test_analytics.py) |
| GUI build (`ninja`) | Succeeded |
| GUI tests (ctest) | 8/8 passed, incl. new tst_analytics |

### 2. Manual verification
Not yet performed. The assistant did not launch or visually inspect the GUI.

### 3. Known defects
None confirmed (not manually checked).

### 4. Unverified items
- Visual layout, spacing, scrolling and dark styling of the Analytics phase in the running app.
- Refresh / cancel flow, Properties and Folder shortcuts, and the TXT switch in the running app.
- Behaviour on a very large real collection (large size covered only by a synthetic unit test).

### 5. Final status
**NOT FINISHED** — implementation and automated checks complete; awaiting manual verification by the user.

---

## Milestone 3: Image Comparison
Scope: compare two images side by side (synchronised zoom/pan inspection viewer), technical property table with highlighted differences, and insights. No folder scan needed.

Design: no new backend command (deviation from the roadmap's `compare` sketch). Metadata reuses the existing `properties` command, one small job per image cached by path; differences/insights are computed in Qt-Core code (`gui/src/backend/comparison.*`) from that payload, so nothing is recalculated. Previews are decoded by Qt only (`previewloader.*`: header sniff, long edge capped at 4096 px, first frame only, worker threads, read-only).

### 1. Automated verification
Command: `run.bat /nolaunch` (2026-11-06). Python selfcheck OK; Python tests 100/100 OK; GUI build OK; ctest 9/9 passed incl. new `tst_comparison` (10 tests: differences, aspect/dimensions, missing metadata, unreadable side, percent, preview loader valid/downscaled/corrupted/missing, pane fit/zoom/pan/reset, synced vs independent zoom/pan, workspace select/replace/swap/clear/unsupported/corrupted).

### 2. Manual verification
Not performed. The assistant did not launch or visually inspect the GUI.

### 3. Known defects
None confirmed (not manually checked).

### 4. Unverified items
- Look, layout and splitter behaviour of the workspace; shortcuts (Esc, Ctrl+Shift+C/X/F/1/0) in the running app.
- Replace / Collection dialogs, Back navigation, wheel/drag feel, large real images.
- Properties table values against real files with EXIF/ICC/DPI.

### 5. Final status
**NOT FINISHED** — implementation and automated checks complete; awaiting manual verification by the user.

---

## Milestone 4: Duplicate Finder
Scope: exact (SHA-256) and visually similar (dHash) duplicate detection over chosen folders or the imported collection; dedicated dialog with groups, thumbnails, Open/Folder/Compare and storage insights. Detection and comparison only; no delete/move. Definitions: [DUPLICATES.md](DUPLICATES.md).

### 1. Automated verification
Command: `run.bat /nolaunch` (2026-11-06). Python tests 128/128 OK (28 new in tests/test_duplicates.py: exact copies across names/folders/extensions, same-size different content, resized/recompressed/reformatted, unrelated, crop/edit rejection, thresholds, anti-chaining, corrupted/unreadable/empty, cache reuse and invalidation, large collection, cancellation, savings, originals untouched). GUI build OK; ctest 10/10 incl. new `tst_duplicates`.

### 2. Manual verification
Not performed. The GUI was not launched or visually inspected.

### 3. Known defects
None confirmed (not manually checked).

### 4. Unverified items
- Look/layout of the dialog, thumbnails, Open / Folder (Explorer select) / Compare hand-off, Ctrl+Shift+D.
- Behaviour and speed on a real large photo library; false-match rate on real photos at threshold 90.

### 5. Final status
**NOT FINISHED** — implementation and automated checks complete; awaiting manual verification by the user.

---

## Milestone: UI Refinement (layout, spacing, visual consistency)
Scope: inspect every screen/dialog at normal and minimum window size, fix real layout defects, keep the RESORA dark identity. No engine, protocol or feature changes.

### Layout problems found (from real renders, `gui/tests/grab_screens.cpp`, Windows platform plugin, 125 % scaling)
- Compare workspace sat inside the window-wide scroll area: at 960x640 the viewer was pushed far below the fold, the right pane clipped, "17%% FIT" showed a doubled percent sign.
- Scan page: the 16:9 viewfinder grew with the window width and pushed Cancel off screen at the minimum size.
- Result/source action rows were wider than the 780 px column; layouts squeezed buttons below their label (icon touching the border, e.g. IMAGE PROPERTIES).
- ChamferButton measured its size with the pre-stylesheet font and painted with the polished one, hence the mismatch above.
- Report rows: path elided to "Not generated …nfirm a file)." because buttons took the row.
- Content column stuck to the left with a large dead area on wide windows.
- Duplicate Finder: whole dialog in one scroll area; two big empty list boxes pushed results below the fold; member-row buttons clipped ("FOLDE", "COMPA"); settings and results never visible together.
- Analytics: uneven row spacing in tile grids. List widgets had no theme styling.

### Improvements
- Each phase page scrolls on its own; the Compare workspace fills the window (no outer scroll). Content column centred, width 860, tighter margins/spacing.
- New shared `FlowLayout` (wrapping button rows) for source and readout actions; ChamferButton sizes from a fixed label font.
- Viewfinder capped at 250 px high; report rows put the full path on its own line above the buttons.
- Duplicate Finder: setup column (360 px) beside results, status/progress under the button, placeholder before first result, wrapped member rows, splitter sizing; themed QListWidget/QSplitter.
- Fixed percent text in image pane; analytics tile grid spacing.

### Files changed
gui/CMakeLists.txt, gui/src/mainwindow.cpp, gui/src/ui/theme.cpp, gui/src/widgets/{chamferbutton.cpp,chamferbutton.h,flowlayout.cpp,flowlayout.h,duplicatefinder.cpp,duplicatefinder.h,duplicatefinder_results.cpp,imagepane.cpp,progresspanel.cpp,resultspanel.cpp,analyticspanel.cpp}, gui/tests/{CMakeLists.txt,grab_screens.cpp,tst_widgets.cpp}. Engine, main.py, protocol untouched.

### 1. Automated verification
Command: `run.bat /nolaunch` (2026-11-06). Python tests, engine selfcheck and ctest 10/10 passed after the changes (new `flowLayoutWrapsInsteadOfSqueezing` in tst_widgets; see final run in the report). Build OK.

### 2. Visual verification (actual rendering)
`grab_screens.exe` rendered the real MainWindow and dialogs with sample data at 1180x800 and 960x640 and saved PNGs that were inspected: Source, Scan (running/failed), Readout, Analytics (empty/filled), Compare (empty/two images), Settings, Properties, Duplicate Finder (setup/exact/similar), About, Import. Before/after were compared for Source, Readout, Scan, Compare, Duplicates, Analytics. This is rendering with sample data, not the live app driven by a user.

### 3. Known defects
- Dialogs were inspected at default size only, not at 100 %/150 %/200 % scaling or on a small physical screen; real-app interaction (hover/pressed/focus states, dragging dialogs, live scan) was not exercised.
- Duplicate Finder group list still elides long summaries; its empty folder lists have no placeholder text.
- Properties/Settings dialogs repeat the title in the title bar and the eyebrow (left as is).

### 4. Unverified items
Display scaling other than 125 %, multi-monitor boundary handling, high-contrast, keyboard traversal order after the layout changes.

### 5. Final status
**NOT FINISHED** — implementation and automated checks done; awaiting manual review.

---

## Milestone: Analytics Dashboard (Dashboard + Detailed View)
Scope: Analytics view gets two switchable views over the same `AnalyticsData`: **Dashboard** (default; stat tiles, resolution bar chart, orientation and format donuts, storage-by-format horizontal bars, megapixel span, insights) and **Detailed View** (the previous numeric view, unchanged). No engine, protocol, report or `main.py` change; switching never rescans.

### Chart technology
Custom **QPainter** widgets (`ChartBase` + `BarChart`, `DonutChart`, `HBarChart`, `SegmentedSwitch`), data shaping in Qt-Core-only `backend/dashboardmodel`. Qt Charts exists on this machine only as a static `libQt6Charts.a` (GPLv3/commercial licence, extra module, stock look that would need heavy restyling to match RESORA); Matplotlib would add a Python dependency and a rendering process to an interactive GUI. QPainter adds no dependency, matches the identity and gives exact control of labels, hover and accessibility.

### Chart choices
Bars: resolution classes (count above, name and percent below, nice axis). Donut + text legend: orientation, formats. Horizontal bars: storage by format (bar = share of storage, tick = share of images). Tiles: totals, averages. Every chart repeats labels, counts and percentages as text (colour is never the only cue); hover shows exact values; click or Left/Right/Esc selects a category and reveals its statistics under the chart; empty states per chart; zero classes stay visible; unknown file sizes give an empty storage chart, not zeros.

### Files
New: `gui/src/backend/dashboardmodel.{h,cpp}`, `gui/src/widgets/{chartbase,barchart,donutchart,hbarchart,segmentedswitch,analyticsdashboard}.{h,cpp}`, `gui/tests/tst_dashboard.cpp`. Changed: `analyticspanel.{h,cpp}`, `gui/CMakeLists.txt`, `gui/tests/CMakeLists.txt`, `gui/tests/grab_screens.cpp` (full-panel grabs).

### 1. Automated verification
`run.bat /nolaunch` (2026-11-06): engine self-check, Python tests and ctest 11/11 passed (new `tst_dashboard`: chart items vs engine values and denominators, empty/missing data, axis/bar/sweep math, Dashboard default and view switching without data loss, same values in both views, selection detail and reset, keyboard switch, a 12,000,000-image collection, grabs at 1100/520/360 px wide). Existing `tst_analytics` still passes unchanged.

### 2. Visual verification (actual rendering)
`grab_screens.exe` rendered the real MainWindow/panel with sample data (3 analyzed, 2 failed) at 1180x800 and 960x640; both views were inspected as PNGs. Seen: tiles, bar chart, donuts with legends, storage bars, span, insights, failure banner, Detailed View intact; two defects found this way (axis labels 0/0/1, blank gap above the Detailed page) were fixed and re-rendered.

### 3. Known defects
None confirmed.

### 4. Unverified items
Hover tooltips, click selection and keyboard use were tested by code only, not by a person with a mouse. Empty-collection and very large collections were not visually inspected. No inspection at 360 px real width, other display scaling, or a live scan. Only one resolution/format set was rendered (no 8-class ladder).

### 5. Final status
**NOT FINISHED** - implemented and automatically tested; awaiting manual review. Nothing committed or pushed.

---

## Refinement: Image Comparison Insights (visual analytics)
Scope: Insights section of Image Comparison now leads with a Quick Verdict and native QPainter visuals (paired MP bars, file-size bars, true-proportion dimension rectangles, aspect-ratio scale, A/B metadata table); the previous written explanations are kept, grouped, in a collapsed "Detailed explanations" section. No engine/backend change; `main.py` untouched.
### 1. Automated verification
`run.bat /nolaunch` (2026-11-06): build OK, 12/12 ctest suites pass incl. new `tst_insights` (metrics for different/identical resolutions and sizes, zero/missing/invalid values, aspect identical/nearly/different, metadata unavailable-vs-zero, rectangle proportions and common scale, rendering at 220-1600 px wide) and the unchanged `tst_comparison`. Engine selfcheck and Python tests passed.
### 2. Visual verification (actual rendering)
`grab_screens.exe <dir> insights` (real Windows platform plugin) rendered the Insights for five scenarios (different, identical, same aspect, invalid/missing, extreme size gap) at 900/520/340 px; PNGs were inspected. A clipped footer in the file-size chart at 340 px was found this way and fixed. Not inspected inside the running main window with real loaded images.
### 3. Known defects
Very small shares print as "0.0% of the larger" instead of "<0.1%".
### 4. Unverified items
Live interaction in the running app (splitter resizing, scrolling, keyboard focus on the collapsible header), other DPI scaling, real photos with EXIF/ICC.
### 5. Final status
**NOT FINISHED** - implemented and tested; awaiting manual review. Nothing committed or pushed.

---

## Milestone: Independent feature organization (ToolRail)
Scope: four first-class tools (Analyzer/Properties, Analytics, Compare, Duplicates) with a ToolRail, own workspaces and sources, and contextual hand-offs via `ToolRequest`. Duplicates and Properties moved from modals to hosted pages; Compare lost its Back button; the old phase rail became the Analyzer's horizontal step strip; promotional buttons removed from Source. No engine/backend change; `main.py` untouched. Decisions taken by assistant on approval: independent sources (with "Use Analyzer import" convenience) and inline Properties.
### 1. Automated verification
`run.bat /nolaunch` (2026-11-07): build OK, 12/12 ctest suites incl. new `toolsAreIndependentAndRouted` (tst_widgets) and adapted `tst_analytics`; engine selfcheck and Python tests passed.
### 2. Manual verification
None. The running app and the new layout were NOT visually inspected (no screenshot taken; `grab_screens` updated to compile but not run).
### 3. Known defects
None confirmed. Likely to check: embedded Duplicates/Properties minimum sizes in small windows; Analytics backend-not-found error appears on the Analyzer's Source notice.
### 4. Unverified items
Look and feel of ToolRail and step strip, all hand-offs by hand, keyboard shortcuts, Analytics progress/cancel, scan -> "Open in Analytics".
### 5. Final status
**NOT FINISHED** - implemented and automatically tested; awaiting manual review. Nothing committed or pushed.

---

## Fix: Duplicates page layout (2026-11-07)
Scope: GUI layout only (`duplicatefinder.cpp`; hosted-page shots added to `tests/grab_screens.cpp`). No engine/backend change; Duplicate Finder behaviour, controls and styling unchanged.
Root causes (Qt layout, not spacing): (1) the eight metric cards lived in two independent `QHBoxLayout`s, so the two rows had unrelated column widths and no vertical spacing; now one `QGridLayout` with shared equal column stretch and explicit spacing. (2) The setup column was `setFixedWidth(360)` while `ToggleSwitch` reports its full label width as minimum size; the member `QScrollArea` had horizontal scrolling off and no minimum width, so at the 960x640 window rows and buttons were clipped and nothing scrolled. Now setup column min 320 / max 360 with stretch 1:2 vs results, member pane min width 340 (page scrolls instead of clipping), group list min 180, elided, no horizontal scrollbar, splitter stretch on the member pane.
### 1. Automated verification
`run.bat /nolaunch` after the fix: build OK, ctest 12/12, engine selfcheck + Python tests passed.
### 2. Manual verification
Claude inspected real renders of the hosted Duplicates page (empty and with sample results) via `build/tests/grab_screens.exe` at 1180x800 and 960x640 (125% scaling): cards aligned, rows/buttons not clipped. Not seen: live scan with real photos, window resizing by hand, other DPI scales. User has not verified.
### 3. Known defects
At 960x640 the whole page scrolls vertically (expected; results need more height than 640).
### 4. Unverified items
Intermediate window sizes by hand, 100%/150% scaling, very long folder paths in lists.
### 5. Final status
**NOT FINISHED** - fixed and visually inspected on sample data; awaiting user review. Nothing committed or pushed.

---

## Template for the next milestone

```
## Milestone: <name>
Scope: <one or two lines>
### 1. Automated verification   (command, date, result; source of evidence)
### 2. Manual verification      (who, date, scenarios actually tested)
### 3. Known defects            (confirmed only)
### 4. Unverified items
### 5. Final status             (FINISHED / NOT FINISHED, with reason)
```

## Duplicates page redesign (2026-11-07) - NOT FINISHED
1. Automated: `run.bat /nolaunch` exit 0, ctest 12/12 (after change). Detection logic untouched.
2. Manual: none by the user. Claude inspected `grab_screens` renders (empty/results) at 1180x800 and 960x640; real running app not driven.
3. Known: at 960x640 the page scrolls vertically; Find button pinned in setup column may sit below the fold.
4. Unverified: real thumbnails (sample data has none), live scan.
5. Status: NOT FINISHED, uncommitted (user asked to hold the commit).
