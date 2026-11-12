# Milestones (status as recorded in docs/MILESTONE.md; evidence stays there)

| Milestone | Status | Automated evidence [R] | Manual check |
|---|---|---|---|
| Engine foundations (protocol, scan, classification, reports) | Implemented (git history) | 69 tests at GUI polish run | n/a [?] |
| GUI v1 → Darkroom identity, RESORA branding, import, header/rail | Implemented (git history) | — | [?] |
| GUI polish: Saved Locations, SVG assets, frameless dialogs, Enter-key fix | **FINISHED** | 69 py tests, ctest 6/6 (2026-11-05 run) | User verified 2026-11-06 (scenarios not itemised) |
| 1 Advanced Image Properties | Implemented; no separate MILESTONE.md section [V] | tests/test_properties.py, tst_properties | [?] |
| 2 Resolution Analytics | NOT FINISHED | 100 py tests, ctest 8/8 | Not done |
| 3 Image Comparison | NOT FINISHED | 100 py, ctest 9/9 | Not done |
| 4 Duplicate Finder | NOT FINISHED | 128 py, ctest 10/10 | Not done |
| UI Refinement (layout/spacing) | NOT FINISHED | ctest 10/10; real renders inspected via grab_screens (sample data) | Awaiting review |
| Analytics Dashboard (Dashboard + Detailed) | NOT FINISHED | ctest 11/11; renders inspected | Awaiting review |
| Comparison Insights redesign (uncommitted) | **[?] unknown / in progress**; not in MILESTONE.md | tst_insights.cpp exists; no recorded run | Not done |
| Independent feature organization (ToolRail, 2026-11-07) | NOT FINISHED | ctest 12/12, py tests pass | Not done |
| Fix: Duplicates page layout (2026-11-07) | NOT FINISHED | ctest 12/12; hosted-page renders inspected at 1180x800 and 960x640 (sample data) | Awaiting user review |

Roadmap (docs/ROADMAP.md): all four features implemented, 2–4 awaiting review. Next milestone: none approved [U: don't start before approval].
| Redesign: Duplicates page structure (2026-11-07) | NOT FINISHED | ctest 12/12; renders inspected at 1180x800 and 960x640 | Uncommitted; awaiting user review |
| Redesign: Duplicates results as group hierarchy (2026-11-08) | NOT FINISHED | ctest 12/12; renders inspected (1 group, many groups, similar, empty) | Uncommitted; awaiting user review |
| `.resora` Phase 1: file format foundation (2026-11-08) | NOT FINISHED | ctest 13/13 (incl. tst_resorafile) | Uncommitted; awaiting user review; implement.md tracker 0/7 |
| `.resora` Phase 1 (update 2026-11-08) | COMPLETED (user approved) | — | superseded row above |
| `.resora` Phase 2: bundled export (2026-11-08) | COMPLETED (user-approved) | tst_bundledexport 27/27 (incl. ReplaceFileW 1175/1176/1177 handling via test seam), full ctest 14/14 (196 tests) | Uncommitted; awaiting user verification |
| `.resora` Phase 3: linked export (2026-11-08) | COMPLETED (user-approved 2026-11-08; earlier row text said in progress) | tst_linkedexport 12/12 (final source rehash), full ctest 15/15 | Uncommitted; awaiting user verification |
| `.resora` Phase 4: import/opening engine (2026-11-08) | NOT FINISHED | tst_resoraimport 16 passed; ctest 16/16 | Phase 4 approved by the user 2026-11-08 (implement.md COMPLETED); manual verification still not done |
| `.resora` Phase 5: dashboard integration (2026-11-08) | COMPLETED (user-approved 2026-11-09) | tst_dashboardstate 15 passed (after runtime-path correction); ctest 17/17 | Implemented, uncommitted; awaiting user review |
| `.resora` Phase 6: export/import UI (2026-11-09) | COMPLETED (user-approved 2026-11-10) | tst_resoraui 14 passed; ctest 18/18 | Uncommitted; not visually verified |
| `.resora` Phase 7: compatibility/reliability (2026-11-10) | IN PROGRESS | full ctest 21/21 (2026-11-12, after low-disk work; earlier 20/20 on 2026-11-11/12 before tst_exportlowdisk existed); tst_resorareliability 10; tst_exportlowdisk 22 cases | Uncommitted; not manually verified |
