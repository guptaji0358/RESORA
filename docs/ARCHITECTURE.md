# Architecture

```
Qt GUI (C++)  --QProcess-->  python -m scripts.engine <command> [args]
     ^                              |
     +---- JSON on stdout, logs on stderr, exit code ----+
```

## IPC contract
Full contract (CLI, NDJSON envelopes, exit codes, QProcess, cancellation, large outputs): [BACKEND_CONTRACT.md](BACKEND_CONTRACT.md). One process per command; stdout = JSON envelopes only; stderr = diagnostics; exit code 0/1/2/3/130.

## Locating the engine
Dev: `run.bat` exports `IMGAN_PYTHON` (venv interpreter) and `IMGAN_BACKEND_DIR` (repo root); the GUI runs `%IMGAN_PYTHON% -m scripts.engine` with that working directory. Future packaging: a compiled executable exposing the identical CLI replaces the interpreter call.

## Engine modules (`scripts/engine/`)
One module = one responsibility.

| Module | Responsibility | Depends on |
|---|---|---|
| `__main__.py` | `python -m` entry; calls `main()` | `main` |
| `main.py` | parse, run one command, map outcome to one envelope + exit code | `cli`, `command`, `params`, `protocol`, `registry` |
| `cli.py` | argparse setup, `UsageError` | `command` |
| `params.py` | load the `--params-file` JSON object | `protocol` (errors) |
| `protocol.py` | envelopes, exit codes, `BackendError`, `CancelToken`, stdin watcher; the only stdout writer | stdlib |
| `command.py` | `Command` / `Context` types (no behaviour) | `protocol` |
| `registry.py` | explicit list of commands, duplicate check | `command`, `commands.*` |
| `commands/<name>.py` | one command each, exports `COMMAND` | `command`, `protocol`, logic packages below |
| `imaging/` | discovery, Pillow metadata, aspect, classification (pure), analyzer, records | stdlib, Pillow (metadata only) |
| `reports/` | CSV/TXT/summary/atomic write over plain dicts | stdlib; never `imaging` |

## Dependency boundaries
- Direction: `commands/*` → `command`, `protocol` (and future logic modules) ← never the reverse. Only `registry` imports commands. No cycles.
- Feature/command modules never import each other. Shared needs go in a lower-level module with a single purpose, not a grab-bag `utils.py`.
- No module imports Qt or another GUI toolkit; none writes to stdout except `protocol.py`; no global mutable state (state travels in `Context`).
- Enforced by `tests/test_engine_modules.py`.

## Adding a module
1. Give it one responsibility, plain inputs and outputs (no `argparse`, `Context` or stdout inside pure logic), e.g. `scripts/engine/scanning/headers.py`.
2. It may import stdlib and lower-level engine modules; never a command or a sibling feature.
3. Expose it through a thin `commands/<name>.py` adapter and register it ([BACKEND_CONTRACT.md §9](BACKEND_CONTRACT.md#9-adding-a-command)).

## Testing a module independently
Import it in a test and call it directly, with no subprocess and no GUI (see `tests/test_engine_modules.py`). Commands additionally get one subprocess test via `python -m scripts.engine` (see `tests/test_contract.py`). Run: `.venv\Scripts\python -m unittest discover -s tests`.

## Size guidelines
Most modules 1-150 lines; 151-250 fine; over 250 review whether responsibilities should split; over 350 needs strong justification; avoid 500+. Guidelines, not limits: never split just to save lines.
