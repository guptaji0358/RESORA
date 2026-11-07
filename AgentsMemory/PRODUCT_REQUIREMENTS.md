# Product requirements

## What RESORA is [V]
Name **RESORA**, tagline "See every pixel clearly." Subtitle "Image Resolution Analyzer". Windows desktop tool: scan a folder (or imported single/multiple images), read each image's pixel dimensions from the header (**no pixel decoding** for analysis), classify resolution (SD … 8K+), export CSV/TXT reports. Original images are opened read-only and never modified.
Intended users: [?] not stated in the repo; [I] people auditing photo/image collections for resolution.

## Features (implemented; status in MILESTONES.md) [V]
1. Scan: folder/recursive, progress, cancel, CSV and TXT reports (independent, overwrite confirmation).
2. Import: folder, single image, multiple images (Ctrl+O), drag-drop.
3. Saved Locations (GUI-only) with path suggestions.
4. Advanced Image Properties (`properties`): dimensions, MP, aspect, size, format, colour mode, ICC, DPI, EXIF, orientation; missing = null/"Not available", never 0.
5. Resolution Analytics (`analytics`): collection/resolution/category/orientation/format stats; optional TXT analytics section. Dashboard (charts) + Detailed View.
6. Image Comparison: two images, synced zoom/pan viewer, differences table, insights (no new backend command; reuses `properties`).
7. Duplicate Finder (`duplicates`): exact (SHA-256) and similar (dHash), storage savings; **detect/compare only, no delete/move**.

## Out of scope / rules [V]
- No image-analysis logic in C++; no Qt in the engine; no modifying user images.
- No features without explicit instruction. Legacy `main.py` not extended.
- Future delete in Duplicate Finder would need explicit confirmation (roadmap note), not approved/built.
- Compiled packaging (Nuitka) is a possible later separate task [V].

## Future direction [?]
Only the approved four-feature roadmap is documented (all four implemented). Anything beyond is unconfirmed; ask the user.
