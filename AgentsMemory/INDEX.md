# AgentsMemory — index (read this first)

Persistent project knowledge for RESORA so agents do not re-explore the repo every task. Created 2026-11-06.
Memory is a **map, not a substitute for the code**: when implementation details matter, open the source; if memory and code disagree, investigate and fix the memory.

## Evidence tags (used in every file)
| Tag | Meaning |
|---|---|
| **[V]** | Verified repository fact (file, doc, git log inspected on the date in the file header) |
| **[U]** | Explicit user statement/decision (in a conversation message or CLAUDE.md written by the user) |
| **[R]** | Recovered from repo docs about past work (the conversation itself is NOT available) |
| **[I]** | Reasonable inference, not confirmed |
| **[?]** | Unknown / unconfirmed |

Never promote [I] or [?] to a decision without the user confirming.

## What to read for which task
| Task type | Read |
|---|---|
| Any task | this file, `CURRENT_STATE.md`, `USER_PREFERENCES.md` |
| Backend/engine change | `ARCHITECTURE.md`, `DECISIONS.md` |
| GUI/visual change | `DESIGN_MEMORY.md`, `ARCHITECTURE.md` (GUI part) |
| New feature / scope question | `PRODUCT_REQUIREMENTS.md`, `MILESTONES.md` |
| Build/test/commit procedure | `WORKFLOW.md` |
| Bug or odd behaviour | `KNOWN_ISSUES.md` |
| "Why was X done / rejected?" | `DECISIONS.md`, `PROJECT_HISTORY.md` |

## Files
- `CURRENT_STATE.md` — where the project is now, uncommitted work, next actions (update most often)
- `USER_PREFERENCES.md` — how the user wants to work (hard rules)
- `WORKFLOW.md` — memory-first task procedure, build/test/commit rules
- `ARCHITECTURE.md` — GUI/engine split, modules, protocol, environment
- `PRODUCT_REQUIREMENTS.md` — what RESORA is, features, scope in/out
- `DESIGN_MEMORY.md` — identity, dark-only look, rejected styles, UI corrections
- `DECISIONS.md` — accepted / rejected / deferred, with reasons
- `MILESTONES.md` — status table of every milestone (details stay in `docs/MILESTONE.md`)
- `KNOWN_ISSUES.md` — defects, limitations, past problems
- `PROJECT_HISTORY.md` — chronology from git + docs

## Authoritative detail lives elsewhere (link, don't copy)
`CLAUDE.md`, `docs/ARCHITECTURE.md`, `docs/BACKEND_CONTRACT.md`, `docs/GUI.md`, `docs/MILESTONE.md` (evidence records), `docs/ROADMAP.md`, `docs/ANALYTICS.md`, `docs/DUPLICATES.md`, `docs/IMAGE_ANALYSIS.md`, `Assets/README.md`.

## Rules for editing memory
- Concise; no source code dumps; no secrets/tokens/passwords ever.
- Append/supersede with date, don't silently overwrite history (mark old entries "superseded by …").
- Only meaningful changes (decisions, corrections, milestone/state changes, new limitations), not every trivial edit.
- Keep this memory's token cost low: total target well under ~6k words; link to docs for detail.
- Not measured: any claimed credit saving. No token usage has been measured yet.
