# Image analysis (`scan` command)

```
python -m scripts.engine scan FOLDER [--recursive] [--csv PATH] [--txt PATH]
```
`folder`, `recursive`, `csv`, `txt` may instead be keys of the `--params-file` object (command line wins). Non-recursive by default. Result shape and error codes: docstring of `scripts/engine/commands/scan.py`. stdout carries only the summary; the per-file table is the CSV (contract section 7), so a GUI that needs rows must request `--csv`.

## Selected files (single / multiple images)
Params file: `{"files": ["C:\a.png", ...]}` (instead of `folder`; both together is `invalid_input`; an empty/non-list/non-string `files` is `invalid_input`). Command line: `python -m scripts.engine --params-file P.json scan [--csv ..] [--txt ..]`. Rules:
- `recursive` does not apply (result reports `false`); no extension filtering: every listed file gets a row, so a missing file is `unreadable`, a non-image `unsupported`/`corrupted`. Nothing is dropped silently.
- Paths are made absolute; exact duplicates (case-insensitive on Windows) are collapsed and counted in `duplicates_ignored`.
- Same analyzer, summary, progress (`total` = unique files), cancellation and CSV/TXT writers as folder scans. TXT shows `Source: N selected file(s)` instead of `Folder:`/`Recursive:`.
- Result additions (backward compatible, protocol v1 unchanged): `source` (`"folder"|"files"`), `selected` (N or null), `duplicates_ignored`; `folder` is `null` for file scans.

## Modules
`imaging/discovery.py` (find candidate files in a folder) / `imaging/selection.py` (normalise an explicit file list) -> `imaging/metadata.py` (Pillow header read) -> `imaging/aspect.py` + `imaging/classification.py` (pure) -> `imaging/analyzer.py` (one file -> `ImageRecord`). `reports/*` consume plain dicts only (`ImageRecord.to_dict()`), never images. `commands/scan.py` orchestrates and owns progress/cancel.

## What is read
- Pillow opens the file read-only and reads the header only (`Image.open`, no `load()`); sources are never modified or re-encoded. Pillow's decompression-bomb limit is lifted during the header read (and restored), so very large valid images are not rejected.
- Candidates are files whose extension Pillow can open (JPEG, PNG, WebP, BMP, TIFF, GIF, and anything else the build registers; AVIF only if the build has a decoder) plus `.avif/.heic/.heif/.jxl`, which are reported as `unsupported` when not decodable. Other files are skipped and counted (`skipped_non_images`). Symlinked directories are not followed.
- **EXIF orientation**: orientations 5-8 swap width and height, so dimensions are the *displayed* ones (`exif_rotated` says so).
- **Multi-frame images** (animated GIF/WebP, multi-page TIFF): the primary (first) frame's size is reported. Frames are not counted or seeked.
- Header-only means damage in pixel data (e.g. a truncated file with an intact header) is **not** detected.

## Statuses
`ok`; `unsupported` (image-like extension, no decoder); `corrupted` (extension is a supported image type but the content is not a valid image); `unreadable` (OS refused: permissions, vanished, locked). A bad file is a row with `error` set; it never stops the scan.

## Resolution classification
Standards (long x short side): HD 1280x720, Full HD 1920x1080, DCI 2K 2048x1080, QHD / 1440p 2560x1440, UHD 4K 3840x2160, DCI 4K 4096x2160, 5K 5120x2880, 8K 7680x4320.

1. Sides are sorted into (long, short), so portrait and landscape classify the same. Width alone is never used.
2. `exact` -> the standard's name. `near` (both sides within 2% of the closest standard, e.g. 1920x1088) -> `"Full HD (approx.)"`.
3. Anything else is never given a standard's name:
   - aspect ratio above 2.4:1 -> `Panorama (>= <tier>)`
   - larger than 8K by over 2% in **both** sides -> `Above 8K`
   - otherwise `Custom (>= <tier>)`, or `Custom (below HD)`.

   `tier` is the largest standard that fits wholly inside the image. Example: 12000x1000 is `Panorama (>= HD)`; 8192x4320 (DCI 8K) is `Custom (>= 8K)`.

The record always carries the true `width`/`height`, `aspect_ratio` (width/height), `aspect_label` (named ratio within 1%, else e.g. `1.90:1`), `resolution`, `resolution_match` (`exact|near|none`) and `resolution_tier`.

## Reports
CSV: UTF-8 with BOM, one row per file including failures, fixed columns (`reports/csv_report.py:COLUMNS`). TXT: header, summary, breakdowns, file table, problem files. Both are written to a temp name and renamed.

## `properties` command (Milestone 1)
`python -m scripts.engine properties <path>...` (or params `{"files": [...]}`, max 500) returns `{"images": [...], "duplicates_ignored": N}`.
Per image (`imaging/properties.py`, header-only, read-only): file (name, path, size, format), displayed + stored dimensions, megapixels,
aspect ratio/label, layout (portrait/landscape/square), resolution class, colour (mode, bit depth, alpha, ICC profile name),
DPI (JFIF/PNG/TIFF header, else EXIF XResolution with unit inch/cm; unit "none" is not DPI), EXIF fields (`imaging/exif_fields.py`),
EXIF orientation (tag, label, whether it swapped width/height), frame count for multi-frame files.
Absent metadata is `null` (never 0); a bad file is an entry with `status != "ok"`. GPS coordinates are never exposed, only `gps_present`.
GUI: `PropertiesDialog` (Ctrl+I, or "Image Properties" on the Readout page) via `backend/propertiesmodel`.
