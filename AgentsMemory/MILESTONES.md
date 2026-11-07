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
