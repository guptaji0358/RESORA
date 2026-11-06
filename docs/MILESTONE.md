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
