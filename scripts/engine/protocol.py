"""Wire protocol shared by every backend command (see docs/BACKEND_CONTRACT.md).

stdout carries newline-delimited JSON envelopes only; stderr carries diagnostics.
This module is the only place that writes to stdout.
"""
from __future__ import annotations

import json
import sys
import threading
from typing import Any, TextIO

PROTOCOL_VERSION = 1

EXIT_OK = 0
EXIT_FAILURE = 1  # runtime failure or unexpected internal error
EXIT_USAGE = 2  # bad command line
EXIT_INVALID_INPUT = 3  # well-formed command line, unusable input (params file, paths)
EXIT_CANCELLED = 130  # cooperative cancellation


class BackendError(Exception):
    """A reportable failure. `code` is a stable machine-readable string."""

    def __init__(self, code: str, message: str, exit_code: int = EXIT_FAILURE) -> None:
        super().__init__(message)
        self.code = code
        self.message = message
        self.exit_code = exit_code


class Cancelled(Exception):
    """Raised by CancelToken.check() when the GUI asked the command to stop."""


class CancelToken:
    """Thread-safe cancel flag. Long commands call `check()` between work units."""

    def __init__(self) -> None:
        self._event = threading.Event()

    def cancel(self) -> None:
        self._event.set()

    @property
    def cancelled(self) -> bool:
        return self._event.is_set()

    def check(self) -> None:
        if self._event.is_set():
            raise Cancelled()


class Emitter:
    """Writes one envelope per line to stdout, flushed immediately."""

    def __init__(self, out: TextIO | None = None) -> None:
        self._out = out if out is not None else sys.stdout
        self._lock = threading.Lock()

    def _emit(self, envelope: dict[str, Any]) -> None:
        line = json.dumps({"v": PROTOCOL_VERSION, **envelope}, ensure_ascii=True, separators=(",", ":"))
        with self._lock:
            self._out.write(line + "\n")
            self._out.flush()

    def progress(self, done: int, total: int | None = None, message: str | None = None) -> None:
        self._emit({"type": "progress", "done": done, "total": total, "message": message})

    def result(self, data: Any) -> None:
        self._emit({"type": "result", "data": data})

    def error(self, code: str, message: str) -> None:
        self._emit({"type": "error", "code": code, "message": message})


def watch_stdin(token: CancelToken) -> threading.Thread:
    """Cancel on a `cancel` line or on stdin EOF (GUI closed the pipe or died)."""

    def run() -> None:
        try:
            for line in sys.stdin:
                if line.strip().lower() == "cancel":
                    break
        except (OSError, ValueError):
            pass
        token.cancel()

    thread = threading.Thread(target=run, name="stdin-watch", daemon=True)
    thread.start()
    return thread
