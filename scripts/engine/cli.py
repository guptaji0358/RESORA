"""Command-line parsing: global flags plus one sub-parser per registered command."""
from __future__ import annotations

import argparse
from pathlib import Path
from typing import NoReturn

from . import __version__
from .command import Command


class UsageError(Exception):
    pass


class _Parser(argparse.ArgumentParser):
    def error(self, message: str) -> NoReturn:
        raise UsageError(message)


def build_parser(registry: dict[str, Command]) -> argparse.ArgumentParser:
    parser = _Parser(prog="scripts.engine")
    parser.add_argument("--version", action="version", version=__version__)
    parser.add_argument("--params-file", type=Path, help="JSON object with structured input for the command")
    parser.add_argument("--cancel-on-stdin", action="store_true",
                        help="cancel on a `cancel` line or EOF on stdin (set by the GUI)")
    sub = parser.add_subparsers(dest="command", parser_class=_Parser)
    for command in registry.values():
        sp = sub.add_parser(command.name, help=None if command.hidden else command.help)
        if command.add_arguments:
            command.add_arguments(sp)
    return parser
