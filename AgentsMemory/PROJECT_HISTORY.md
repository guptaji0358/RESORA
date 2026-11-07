# Project history

Source: `git log` (17 commits to 04c9e02) and docs. **The original conversations are not in the repo**; only what commits/docs reveal is listed. Dates of early commits not recorded here [?].

1. `ba16a07` Initial commit; `e115604` legacy `image_analyzer.py` replaced by `main.py` Tkinter prototype (reference for classification rules).
2. Engine: `0805bc2` NDJSON protocol/CLI/registry → `3ef3c0d` header-only metadata + discovery → `f2c1b3d` classification/aspect → `fe6d7fd` scan command → `3816173` CSV/TXT reports.
3. Build: `98fec1a` run.bat + requirements.
4. GUI: `1ce8cff` skeleton/theme → `6a21dff` QProcess + parser → `f32c1f5` report selectors → `00cf0be` progress/cancel → `3e62e60` results/metric cards → `0c7af43` frameless header, phase rail, chamfer buttons ("Darkroom").
5. `6c6c010` RESORA product identity → `b2bf4c7` import folders/images (Ctrl+O), `run.bat /push`, workflow rules.
6. `8f42880` Duplicate finder (roadmap milestone 4) → `04c9e02` UI refinement.
7. No separate commits exist for GUI polish, milestones 1–3 or the dashboard in `git log`; `commit.txt` holds their message blocks, and the working tree no longer shows milestone 1–3 files as modified, so [I] they were folded into earlier commits (e.g. `8f42880`). Exact grouping is [?]. GUI polish was user-verified 2026-11-06. Dashboard and comparison-insights work are uncommitted (see CURRENT_STATE.md).
8. 2026-11-06: AgentsMemory system requested by user to cut repeated context-gathering cost; created.
