# Four-feature roadmap (technical plan)

Approved order: **1 Advanced Image Properties → 2 Resolution Analytics → 3 Image Comparison → 4 Duplicate Finder.**
One milestone per change; each waits for review. Status: Milestones 1-4 are implemented (4 awaiting review). Definitions: [ANALYTICS.md](ANALYTICS.md).

## Reusable components (inspected)
- Engine: `imaging/metadata.py` (header read, error classification), `imaging/analyzer.py` (`ImageRecord`), `aspect.py`, `classification.py`, `selection.py` (path de-dupe), `reports/summary.py` (aggregation), `protocol.py` (NDJSON, `BackendError`, `CancelToken`), `command.py`/`registry.py` (add a command = one module + one registry line).
- GUI: `BackendProcess` (QProcess + NDJSON + cancel/stall), `FramelessDialog` (shared dark window), `ChamferButton`, `SectionCard`, `MetricCard`, `DistributionBar`, `Theme`/`Icons`, `ImportSelection`.
- Reports, Saved Locations, import flows, assets/qrc: untouched by all four milestones.

## Milestones
| # | Feature | Engine | GUI | Notes |
|---|---|---|---|---|
| 1 | Advanced properties | `imaging/properties.py`, `imaging/exif_fields.py`, `commands/properties.py` (`properties`) | `backend/propertiesmodel`, `widgets/propertiesdialog` (Ctrl+I) | Header-only; missing metadata = `null`, never 0 |
| 2 | Resolution analytics | `imaging/analytics.py` + `analytics` command over the same file list (extends `reports/summary` ideas without changing CSV/TXT) | analytics panel on the Readout phase (cards + `DistributionBar`s) | Separate "unavailable" from 0; average = mean of pixels/MP, documented |
| 3 | Image comparison | `compare` command: two `properties` payloads + deltas | `CompareDialog` with a zoom/pan synchronized viewer using `QImageReader` (scaled read, read-only) | Previews decoded by Qt only for display; originals never written |
| 4 | Duplicate finder | `hashing.py` (SHA-256 streaming), `perceptual.py` (dHash/pHash, Pillow only), `duplicates` command, progress + cancel | results list grouped as *Exact* vs *Visually similar* | Never deletes or modifies; future delete needs explicit confirmation |

## Cross-cutting rules
- New commands follow BACKEND_CONTRACT (progress, cancel checks, no stdout prints, no Qt); additive only, so `PROTOCOL_VERSION` stays 1.
- Per-image problems are data (`status != "ok"`), not job failures.
- Dark-only RESORA styling via `Theme` roles; no stock Windows/Fluent widgets.
- Each milestone: engine tests, GUI tests, docs update, `commit.txt`, MILESTONE.md entry only after user verification.
