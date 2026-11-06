# Resolution analytics (Milestone 2)

Engine: `scripts/engine/imaging/analytics.py` (pure, one streaming pass over per-file records),
`imaging/collection.py` (folder/file-list gathering + header analysis shared by `scan` and `analytics`),
`commands/analytics.py`. GUI: phase **04 Analytics** (`widgets/analyticspanel`, `sharechart`, `pixelspan`,
`backend/analyticsmodel`). The GUI computes nothing; it renders the engine's numbers.

## Where the data comes from
- `scan` adds an `analytics` object to its result (computed from the records it already read: no extra pass).
- `analytics` command (`python -m scripts.engine analytics <folder> [--recursive]` or params `files`) returns
  the same object without writing reports. The Analytics view's **Refresh** runs it on the *currently
  imported* collection. Opening the view never scans anything.
- Headers only (Pillow, no pixel decoding); images are never modified.

## Definitions
- **Collection**: unique paths (case-insensitive on Windows). Duplicates are counted once and reported in `duplicates_ignored`.
- **Analyzed**: `status == "ok"` **and** width and height are positive integers. Everything else is **failed**
  (`corrupted`, `unsupported`, `unreadable`, or `invalid_metadata` for an "ok" record with missing/non-positive size).
  Failed files feed no average, extreme, distribution or storage total (their bytes appear only as `failed_bytes`).
- Missing values are never 0: unknown file size excludes the image from storage figures (`size_unknown`);
  empty sets give `null` (shown "Not available").
- **Total storage** = sum of file sizes of analyzed images with a known size. **Average file size** = total storage ÷ images with known size.
- **Average width / height** = arithmetic mean over analyzed images of the *displayed* dimensions (EXIF rotation applied).
- **Total megapixels** = Σ(width × height) ÷ 1,000,000. **Average megapixels** = total pixels ÷ analyzed ÷ 1,000,000
  (mean of per-image exact megapixels; not the rounded per-file value). Total ≠ average.
- **Highest / lowest resolution**: by pixel count (width × height); dimensions shown; ties → smallest path, with `tied` = how many images share that count.
- **Most common resolution**: exact displayed WxH with the most images; ties → larger pixel count, then smaller width; `tied` = number of sizes sharing the top count.
- **Categories** (denominator = analyzed): HD, Full HD, DCI 2K, QHD / 1440p, UHD 4K, DCI 4K, 5K, 8K, Above 8K, Below HD / Other.
  Taken from the existing classifier output (`resolution_match` exact/near → that standard; tier "Above 8K"; otherwise
  "Below HD / Other", which also holds non-standard sizes such as custom or panorama). No rules are duplicated.
- **Orientation** (denominator = analyzed): landscape (w > h), portrait, square, from displayed size.
- **Formats** (denominator = analyzed): Pillow format normalised: JPEG+MPO → JPEG, PNG, WebP, GIF, BMP, TIFF; all others → "Other"
  with `other_formats` detail. Each format also has `bytes` and `bytes_percent` (denominator = total storage).
- Percentages are `count × 100 / denominator`, rounded to 2 decimals (shown with 1); they may not add to exactly 100.
  Zero denominator → `null`.
- Totals are Python integers (no overflow); JSON integers are read exactly (64-bit) by the GUI. Memory is O(distinct sizes);
  at most 50 failures are listed (`failures_truncated`).

## Limitations
- Animated / multi-frame images count once with first-frame dimensions (frames are not counted, as that can need decoding).
- Exactly representable up to 2^53 in JSON consumers other than the GUI.
- CSV is unchanged. TXT gets an **optional** "Analytics summary" section (off by default; `--txt-analytics` /
  GUI switch "Add analytics summary to the TXT report"); without it the TXT is byte-identical to before.
