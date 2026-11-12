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
