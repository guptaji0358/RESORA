"""Types a command module is written against: `Command` (what it exports) and
`Context` (what the engine hands it). No behaviour, no imports of commands."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from typing import Any, Callable

from .protocol import CancelToken, Emitter


@dataclass(frozen=True)
class Context:
    """Everything a command may use to talk to the outside world."""

    emit: Emitter
    cancel: CancelToken
    params: dict[str, Any]


@dataclass(frozen=True)
class Command:
    name: str
    help: str
    run: Callable[[argparse.Namespace, Context], Any]  # returns the result `data`
    add_arguments: Callable[[argparse.ArgumentParser], None] | None = None
    hidden: bool = False  # diagnostic commands not meant for the GUI
