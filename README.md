# Image Resolution Analyzer

Windows desktop tool (Python 3.11+, Tkinter, Pillow) that scans a folder, reads
each image's real pixel dimensions from its header (pixels are not decoded),
classifies the resolution and exports CSV / TXT reports. Original images are
only opened read-only and are never modified.

## Install (Windows, PowerShell)

```powershell
cd path\to\this\folder
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

If activation is blocked: `Set-ExecutionPolicy -Scope Process Bypass`, then activate again.
Tkinter ships with the standard python.org installer.

## Run

```powershell
python image_analyzer.py
```

1. **Browse Folder** and pick a folder (tick *Include subfolders* for recursive scan).
2. **Start Scan**. Results appear live; **Stop Scan** keeps what was found so far.
3. Click a column heading to sort (click again to reverse). Select any row.
4. **Export CSV** / **Export TXT** save the table in its current sort order.

## Classification

Based on actual pixel size after applying EXIF orientation. Orientation is
ignored (portrait is treated like landscape).

| Class | Reference size |
|---|---|
| SD | 640 × 480 |
| HD | 1280 × 720 |
| Full HD | 1920 × 1080 |
| QHD / 1440p | 2560 × 1440 |
| 4K UHD | 3840 × 2160 |
| 5K | 5120 × 2880 |
| 8K UHD | 7680 × 4320 |
| Above 8K | larger than 8K UHD |

**Non-standard sizes:** an image gets the highest tier whose width *and* height
it both reach. So 4000×3000 is 4K UHD, and 2560×1080 (ultrawide) is Full HD
because its height doesn't reach 1440. Anything below 640×480 in either
dimension is *Below SD*. Above 8K means it meets 8K in both dimensions and
exceeds it in at least one.

**DCI 2K vs QHD:** 2048×1080 is reported as `Full HD (DCI 2K)`; it is the cinema
format and is not the same as 2K/QHD 2560×1440.

**Aspect ratio:** 16:9 is never assumed. Ratios within 1% of common ones are
named (4:3, 3:2, 16:10, 21:9 ...); others are shown as e.g. `3:1 (ultrawide)` or
`1.43:1 (unusual)`; portrait images are marked `(portrait)`.

## Notes

- Supported: every format Pillow can open (JPG, PNG, WEBP, BMP, TIFF, GIF, ICO, ...).
  PDF/EPS/PS are skipped.
- Corrupt or unreadable files are counted as *Failed* and listed in the TXT report.
- Files with the same name in different folders are all listed (full path shown).
- Scanning runs in a background thread so the window stays responsive.
- CSV is UTF-8 with BOM so Excel shows non-ASCII filenames correctly.
