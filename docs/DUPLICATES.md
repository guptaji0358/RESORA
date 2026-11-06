# Duplicate Finder (`duplicates` command)

Read-only. Never deletes, moves, renames or rewrites an image. Only the folders/files you name are scanned.

## Sources (params file or CLI)
`folders` (list; CLI positional), `files` (imported collection), `exclude` (folders not entered, with everything below), `recursive`, `similar` (default true), `threshold` (50-100, default 90), `cache_path` (SQLite file). Overlapping sources are de-duplicated by path.

## Exact duplicates
Files are grouped by size (a unique size cannot have a twin and is never read), then hashed with streaming SHA-256. Equal digest = exact group, regardless of name, extension or folder. 0-byte files are counted (`empty_files`) and ignored.

## Visually similar
128-bit dHash (9x9 greyscale, horizontal + vertical gradients, alpha flattened on white, EXIF orientation applied). `similarity = 100 * (1 - hamming/128)`. Threshold 90 = at most 12 differing bits. Categories: >=97 Nearly identical, >=93 Very similar, else Similar. Not proof of identity.
- One representative per exact group is fingerprinted; its `identical_copies` says how many exact twins exist.
- Anchor clustering (no chaining): the first image takes everything within the threshold *of itself*. The anchor is only the comparison base, never a preferred copy.
- Images whose width/height ratio differs by >3% never match (crops, heavy edits). Flat images (no detail) have no fingerprint (`flat_images`).
- Candidates found with pigeonhole slices, so large sets avoid all-pairs comparison.

## Storage
`exact_bytes` = all files in exact groups; `exact_savings_bytes` = sum of size*(count-1) (one copy kept per group). `similar_bytes` = combined size of listed similar images; they are different files and are **not** recoverable space.

## Result
`stats`, `exact_groups[]` (id, sha256, size, count, total_bytes, savings_bytes, files[]), `similar_groups[]` (id, category, lowest_similarity, files[] with similarity/distance/category/anchor/identical_copies), `skipped[]` (unreadable/corrupted, capped at 200), `cache_warning`. Per-file problems are data, never job failures. Progress is per phase ("Hashing", "Comparing").

## Performance
Size pre-filter, one file open at a time, 1 MiB hash chunks, JPEG draft decoding, cache reuse when path+size+mtime are unchanged, cancel checked per chunk/file. GUI: groups list is virtual; at most 100 member rows per group; thumbnails decoded on worker threads.
