"""Loads the JSON params file passed by the GUI (`--params-file`)."""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from .protocol import EXIT_INVALID_INPUT, BackendError


def load_params(path: Path | None) -> dict[str, Any]:
    if path is None:
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise BackendError("invalid_input", f"cannot read params file: {exc}", EXIT_INVALID_INPUT) from exc
    if not isinstance(data, dict):
        raise BackendError("invalid_input", "params file must contain a JSON object", EXIT_INVALID_INPUT)
    return data
