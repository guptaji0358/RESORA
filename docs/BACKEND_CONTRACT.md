# Backend CLI Contract (protocol v1)

The GUI and backend share only this contract. Implementation: `scripts/engine/protocol.py` (wire format), `cli.py` + `main.py` (CLI), `registry.py` (commands); module map in [ARCHITECTURE.md](ARCHITECTURE.md). Verified by `tests/test_contract.py`.

## 1. CLI

```
python -m scripts.engine [--params-file FILE] [--cancel-on-stdin] <command> [command args]
```

- One process = one command = one job. No daemon, no shared state between invocations.
- Simple inputs are command-line arguments. Structured or long inputs (many paths, option sets) go in a UTF-8 JSON **object** file passed with `--params-file`; the GUI writes it to a temp file and deletes it afterwards. Avoids Windows command-line length/quoting limits.
- Visible commands: `selfcheck` (legacy alias `--selfcheck`), `scan` ([IMAGE_ANALYSIS.md](IMAGE_ANALYSIS.md)). Commands starting with `_` are diagnostics, hidden from help, not for the GUI.
- `scan` accepts either a folder or, via the params file, `{"files": [...]}` (selected images; see [IMAGE_ANALYSIS.md](IMAGE_ANALYSIS.md)). The GUI uses the params file for every file selection, so path count never hits command-line limits.
- Adding flags/commands is backward compatible; renaming or removing them, or changing result shapes, requires bumping `PROTOCOL_VERSION`.

## 2. stdout: newline-delimited JSON envelopes (and nothing else)

Each line is one compact, ASCII-only JSON object, flushed immediately:

| `type` | Fields | Meaning |
|---|---|---|
| `progress` | `done` (int), `total` (int or null), `message` (string or null) | Zero or more, before the final line |
| `result` | `data` (any JSON) | Success. Exactly one, always last |
| `error` | `code` (string), `message` (string) | Failure. Exactly one, always last |

All envelopes carry `"v": 1`. A run ends with exactly one `result` or `error`. Only `scripts/engine/protocol.py` writes to stdout; `print()` in command code is forbidden. A process that exits without a final envelope (crash, kill, timeout) is a failure the GUI must synthesize itself.

## 3. stderr

Free-form UTF-8 diagnostics for humans/logs (tracebacks, warnings). Never parsed. Every failure also writes a one-line reason to stderr. The GUI keeps the tail (e.g. last 64 KB) and shows it on failure.

## 4. Exit codes and errors

| Exit | Meaning | `error.code` |
|---|---|---|
| 0 | success (`result` emitted) | — |
| 1 | runtime failure / unexpected exception | command-specific, or `internal` for uncaught exceptions |
| 2 | command-line usage error | `usage` |
| 3 | invalid input (bad params file, missing/unreadable path) | `invalid_input` |
| 130 | cancelled | `cancelled` |

The exit code is the authority on success; the `error` envelope supplies detail. Commands raise `BackendError(code, message, exit_code)`; they never catch-and-hide. `code` values are stable identifiers the GUI may switch on; `message` is for display only. Per-item problems that should not abort a job (e.g. one unreadable image in a scan) belong in the `result` data, not as a job failure.

## 5. QProcess usage (GUI side)

- Program `%IMGAN_PYTHON%`, arguments `-m scripts.engine --cancel-on-stdin <command> ...`, working directory `%IMGAN_BACKEND_DIR%`. Set `PYTHONIOENCODING=utf-8` in the process environment. Use `setProgram`/`setArguments` (no shell string).
- Keep one `BackendJob` object per run (QObject wrapping a QProcess): signals `progress(done,total,message)`, `finished(result)`, `failed(code,message,exitCode)`. Never use `waitFor*` on the GUI thread.
- Read with `readyReadStandardOutput`: append to a buffer, split on `\n`, parse each complete line as JSON, keep the partial tail. A non-JSON stdout line is a protocol violation → fail the job.
- Collect `readyReadStandardError` into a bounded ring buffer.
- Completion: `finished(exitCode, status)`. Success = exit 0, normal exit and a `result` envelope. `errorOccurred(FailedToStart)` → failed with code `spawn_failed`. Crash/kill/no final envelope → `crashed`.
- Several jobs may run concurrently; they are independent processes.

## 6. Cancellation and timeouts

- **Cooperative cancel:** GUI writes `cancel\n` to the process stdin. The backend (started with `--cancel-on-stdin`) sets its `CancelToken`; commands call `ctx.cancel.check()` between work units and the run ends with `error{code:"cancelled"}`, exit 130.
- **Orphan protection:** stdin EOF means the same as `cancel`, so a backend never outlives a crashed GUI that owned its pipe.
- **Escalation:** after sending `cancel`, wait a grace period (default 3 s), then `QProcess::kill()`. Do not rely on `terminate()` (it cannot stop a console-less Windows process). A cancelled job is reported as cancelled whatever the exit status.
- **Timeouts are GUI-owned.** Start a `QTimer` per job; on expiry report `timeout` and run the same cancel → grace → kill sequence. Defaults: `selfcheck` 30 s; long jobs (scans) have no total timeout but a **stall timeout** (no stdout line for 60 s) so long as they emit progress. Commands must emit progress at least every ~5 s during long work.
- Killed processes may leave partial output files; commands must write result files to a temp name and rename on completion, and the GUI treats files from failed/cancelled jobs as invalid.

## 7. Large outputs and progress

- **Progress:** throttle to at most ~10 envelopes/s; `done`/`total` counts work units (files), `total` null when unknown. Progress is advisory; the final envelope is authoritative.
- **Large results:** stdout stays small (envelopes are expected to be well under 1 MB). Bulk data (per-image tables, reports) is written to a file at a path given by `--output PATH` (or `params`), and the `result.data` carries the path and summary counts (`{"output": "...", "items": N}`). The GUI reads that file (off the GUI thread) after `finished`.
- For incremental data, prefer progress-style batches over one giant document; never emit a single stdout line larger than ~1 MB.

## 8. Independence from Qt

The engine imports no Qt/GUI toolkit (checked by a test) and resolves nothing relative to the working directory. Everything above can be driven from a plain terminal:

```
python -m scripts.engine selfcheck
python -m scripts.engine --cancel-on-stdin _contract-test progress --steps 5 --delay 0.2
```

## 9. Adding a command

1. Create `scripts/engine/commands/<name>.py` exporting `COMMAND = Command(name=..., help=..., run=run, add_arguments=...)`. `run(args, ctx)` returns the result `data`, uses `ctx.emit.progress(...)`, `ctx.cancel.check()`, `ctx.params`, and raises `BackendError` for expected failures. Document its args/result shape in the module docstring.
2. Register it: one explicit import and one list entry in `scripts/engine/registry.py:build_registry`. No dynamic imports or path hacks.
3. Put reusable logic in its own module/package (see [ARCHITECTURE.md](ARCHITECTURE.md#adding-a-module)); the command module is a thin adapter. Commands never import each other, never write to stdout, never import Qt.
4. Add tests in `tests/` (logic directly, plus one subprocess test through this contract).
