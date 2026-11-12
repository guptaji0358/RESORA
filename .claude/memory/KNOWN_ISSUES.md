# Known issues, limitations, past problems

## Open / limitations [R docs/MILESTONE.md]
- Duplicate Finder: group list elides long summaries (no horizontal scrollbar); empty folder lists have no placeholder text. At 960x640 the Duplicates page scrolls vertically.
- Past layout bug (fixed 2026-11-07): metric cards in two separate HBoxes (misaligned, no row gap) and fixed 360 px setup column + scroll area without min width clipped member rows at 960x640. Rule: related cells share one QGridLayout; a column's min width must honour widgets whose minimumSizeHint is their full text (ToggleSwitch).
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

- `.resora` import (Phase 4): no stale cache sweep (crash leaves `doc-*` folders). (Large single-image extraction cancel is now per 1 MiB chunk; fixed.)

## 2026-11-10 Phase 7 known limits [V]
- `.resora` reliability verified on Windows 11 / local NTFS only; network shares, removable drives, FAT, cloud-synced folders, antivirus interference, full disk and power loss are untested. Finalization never deletes the only valid copy but is not power-fail atomic. A stale cache folder may stay up to ~1 h (7 days for pre-Phase-7 markers) or while its pid is reused.

## 2026-11-12 Test environment [V]
- Qt test binaries exit 0xc0000135 under plain ctest unless `D:\Apps\Qt6\6.12.0\mingw_64\bin` and `D:\Apps\Qt6\Tools\mingw1310_64\bin` are on PATH. Run ctest with those on PATH (run.bat is not used).

## 2026-11-12 Low-disk limits [V]
- The free-space check is a snapshot: concurrent source growth is detected (Bundled budget capped at the estimate -> `SourceProblem`; Linked final rehash), not prevented. The 16 MiB reserve is a margin, not a guarantee. Real ENOSPC surfaces as `WriteFailed` (QSaveFile hides the OS code). Real full disk, network/removable drives and power loss untested.

## 2026-11-12 Linked-path / cache limits [V]
- [superseded 2026-11-12 audit] Mapped drive letters ARE now rejected (QueryDosDeviceW + GetDriveTypeW fallback, SUBST followed); cache deletion holds a Windows `DirLock` (no share-delete, reparse-aware). Remaining: local symlink/junction to a network location is undetected before open (OneDrive reparse points make blanket rejection unusable); window between lock release and final rmdir (removes a link only); non-Windows has no lock. UNC paths in old Linked files open as UnsafePath (relink to local). No UI shows `pendingCleanup()`; UI never calls `retryCleanup()`; the pending list dies with the `Session` object on replace/reset, so the startup sweep (owner dead + >1 h) is the accepted fallback. Real SMB/mapped-share and symlink-privilege cases untested (tests use SUBST/DefineDosDevice and junctions).
- [superseded 2026-11-12 cleanup ownership] The pending-cleanup list no longer dies with the Session: it lives in the shared `CacheCleanupManager` and is retried at replacement, reset, Open dialog close and shutdown. Remaining: the list is in-memory (a folder still pending at exit is recovered by the startup sweep only, when its owner pid is dead and it is older than 1 h); a folder refused for another live process stays pending until exit; retries run on the GUI thread (small, bounded); no UI shows pending folders.
