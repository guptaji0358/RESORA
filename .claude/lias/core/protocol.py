"""Structured JSON envelope shared by every LIAS command. stdout carries only this."""
from __future__ import annotations

import json
import sys
from typing import Any


def envelope(status: str, data: Any = None, error: str | None = None, **extra: Any) -> dict[str, Any]:
    env: dict[str, Any] = {"status": status}
    if data is not None:
        env["data"] = data
    if error is not None:
        env["error"] = error
    env.update(extra)
    return env


def emit(env: dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(env, ensure_ascii=False, separators=(",", ":")) + "\n")
    sys.stdout.flush()
