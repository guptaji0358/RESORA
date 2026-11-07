# Known issues, limitations, past problems

## Open / limitations [R docs/MILESTONE.md]
- Duplicate Finder: group list elides long summaries; empty folder lists have no placeholder text.
- Properties/Settings dialogs repeat the title in title bar and eyebrow (left as is).
- Not inspected: display scaling other than 125%, multi-monitor, high-contrast, keyboard traversal after layout changes, real-app hover/pressed/focus, live scans, very large real collections, 360 px real width.
- Dashboard hover/click/keyboard tested by code only. Duplicate false-match rate on real photos at threshold 90 unmeasured.
- Properties table vs real EXIF/ICC/DPI files unverified.
- Source-state caveat: an automated run's log doesn't record exact source state.
- Env: `py -3` is broken on this machine (stale 3.15t registration); use `python`. MSYS2 `g++` on PATH is the wrong ABI. [V]
- Memory-system: no token savings measured.

## Past problems (fixed) [R]
- Stray Enter activated implicit default buttons / closed dialogs (autoDefault ChamferButton, QLineEdit ignoring Enter). Fixed: autoDefault off, keyboard-focus-only activation, neutral focus anchor.
- ChamferButton measured with a different font than painted → squeezed labels. Fixed.
- Layout defects listed under "UI Refinement" in docs/MILESTONE.md (fixed).
- Dashboard axis labels 0/0/1 and blank gap above Detailed page (fixed, found by rendering).
- Dialogs once opened off-screen / unusable controls → frameless + `placeOnScreen`.

## Confirmed defects now
None confirmed (per docs). Insights redesign unassessed [?].
