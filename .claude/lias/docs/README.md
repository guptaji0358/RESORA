# LIAS (Local Intelligence & Automation System) — v0.1, Stage 3

Everything lives in `.claude/lias/`; Claude Code requires `settings.json` and `skills/` directly under `.claude/`.

| Piece | Path |
|---|---|
| CLI (single entry) | `lias.py` — `run`, `list`, `know add/search/forget/stats`, `doctor` |
| Core | `core/` — `protocol` (JSON envelope), `guard` (root confinement, secret scan), `store` (SQLite+FTS5), `runner` |
| Automations | `automations/<name>.json` (manifest) + `<name>.py` (stdin JSON -> stdout JSON) |
| Hook | `hooks/prompt_retrieve.py` (UserPromptSubmit, registered in `.claude/settings.json`) |
| Skill | `.claude/skills/lias/SKILL.md` (the only one; description costs context every turn) |
| Runtime data (git-ignored) | `database/knowledge.db`, `database/backups/`, `cache/`, `logs/events.jsonl` |

## Behaviour
- **Runner statuses:** ok, invalid_input, needs_confirmation, missing_dependency, timeout, failed, error. Output is verified to be a JSON object; over 20k chars it is truncated. Results are cached per manifest `cache_ttl_s`, keyed on args and file mtimes.
- **Knowledge:** duplicates are detected by hash. A new confirmed value for the same category/key supersedes the old one, which is kept as `historical`. Unverified info cannot override confirmed. `ttl_days` sets expiry. Secrets are refused. `forget` is a soft delete.
- **Retrieval:** FTS5 BM25 with porter stemming, then a coverage filter (hook requires 60% of the prompt's content words). No match means nothing is injected. Facts are injected as data, max 3 and 1200 chars.
- **Recovery:** the DB is integrity-checked on open. If corrupt, it is moved to `knowledge.corrupt-<ts>.db` and the newest healthy backup is restored (a snapshot is taken after every write, 7 days kept). Without a backup a fresh DB is created and the quarantined file is kept.
- **Security:** automation paths are resolved inside the project root, automations run via `python -I` with no shell, and knowledge is never executed.

## Add an automation
Create `automations/x.json` + `x.py`, then add a test. Mark anything that changes files `"destructive": true`.

## Tests
`python -m unittest discover -s .claude/lias/tests` (29 tests).

## Not yet done (later stages)
Real-session verification that Claude actually picks the skill and calls the CLI, the measurement harness (`lias stats`, A/B), a PostToolUse logging hook, agents (none created on purpose), and the optional semantic layer.

## Stage 4: live integration results (2026-11-12, `claude -p`, v2.1.322, Sonnet, no slash commands typed)

| Test | Result |
|---|---|
| Stored fact asked in a fresh session | The hook injected it. 1 turn, 0 tool calls, correct answer |
| "Analyze the size of all source files." | Claude loaded the `lias` skill on its own, ran `list`, then `run source_sizes`. Correct output |
| Fact never stored ("physics lab room") | Claude searched, answered "I don't know" and offered to store it. No guessing |
| "My chemistry lecture is in Room 12..." | Claude invoked the skill and ran `know add` (id 2) with no slash command |
| New session after that | The hook injected the stored fact. 1 turn, 0 tool calls |

Not tested live: contradiction through Claude (unit-tested only), hook behaviour in the desktop app (only headless `-p`), and many varied prompts. Skill matching is probabilistic, so these are samples, not guarantees. Hook recursion is impossible by design (hooks never invoke Claude, and the hook writes only to its own log). Each prompt fired it once.

### Measurement (n=1 per arm, noisy; NOT proof of savings)
Same prompt "Analyze the size of all source files.":

| | turns | output tokens | cost (USD) |
|---|---|---|---|
| LIAS (skill + automation) | 5 | 995 | 0.102 |
| Baseline (`--disable-slash-commands`, Bash/Glob/Grep/Read) | 2 | 1556 | 0.192 |

LIAS used more turns and tool calls here. The baseline solved it with one shell pipeline. The lower cost may come from fewer output tokens and cache effects. Repeat runs are needed before concluding anything. Honest takeaway: LIAS gives deterministic, reusable, verified results. The cost benefit is unproven.

## Measurement harness (`bench/harness.py`)
`python .claude/lias/bench/harness.py --reps 3` runs each task in `bench/tasks.json` through `claude -p` with LIAS on (skill + hook) and off (`--disable-slash-commands`, `LIAS_DISABLE=1`, plain Bash), arms alternating per rep. It records turns, tool calls, tokens and cost as reported by Claude Code, wall time, and a regex correctness check. It uses a throwaway knowledge DB (`LIAS_DB`). Each run spends real usage (about $0.03-0.10). `lias.py stats` summarizes the local event log (automation runs, cache hits, retrievals, injected characters).

### Results, 2026-11-12 (3 reps per cell, 12 runs, all 12 answers correct)
| Task | Arm | Cost mean (USD) | Turns | Tool calls | Output tokens | Wall (s) |
|---|---|---|---|---|---|---|
| source_sizes | LIAS | 0.093 | 5 | 3 | 1461 | 19.8 |
| source_sizes | baseline | 0.064 | 3 | 2 | 2162 | 35.8 |
| recall_threshold | LIAS | 0.063 | 1.7 | 0.7 | 304 | 9.4 |
| recall_threshold | baseline | 0.039 | 2.3 | 1.3 | 431 | 10.3 |

**Finding: LIAS did not reduce cost here; it raised it by about 47% (source_sizes) and 62% (recall).** It did reduce output tokens (about -32% and -29%), and source_sizes wall time (-45%). Likely causes, not yet isolated:
- The skill description sits in context on every turn and loading the skill adds a turn.
- The `list` call before `run` adds a turn (5 turns versus 3).
- The repo is small and the baseline answers with one shell pipeline or a single grep, so there is little repeated work for LIAS to remove.

Limits: 3 reps is small, the two tasks are narrow, cost is API-equivalent USD and not subscription credits, and the seeded fact can go stale. Savings would be expected on larger repos and on repeated, expensive discoveries, but that is **unmeasured**.

Next optimization candidates: put the automation list into SKILL.md so `list` is unnecessary, shorten the skill description, then re-run this harness.
