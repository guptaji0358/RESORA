# Design memory

## Identity [V docs/GUI.md]
- Product RESORA (RES from resolution + ORA). Mark = "resolving grid" quadtree: coarse dim cell top-left subdividing into finer, brighter cells to one ember pixel (source `Assets/branding/mark.svg`). Wordmark Bahnschrift Bold, wide tracking, RES paper / ORA ember.
- "Darkroom" look: warm black `#0D0C0B`, paper-white text, single ember accent `#FF5B2E`, lime = success, rose = failure; Bahnschrift display, Cascadia Mono for data.
- **Dark-only**: no light theme, no toggle. [U][V]
- Frameless window with custom caption buttons, phase rail, chamfered (clipped-corner) buttons (`ChamferButton`), `ToggleSwitch`, Viewfinder, DistributionBar ("resolution ladder").
- Colour is never the only state cue (state always also written as a word). [V]

## Rejected / avoid [U][V]
- Stock Windows/Fluent-style widgets or look (roadmap: "no stock Windows/Fluent widgets"). Native file dialogs are the accepted exception.
- Splash screen (rejected: app starts instantly, would only add delay). [V]
- Qt Charts and Matplotlib for the dashboard (licence/extra module/stock look; extra Python dependency + render process) → custom QPainter charts. [V]
- Stylesheets per widget: style by dynamic property via `Theme` instead. [V]

## Assets [V]
SVG icon family in `Assets/icons` (24 px grid, 1.5 stroke, mitred joins, one chamfered corner, one ember pixel accent). Only two colours. Add icon = SVG + one `<file>` in `resora.qrc`; `tst_icons` guards. Missing asset → visible dashed red fallback box.

## Dialogs [V]
Shared `FramelessDialog` base: custom title bar, Esc closes, drag via system move, `placeOnScreen` (centre over parent, clamp to screen). Enter must never trigger an implicit default button (past bug; see KNOWN_ISSUES).

## UI corrections found in past work [R docs/MILESTONE.md "UI Refinement"]
Compare workspace buried in outer scroll; viewfinder pushed Cancel off screen; action rows squeezed buttons (fixed with `FlowLayout`, fixed label font in ChamferButton); report path elided by buttons; duplicate finder dialog layout; doubled "%%" in image pane. Layout: per-page scrolling, centred 860 px column.

## Dashboard design [V]
Bars (resolution classes), donuts + text legend (orientation, formats), horizontal bars (storage by format), tiles; labels/counts/percentages repeated as text; hover exact values; click/keys select category; empty states per chart.

## Unknown [?]
Specific user design rejections from earlier conversations (beyond what docs record) are not recoverable from the repo.

## 2026-11-07 Navigation
ToolNav in the title bar (AppHeader; icon + name, ember underline on active, icon-only below 1160 px; replaced the left ToolRail 2026-11-07 per user) = app navigation; Analyzer's horizontal step strip (PhaseRail) = Analyzer-internal only. Hand-offs are contextual "Open in …" actions, not primary promo buttons.

## 2026-11-07 Duplicates page structure [U]
Sections must be separated, not crowded: header (title + description), setup column (Sources, Options; action pinned), results (grouped stat rows with headings, tabs, groups|members). Long paths elide, no horizontal scrollbars in lists.
